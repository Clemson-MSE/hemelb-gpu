
// This file is part of HemeLB and is Copyright (C)
// the HemeLB team and/or their institutions, as detailed in the
// file AUTHORS. This software is provided under the terms of the
// license in the file LICENSE.

#include <cmath>
#include <list>
#include <map>
#include <algorithm>
#include <zlib.h>

#include "debug/Debugger.h"
#include "io/formats/geometry.h"
#include "io/writers/xdr/XdrMemReader.h"
#include "geometry/decomposition/BasicDecomposition.h"
#include "geometry/decomposition/OptimisedDecomposition.h"
#include "geometry/GeometryReader.h"
#include "lb/lattices/D3Q27.h"
#include "net/net.h"
#include "net/IOCommunicator.h"
#include "logging/Logger.h"
#include "util/UtilityFunctions.h"
#include "constants.h"
namespace hemelb
{
  namespace geometry
  {

    GeometryReader::GeometryReader(const bool reserveSteeringCore,
                                   const lb::lattices::LatticeInfo& latticeInfo,
                                   reporting::Timers &atimings, const net::IOCommunicator& ioComm) :
      latticeInfo(latticeInfo), hemeLbComms(ioComm), timings(atimings)
    {
      // This rank should participate in the domain decomposition if
      //  - there's no steering core (then all ranks are involved)
      //  - we're not on core 0 (the only core that might ever not participate)
      //  - there's only one processor (so core 0 has to participate)

      // Create our own group, without the root node if we're not running with it.
      if (reserveSteeringCore && ioComm.Size() > 1)
      {
        participateInTopology = !ioComm.OnIORank();

        std::vector<int> lExclusions(1);
        lExclusions[0] = 0;
        net::MpiGroup computeGroup = hemeLbComms.Group().Exclude(lExclusions);
        // Create a communicator just for the domain decomposition.
        computeComms = ioComm.Create(computeGroup);
        // Note that on the steering core, this is a null communicator.
      }
      else
      {
        participateInTopology = true;
        computeComms = ioComm;
      }

    }

    GeometryReader::~GeometryReader()
    {
    }

    Geometry GeometryReader::LoadAndDecompose(const std::string& dataFilePath)
    {
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Starting file read timer");
      timings[hemelb::reporting::Timers::fileRead].Start();

      // Create hints about how we'll read the file. See Chapter 13, page 400 of the MPI 2.2 spec.
      MPI_Info fileInfo;
      HEMELB_MPI_CALL(MPI_Info_create, (&fileInfo));
      std::string accessStyle = "access_style";
      std::string accessStyleValue = "sequential";
      std::string buffering = "collective_buffering";
      std::string bufferingValue = "true";

      HEMELB_MPI_CALL(MPI_Info_set, (fileInfo,
          const_cast<char*> (accessStyle.c_str()),
          const_cast<char*> (accessStyleValue.c_str()))
      );
      HEMELB_MPI_CALL(MPI_Info_set, (fileInfo,
          const_cast<char*> (buffering.c_str()),
          const_cast<char*> (bufferingValue.c_str()))
      );

      // Open the file.
      file = net::MpiFile::Open(hemeLbComms, dataFilePath, MPI_MODE_RDONLY, fileInfo);
      logging::Logger::Log<logging::Info, logging::OnePerCore>("Opened config file %s", dataFilePath.c_str());
      // TODO: Why is there this fflush?
      fflush( NULL);

      // Set the view to the file.
      file.SetView(0, MPI_CHAR, MPI_CHAR, "native", fileInfo);

      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Reading file preamble");
      Geometry geometry = ReadPreamble();

      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Reading file header");
      ReadHeader(geometry.GetBlockCount());

      // Close the file - only the ranks participating in the topology need to read it again.
      file.Close();

      timings[hemelb::reporting::Timers::initialDecomposition].Start();
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Beginning initial decomposition");
      principalProcForEachBlock.resize(geometry.GetBlockCount());

      if (!participateInTopology)
      {
        // If we are the steering core, mark them all as unknown.
        for (site_t block = 0; block < geometry.GetBlockCount(); ++block)
        {
          principalProcForEachBlock[block] = -1;
        }
      }
      else
      {
        // Get an initial base-level decomposition of the domain macro-blocks over processors.
        // This will later be improved upon by ParMetis.
        decomposition::BasicDecomposition basicDecomposer(geometry,
                                                          latticeInfo,
                                                          computeComms,
                                                          fluidSitesOnEachBlock);
        basicDecomposer.Decompose(principalProcForEachBlock);

        if (ShouldValidate())
        {
          basicDecomposer.Validate(principalProcForEachBlock);
        }
      }
      timings[hemelb::reporting::Timers::initialDecomposition].Stop();
      // Perform the initial read-in.
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Reading in my blocks");

      if (participateInTopology)
      {
        // Reopen in the file just between the nodes in the topology decomposition. Read in blocks
        // local to this node.
        file = net::MpiFile::Open(computeComms, dataFilePath, MPI_MODE_RDONLY, fileInfo);

        ReadInBlocksWithHalo(geometry, principalProcForEachBlock, computeComms.Rank());

        if (ShouldValidate())
        {
          ValidateGeometry(geometry);
        }
      }

      timings[hemelb::reporting::Timers::fileRead].Stop();

      hemelb::logging::Logger::Log<hemelb::logging::Debug, hemelb::logging::Singleton>("Begin optimising the domain decomposition.");
      timings[hemelb::reporting::Timers::domainDecomposition].Start();

      // Having done an initial decomposition of the geometry, and read in the data, we optimise the
      // domain decomposition.
      if (participateInTopology)
      {
        logging::Logger::Log<logging::Debug, logging::OnePerCore>("Beginning domain decomposition optimisation");
        OptimiseDomainDecomposition(geometry, principalProcForEachBlock);
        logging::Logger::Log<logging::Debug, logging::OnePerCore>("Ending domain decomposition optimisation");

        if (ShouldValidate())
        {
          ValidateGeometry(geometry);
        }
        file.Close();
      }

      // Finish up - close the file, set the timings, deallocate memory.
      HEMELB_MPI_CALL(MPI_Info_free, (&fileInfo));

      timings[hemelb::reporting::Timers::domainDecomposition].Stop();

      return geometry;
    }

    std::vector<char> GeometryReader::ReadOnAllTasks(unsigned nBytes)
    {
      std::vector<char> buffer(nBytes);
      const net::MpiCommunicator& comm = file.GetCommunicator();
      if (comm.Rank() == HEADER_READING_RANK)
      {
        file.Read(buffer);
      }
      comm.Broadcast(buffer, HEADER_READING_RANK);
      return buffer;
    }

    /**
     * Read in the section at the beginning of the config file.
     */
    Geometry GeometryReader::ReadPreamble()
    {
      const unsigned preambleBytes = io::formats::geometry::PreambleLength;
      std::vector<char> preambleBuffer = ReadOnAllTasks(preambleBytes);

      // Create an Xdr translator based on the read-in data.
      io::writers::xdr::XdrReader preambleReader = io::writers::xdr::XdrMemReader(&preambleBuffer[0],
                                                                                  preambleBytes);

      unsigned hlbMagicNumber, gmyMagicNumber, version;
      // Read in housekeeping values
      preambleReader.readUnsignedInt(hlbMagicNumber);
      preambleReader.readUnsignedInt(gmyMagicNumber);
      preambleReader.readUnsignedInt(version);

      // Check the value of the HemeLB magic number.
      if (hlbMagicNumber != io::formats::HemeLbMagicNumber)
      {
        throw Exception() << "This file does not start with the HemeLB magic number."
            << " Expected: " << unsigned(io::formats::HemeLbMagicNumber)
            << " Actual: " << hlbMagicNumber;
      }

      // Check the value of the geometry file magic number.
      if (gmyMagicNumber != io::formats::geometry::MagicNumber)
      {
        throw Exception() << "This file does not have the geometry magic number."
            << " Expected: " << unsigned(io::formats::geometry::MagicNumber)
            << " Actual: " << gmyMagicNumber;
      }

      if (version != io::formats::geometry::VersionNumber)
      {
        throw Exception() << "Version number incorrect."
            << " Supported: " << unsigned(io::formats::geometry::VersionNumber)
            << " Input: " << version;
      }

      // Variables we'll read.
      // We use temporary vars here, as they must be the same size as the type in the file
      // regardless of the internal type used.
      unsigned int blocksX, blocksY, blocksZ, blockSize;
      double voxelSize;
      util::Vector3D<double> origin;

      // Read in the values.
      preambleReader.readUnsignedInt(blocksX);
      preambleReader.readUnsignedInt(blocksY);
      preambleReader.readUnsignedInt(blocksZ);
      preambleReader.readUnsignedInt(blockSize);
      preambleReader.readDouble(voxelSize);
      for (unsigned int i = 0; i < 3; ++i)
      {
        preambleReader.readDouble(origin[i]);
      }

      // Read the padding unsigned int.
      unsigned paddingValue;
      preambleReader.readUnsignedInt(paddingValue);

      return Geometry(util::Vector3D<site_t>(blocksX, blocksY, blocksZ),
                      blockSize);
    }

    /**
     * Read the header section, with minimal information about each block.
     *
     * Results are placed in the member arrays fluidSitesPerBlock,
     * bytesPerCompressedBlock and bytesPerUncompressedBlock.
     */
    void GeometryReader::ReadHeader(site_t blockCount)
    {
      site_t headerByteCount = GetHeaderLength(blockCount);
      std::vector<char> headerBuffer = ReadOnAllTasks(headerByteCount);

      // Create a Xdr translation object to translate from binary
      hemelb::io::writers::xdr::XdrReader preambleReader =
          hemelb::io::writers::xdr::XdrMemReader(&headerBuffer[0], (unsigned int) headerByteCount);

      // Read in all the data.
      for (site_t block = 0; block < blockCount; block++)
      {
        unsigned int sites, bytes, uncompressedBytes;
        preambleReader.readUnsignedInt(sites);
        preambleReader.readUnsignedInt(bytes);
        preambleReader.readUnsignedInt(uncompressedBytes);

        fluidSitesOnEachBlock.push_back(sites);
        bytesPerCompressedBlock.push_back(bytes);
        bytesPerUncompressedBlock.push_back(uncompressedBytes);
      }
    }

    /**
     * Read in the necessary blocks from the file.
     */
    void GeometryReader::ReadInBlocksWithHalo(Geometry& geometry,
                                              const std::vector<proc_t>& unitForEachBlock,
                                              const proc_t localRank)
    {
      // Create a list of which blocks to read in.
      timings[hemelb::reporting::Timers::readBlocksPrelim].Start();

      // Populate the list of blocks to read (including a halo one block wide around all
      // local blocks).
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Determining blocks to read");
      std::vector<bool> readBlock = DecideWhichBlocksToReadIncludingHalo(geometry,
                                                                         unitForEachBlock,
                                                                         localRank);

      if (ShouldValidate())
      {
        logging::Logger::Log<logging::Debug, logging::OnePerCore>("Validating block sizes");

        // Validate the uncompressed length of the block on disk fits out expectations.
        for (site_t block = 0; block < geometry.GetBlockCount(); ++block)
        {
          if (bytesPerUncompressedBlock[block]
              > io::formats::geometry::GetMaxBlockRecordLength(geometry.GetBlockSize(),
                                                               fluidSitesOnEachBlock[block]))
          {
            logging::Logger::Log<logging::Critical, logging::OnePerCore>("Block %i is %i bytes when the longest possible block should be %i bytes",
                                                             block,
                                                             bytesPerUncompressedBlock[block],
                                                             io::formats::geometry::GetMaxBlockRecordLength(geometry.GetBlockSize(),
                                                                                                            fluidSitesOnEachBlock[block]));
          }
        }
      }

      // Next we spread round the lists of which blocks each core needs access to.
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Informing reading cores of block needs");
      net::Net net = net::Net(computeComms);
      Needs needs(geometry.GetBlockCount(),
                  readBlock,
                  util::NumericalFunctions::min(READING_GROUP_SIZE, computeComms.Size()),
                  net,
                  ShouldValidate());

      timings[hemelb::reporting::Timers::readBlocksPrelim].Stop();
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Reading blocks");
      timings[hemelb::reporting::Timers::readBlocksAll].Start();

      // Set the initial offset to the first block, which will be updated as we progress
      // through the blocks.
      MPI_Offset offset = io::formats::geometry::PreambleLength
          + GetHeaderLength(geometry.GetBlockCount());

      // Iterate over each block.
      for (site_t nextBlockToRead = 0; nextBlockToRead < geometry.GetBlockCount(); ++nextBlockToRead)
      {
        // Read in the block on all cores (nothing will be done if this core doesn't need the block).
        ReadInBlock(offset,
                    geometry,
                    needs.ProcessorsNeedingBlock(nextBlockToRead),
                    nextBlockToRead,
                    readBlock[nextBlockToRead]);

        // Update the offset to be ready for the next block.
        offset += bytesPerCompressedBlock[nextBlockToRead];
      }

      timings[hemelb::reporting::Timers::readBlocksAll].Stop();
    }

    void GeometryReader::ReadInBlock(MPI_Offset offsetSoFar, Geometry& geometry,
                                     const std::vector<proc_t>& procsWantingThisBlock,
                                     const site_t blockNumber, const bool neededOnThisRank)
    {
      // Easy case if there are no sites on the block.
      if (fluidSitesOnEachBlock[blockNumber] <= 0)
      {
        return;
      }
      std::vector<char> compressedBlockData;
      proc_t readingCore = GetReadingCoreForBlock(blockNumber);

      net::Net net = net::Net(computeComms);

      if (readingCore == computeComms.Rank())
      {
        timings[hemelb::reporting::Timers::readBlock].Start();
        // Read the data.
        compressedBlockData.resize(bytesPerCompressedBlock[blockNumber]);
        file.ReadAt(offsetSoFar, compressedBlockData);

        // Spread it.
        for (std::vector<proc_t>::const_iterator receiver = procsWantingThisBlock.begin(); receiver
            != procsWantingThisBlock.end(); receiver++)
        {
          if (*receiver != computeComms.Rank())
          {

            net.RequestSendV(compressedBlockData, *receiver);
          }
        }
        timings[hemelb::reporting::Timers::readBlock].Stop();
      }
      else if (neededOnThisRank)
      {
        compressedBlockData.resize(bytesPerCompressedBlock[blockNumber]);

        net.RequestReceiveV(compressedBlockData, readingCore);

      }
      else
      {
        return;
      }
      timings[hemelb::reporting::Timers::readNet].Start();
      net.Dispatch();
      timings[hemelb::reporting::Timers::readNet].Stop();
      timings[hemelb::reporting::Timers::readParse].Start();
      if (neededOnThisRank)
      {
        // Create an Xdr interpreter.
        std::vector<char> blockData = DecompressBlockData(compressedBlockData,
                                                          bytesPerUncompressedBlock[blockNumber]);
        io::writers::xdr::XdrMemReader lReader(&blockData.front(), blockData.size());

        ParseBlock(geometry, blockNumber, lReader);

        // If debug-level logging, check that we've read in as many sites as anticipated.
        if (ShouldValidate())
        {
          // Count the sites read,
          site_t numSitesRead = 0;
          for (site_t site = 0; site < geometry.GetSitesPerBlock(); ++site)
          {
            if (geometry.Blocks[blockNumber].Sites[site].targetProcessor != SITE_OR_BLOCK_SOLID)
            {
              ++numSitesRead;
            }
          }
          // Compare with the sites we expected to read.
          if (numSitesRead != fluidSitesOnEachBlock[blockNumber])
          {
            logging::Logger::Log<logging::Error, logging::OnePerCore>("Was expecting %i fluid sites on block %i but actually read %i",
                                                          fluidSitesOnEachBlock[blockNumber],
                                                          blockNumber,
                                                          numSitesRead);
          }
        }
      }
      else if (!geometry.Blocks[blockNumber].Sites.empty())
      {
        geometry.Blocks[blockNumber].Sites = std::vector<GeometrySite>(0, GeometrySite(false));
      }
      timings[hemelb::reporting::Timers::readParse].Stop();
    }

    std::vector<char> GeometryReader::DecompressBlockData(const std::vector<char>& compressed,
                                                          const unsigned int uncompressedBytes)
    {
      timings[hemelb::reporting::Timers::unzip].Start();
      // For zlib return codes.
      int ret;

      // Set up the buffer for decompressed data. We know how long the the data is
      std::vector<char> uncompressed(uncompressedBytes);

      // Set up the inflator
      z_stream stream;
      stream.zalloc = Z_NULL;
      stream.zfree = Z_NULL;
      stream.opaque = Z_NULL;
      stream.avail_in = compressed.size();
      stream.next_in = reinterpret_cast<unsigned char*> (const_cast<char*> (&compressed.front()));

      ret = inflateInit(&stream);
      if (ret != Z_OK)
        throw Exception() << "Decompression error for block";

      stream.avail_out = uncompressed.size();
      stream.next_out = reinterpret_cast<unsigned char*> (&uncompressed.front());

      ret = inflate(&stream, Z_FINISH);
      if (ret != Z_STREAM_END)
        throw Exception() << "Decompression error for block";

      uncompressed.resize(uncompressed.size() - stream.avail_out);
      ret = inflateEnd(&stream);
      if (ret != Z_OK)
        throw Exception() << "Decompression error for block";

      timings[hemelb::reporting::Timers::unzip].Stop();
      return uncompressed;
    }

    void GeometryReader::ParseBlock(Geometry& geometry, const site_t block,
                                    io::writers::xdr::XdrReader& reader)
    {
      // We start by clearing the sites on the block. We read the blocks twice (once before
      // optimisation and once after), so there can be sites on the block from the previous read.
      geometry.Blocks[block].Sites.clear();

      for (site_t localSiteIndex = 0; localSiteIndex < geometry.GetSitesPerBlock(); ++localSiteIndex)
      {
        geometry.Blocks[block].Sites.push_back(ParseSite(reader));
      }
    }

    GeometrySite GeometryReader::ParseSite(io::writers::xdr::XdrReader& reader)
    {
      // Read the fluid property.
      unsigned isFluid;
      bool success = reader.readUnsignedInt(isFluid);

      if (!success)
      {
        logging::Logger::Log<logging::Error, logging::OnePerCore>("Error reading site type");
      }

      /// @todo #598 use constant in hemelb::io::formats::geometry
      GeometrySite readInSite(isFluid != 0);

      // If solid, there's nothing more to do.
      if (!readInSite.isFluid)
      {
        return readInSite;
      }

      const io::formats::geometry::DisplacementVector& neighbourhood =
          io::formats::geometry::Get().GetNeighbourhood();
      // Prepare the links array to have enough space.
      readInSite.links.resize(latticeInfo.GetNumVectors() - 1);

      bool isGmyWallSite = false;

      // For each link direction...
      for (Direction readDirection = 0; readDirection < neighbourhood.size(); readDirection++)
      {
        // read the type of the intersection and create a link...
        unsigned intersectionType;
        reader.readUnsignedInt(intersectionType);

        GeometrySiteLink link;
        link.type = (GeometrySiteLink::IntersectionType) intersectionType;

        // walls have a floating-point distance to the wall...
        if (link.type == GeometrySiteLink::WALL_INTERSECTION)
        {
          isGmyWallSite = true;
          float distance;
          reader.readFloat(distance);
          link.distanceToIntersection = distance;
        }
        // inlets and outlets (which together with none make up the other intersection types)
        // have an iolet id and a distance float...
        else if (link.type != GeometrySiteLink::NO_INTERSECTION)
        {
          float distance;
          unsigned ioletId;
          reader.readUnsignedInt(ioletId);
          reader.readFloat(distance);

          link.ioletId = ioletId;
          link.distanceToIntersection = distance;
        }

        // Now, attempt to match the direction read from the local neighbourhood to one in the
        // lattice being used for simulation. If a match is found, assign the link to the read
        // site.
        for (Direction usedLatticeDirection = 1; usedLatticeDirection < latticeInfo.GetNumVectors(); usedLatticeDirection++)
        {
          if (latticeInfo.GetVector(usedLatticeDirection) == neighbourhood[readDirection])
          {
            // If this link direction is necessary to the lattice in use, keep the link data.
            readInSite.links[usedLatticeDirection - 1] = link;
            break;
          }
        }
      }

      unsigned normalAvailable;
      reader.readUnsignedInt(normalAvailable);
      readInSite.wallNormalAvailable = (normalAvailable
          == io::formats::geometry::WALL_NORMAL_AVAILABLE);

      if (readInSite.wallNormalAvailable != isGmyWallSite)
      {
        std::string msg = isGmyWallSite
          ? "wall fluid site without"
          : "bulk fluid site with";
        throw Exception() << "Malformed GMY file, "
            << msg << " a defined wall normal currently not allowed.";
      }

      if (readInSite.wallNormalAvailable)
      {
        reader.readFloat(readInSite.wallNormal[0]);
        reader.readFloat(readInSite.wallNormal[1]);
        reader.readFloat(readInSite.wallNormal[2]);
      }

      return readInSite;
    }

    proc_t GeometryReader::GetReadingCoreForBlock(site_t blockNumber)
    {
      return proc_t(blockNumber % util::NumericalFunctions::min(READING_GROUP_SIZE,
                                                                computeComms.Size()));
    }

    /**
     * This function is only called if in geometry-validation mode.
     * @param geometry
     */
    void GeometryReader::ValidateGeometry(const Geometry& geometry)
    {
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Validating the GlobalLatticeData");

      // We check the isFluid property and the link type for each direction

      std::vector<proc_t> myProcForSite;
      std::vector<unsigned> dummySiteData;

      // We also validate that each processor has the same beliefs about each site.
      for (site_t block = 0; block < geometry.GetBlockCount(); ++block)
      {
        // Clear vectors
        myProcForSite.clear();
        dummySiteData.clear();

        if (geometry.Blocks[block].Sites.size() == 0)
        {
          for (site_t localSite = 0; localSite < geometry.GetSitesPerBlock(); ++localSite)
          {
            myProcForSite.push_back(SITE_OR_BLOCK_SOLID);
            dummySiteData.push_back(std::numeric_limits<unsigned>::max());
            for (Direction direction = 1; direction < latticeInfo.GetNumVectors(); ++direction)
            {
              dummySiteData.push_back(std::numeric_limits<unsigned>::max());
            }
          }
        }
        else
        {
          for (site_t localSite = 0; localSite < geometry.GetSitesPerBlock(); ++localSite)
          {
            myProcForSite.push_back(geometry.Blocks[block].Sites[localSite].targetProcessor);

            dummySiteData.push_back(geometry.Blocks[block].Sites[localSite].isFluid);

            for (Direction direction = 1; direction < latticeInfo.GetNumVectors(); ++direction)
            {
              if (geometry.Blocks[block].Sites[localSite].isFluid)
              {
                dummySiteData.push_back(geometry.Blocks[block].Sites[localSite].links[direction - 1].type);
              }
              else
              {
                dummySiteData.push_back(std::numeric_limits<unsigned>::max());
              }
            }
          }
        }

        // Reduce using a minimum to find the actual processor for each site (ignoring the
        // invalid entries).
        std::vector<proc_t> procForSiteRecv = computeComms.AllReduce(myProcForSite, MPI_MIN);
        std::vector<unsigned> siteDataRecv = computeComms.AllReduce(dummySiteData, MPI_MIN);

        for (site_t site = 0; site < geometry.GetSitesPerBlock(); ++site)
        {
          if (procForSiteRecv[site] == ConvertTopologyRankToGlobalRank(computeComms.Rank())
              && (myProcForSite[site] != ConvertTopologyRankToGlobalRank(computeComms.Rank())))
          {
            logging::Logger::Log<logging::Critical, logging::OnePerCore>("Other cores think this core has site %li on block %li but it disagrees.",
                                                             site,
                                                             block);
          }
          else if (myProcForSite[site] != SITE_OR_BLOCK_SOLID && procForSiteRecv[site]
              != myProcForSite[site])
          {
            logging::Logger::Log<logging::Critical, logging::OnePerCore>("This core thought that core %li has site %li on block %li but others think it's on core %li.",
                                                             myProcForSite[site],
                                                             site,
                                                             block,
                                                             procForSiteRecv[site]);
          }

          if (geometry.Blocks[block].Sites.size() > 0)
          {
            if (dummySiteData[site * latticeInfo.GetNumVectors()] != siteDataRecv[site
                * latticeInfo.GetNumVectors()])
            {
              logging::Logger::Log<logging::Critical, logging::OnePerCore>("Different fluid state was found for site %li on block %li. One: %li, Two: %li .",
                                                               site,
                                                               block,
                                                               dummySiteData[site
                                                                   * latticeInfo.GetNumVectors()],
                                                               siteDataRecv[site
                                                                   * latticeInfo.GetNumVectors()]);
            }

            for (Direction direction = 1; direction < latticeInfo.GetNumVectors(); ++direction)
            {
              if (dummySiteData[site * latticeInfo.GetNumVectors() + direction]
                  != siteDataRecv[site * latticeInfo.GetNumVectors() + direction])
              {
                logging::Logger::Log<logging::Critical, logging::OnePerCore>("Different link type was found for site %li, link %i on block %li. One: %li, Two: %li .",
                                                                 site,
                                                                 direction,
                                                                 block,
                                                                 dummySiteData[site
                                                                     * latticeInfo.GetNumVectors()
                                                                     + direction],
                                                                 siteDataRecv[site
                                                                     * latticeInfo.GetNumVectors()
                                                                     + direction]);
              }
            }

          }
        }
      }
    }

    std::vector<bool> GeometryReader::DecideWhichBlocksToReadIncludingHalo(
                                                                           const Geometry& geometry,
                                                                           const std::vector<proc_t>& unitForEachBlock,
                                                                           proc_t localRank)
    {
      std::vector<bool> shouldReadBlock(geometry.GetBlockCount(), false);

      // Read a block in if it has fluid sites and is to live on the current processor. Also read
      // in any neighbours with fluid sites.
      for (site_t blockI = 0; blockI < geometry.GetBlockDimensions().x; ++blockI)
      {
        for (site_t blockJ = 0; blockJ < geometry.GetBlockDimensions().y; ++blockJ)
        {
          for (site_t blockK = 0; blockK < geometry.GetBlockDimensions().z; ++blockK)
          {
            site_t lBlockId = geometry.GetBlockIdFromBlockCoordinates(blockI, blockJ, blockK);

            if (unitForEachBlock[lBlockId] != localRank)
            {
              continue;
            }

            // Read in all neighbouring blocks.
            for (site_t neighI = util::NumericalFunctions::max<site_t>(0, blockI - 1); (neighI
                <= (blockI + 1)) && (neighI < geometry.GetBlockDimensions().x); ++neighI)
            {
              for (site_t neighJ = util::NumericalFunctions::max<site_t>(0, blockJ - 1); (neighJ
                  <= (blockJ + 1)) && (neighJ < geometry.GetBlockDimensions().y); ++neighJ)
              {
                for (site_t neighK = util::NumericalFunctions::max<site_t>(0, blockK - 1); (neighK
                    <= (blockK + 1)) && (neighK < geometry.GetBlockDimensions().z); ++neighK)
                {
                  site_t lNeighId = geometry.GetBlockIdFromBlockCoordinates(neighI, neighJ, neighK);

                  shouldReadBlock[lNeighId] = true;
                }
              }
            }
          }
        }
      }

      return shouldReadBlock;
    }

    void GeometryReader::OptimiseDomainDecomposition(Geometry& geometry,
                                                     const std::vector<proc_t>& procForEachBlock)
    {
      decomposition::OptimisedDecomposition optimiser(timings,
                                                      computeComms,
                                                      geometry,
                                                      latticeInfo,
                                                      procForEachBlock,
                                                      fluidSitesOnEachBlock);

      timings[hemelb::reporting::Timers::reRead].Start();
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Rereading blocks");
      // Reread the blocks based on the ParMetis decomposition.
      RereadBlocks(geometry,
                   optimiser.GetMovesCountPerCore(),
                   optimiser.GetMovesList(),
                   procForEachBlock);
      timings[hemelb::reporting::Timers::reRead].Stop();

      timings[hemelb::reporting::Timers::moves].Start();
      // Implement the decomposition now that we have read the necessary data.
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Implementing moves");
      ImplementMoves(geometry,
                     procForEachBlock,
                     optimiser.GetMovesCountPerCore(),
                     optimiser.GetMovesList());
      timings[hemelb::reporting::Timers::moves].Stop();
    }

    // The header section of the config file contains a number of records.
    site_t GeometryReader::GetHeaderLength(site_t blockCount) const
    {
      return io::formats::geometry::HeaderRecordLength * blockCount;
    }

    void GeometryReader::RereadBlocks(Geometry& geometry, const std::vector<idx_t>& movesPerProc,
                                      const std::vector<idx_t>& movesList,
                                      const std::vector<int>& procForEachBlock)
    {
      // Initialise the array (of which proc each block belongs to) to what it was before.
      std::vector<int> newProcForEachBlock(geometry.GetBlockCount());

      for (site_t blockNumber = 0; blockNumber < geometry.GetBlockCount(); ++blockNumber)
      {
        newProcForEachBlock[blockNumber] = procForEachBlock[blockNumber];
      }

      // Set the proc for each block to be the current proc whenever a site on that block is
      // going to be moved to the current proc.
      idx_t moveIndex = 0;

      for (proc_t fromProc = 0; fromProc < computeComms.Size(); ++fromProc)
      {
        for (idx_t moveNumber = 0; moveNumber < movesPerProc[fromProc]; ++moveNumber)
        {
          idx_t block = movesList[3 * moveIndex];
          idx_t toProc = movesList[3 * moveIndex + 2];
          ++moveIndex;

          if (toProc == (idx_t) computeComms.Rank())
          {
            newProcForEachBlock[block] = computeComms.Rank();
          }
        }
      }

      // Reread the blocks into the GlobalLatticeData now.
      ReadInBlocksWithHalo(geometry, newProcForEachBlock, computeComms.Rank());
    }

    void GeometryReader::ImplementMoves(Geometry& geometry,
                                        const std::vector<proc_t>& procForEachBlock,
                                        const std::vector<idx_t>& movesFromEachProc,
                                        const std::vector<idx_t>& movesList) const
    {
      // First all, set the proc rank for each site to what it originally was before
      // domain decomposition optimisation. Go through each block...
      for (site_t block = 0; block < geometry.GetBlockCount(); ++block)
      {
        // If this proc has owned a fluid site on this block either before or after optimisation,
        // the following will be non-null.
        if (geometry.Blocks[block].Sites.size() > 0)
        {
          // Get the original proc for that block.
          proc_t originalProc = procForEachBlock[block];

          // For each site on that block...
          for (site_t siteIndex = 0; siteIndex < geometry.GetSitesPerBlock(); ++siteIndex)
          {
            // ... if the site is non-solid...
            if (geometry.Blocks[block].Sites[siteIndex].targetProcessor != SITE_OR_BLOCK_SOLID)
            {
              // ... set its rank to be the rank it had before optimisation.
              geometry.Blocks[block].Sites[siteIndex].targetProcessor
                  = ConvertTopologyRankToGlobalRank(originalProc);
            }
          }
        }
      }

      // Now implement the moves suggested by parmetis.
      idx_t moveIndex = 0;

      // For each source proc, go through as many moves as it had.
      for (proc_t fromProc = 0; fromProc < computeComms.Size(); ++fromProc)
      {
        for (idx_t moveNumber = 0; moveNumber < movesFromEachProc[fromProc]; ++moveNumber)
        {
          // For each move, get the block, site and destination proc.
          idx_t block = movesList[3 * moveIndex];
          idx_t site = movesList[3 * moveIndex + 1];
          idx_t toProc = movesList[3 * moveIndex + 2];

          // Only implement the move if we have read that block's data.
          if (geometry.Blocks[block].Sites.size() > 0)
          {
            // Some logging code - the unmodified rank for each move's site should equal
            // lFromProc.
            if (ShouldValidate())
            {
              if (geometry.Blocks[block].Sites[site].targetProcessor
                  != ConvertTopologyRankToGlobalRank((proc_t) fromProc))
              {
                logging::Logger::Log<logging::Error, logging::OnePerCore>("Block %ld, site %ld from move %u was originally on proc %i, not proc %u.",
                                                              block,
                                                              site,
                                                              moveIndex,
                                                              geometry.Blocks[block].Sites[site].targetProcessor,
                                                              fromProc);
              }
            }

            // Implement the move.
            geometry.Blocks[block].Sites[site].targetProcessor
                = ConvertTopologyRankToGlobalRank((proc_t) toProc);
          }

          ++moveIndex;
        }
      }
    }

    proc_t GeometryReader::ConvertTopologyRankToGlobalRank(proc_t topologyRankIn) const
    {
      // If the global rank is not equal to the topology rank, we are not using rank 0 for
      // LBM.
      return (hemeLbComms.Rank() == computeComms.Rank())
        ? topologyRankIn
        : (topologyRankIn + 1);
    }

    bool GeometryReader::ShouldValidate() const
    {
#ifdef HEMELB_VALIDATE_GEOMETRY
      return true;
#else
      return false;
#endif
    }
  }
}
