
// This file is part of HemeLB and is Copyright (C)
// the HemeLB team and/or their institutions, as detailed in the
// file AUTHORS. This software is provided under the terms of the
// license in the file LICENSE.

#include "colloids/ColloidController.h"
#include "geometry/BlockTraverser.h"
#include "geometry/SiteTraverser.h"
#include "logging/Logger.h"

namespace hemelb
{
  namespace colloids
  {
    void ColloidController::OutputInformation(const LatticeTimeStep timestep) const
    {
      timers[reporting::Timers::colloidOutput].Start();
      particleSet->OutputInformation(timestep);
      timers[reporting::Timers::colloidOutput].Stop();
    }

    // destructor
    ColloidController::~ColloidController()
    {
      delete particleSet;
    }

    // constructor - called by SimulationMaster::Initialise()
    ColloidController::ColloidController(const geometry::LatticeData& latDatLBM,
                                         const lb::SimulationState& simulationState,
                                         const geometry::Geometry& gmyResult,
                                         io::xml::Document& xml,
                                         lb::MacroscopicPropertyCache& propertyCache,
                                         const hemelb::lb::LbmParameters *lbmParams,
                                         const std::string& outputPath,
                                         const net::IOCommunicator& ioComms_,
                                         reporting::Timers& timers) :
      ioComms(ioComms_), simulationState(simulationState), timers(timers)
    {
      // The neighbourhood used here is different to the latticeInfo used to create latDatLBM
      // The portion of the geometry input file that was read in by this proc, i.e. gmyResult
      // contains more information than was used when creating the LB lattice, i.e. latDatLBM
      // so, we traverse mLatDat to find local fluid sites but then get neighbour information
      // from the geometry file using a neighbour lattice definition appropriate for colloids

      // get the description of the colloid neighbourhood (as a vector of Vector3D of site_t)
      const Neighbourhood neighbourhood = GetNeighbourhoodVectors(REGION_OF_INFLUENCE);

      // determine information about neighbour sites and processors for all local fluid sites
      InitialiseNeighbourList(latDatLBM, gmyResult, neighbourhood);

      bool allGood = ioComms.OnIORank() || (neighbourProcessors.size() > 0);
      logging::Logger::Log<logging::Debug, logging::OnePerCore>(
        "[Rank %i]: ColloidController - neighbourhood %i, neighbours %i, allGood %i\n",
        ioComms.Rank(), neighbourhood.size(), neighbourProcessors.size(), allGood);

      io::xml::Element particlesElem = xml.GetRoot().GetChildOrThrow("colloids").GetChildOrThrow("particles");
      particleSet = new ParticleSet(latDatLBM, particlesElem, propertyCache,
                                    lbmParams,
                                    neighbourProcessors, ioComms, outputPath);
    }

    void ColloidController::InitialiseNeighbourList(
            const geometry::LatticeData& latDatLBM,
            const geometry::Geometry& gmyResult,
            const Neighbourhood& neighbourhood)
    {
      // PLAN
      // foreach block in gmyResult (i.e. each block that may have been read from the input file)
      //   if block has sites (i.e. if this process _has_ read this block in from the input file)
      //     foreach site in block (i.e. each site, which may be local or remote, fluid or solid)
      //       if site is local (i.e. if the targetProcessor for this site is equal to localRank)
      //         foreach neighbour of site (i.e. follow each direction vector in the LatticeInfo)
      //           if neighbour is valid (i.e. neighbour site is within the geometry & not solid)
      //             if neighbour is remote (i.e. targetProcessor for neighbour is not localRank)
      //               if new neighbourRank (i.e. neighbourRank is not already in neighbourRanks)
      //                 add the targetProcessor of the neighbour site to our neighbourRanks list

      // foreach block in geometry
      for (geometry::BlockTraverser blockTraverser(latDatLBM);
           blockTraverser.CurrentLocationValid();
           blockTraverser.TraverseOne())
      {
        util::Vector3D<site_t> globalLocationForBlock =
              blockTraverser.GetCurrentLocation() * gmyResult.GetBlockSize();

        // if block has sites
        site_t blockId = blockTraverser.GetCurrentIndex();
        if (gmyResult.Blocks[blockId].Sites.size() == 0)
        {
          logging::Logger::Log<logging::Trace, logging::OnePerCore>(
            "ColloidController: block with id %i and coords (%i,%i,%i) is solid.\n",
            blockId,
            blockTraverser.GetCurrentLocation().x,
            blockTraverser.GetCurrentLocation().y,
            blockTraverser.GetCurrentLocation().z);
          continue;
        }

        // foreach site in block
        for (geometry::SiteTraverser siteTraverser = blockTraverser.GetSiteTraverser();
             siteTraverser.CurrentLocationValid();
             siteTraverser.TraverseOne())
        {
          util::Vector3D<site_t> globalLocationForSite =
                globalLocationForBlock + siteTraverser.GetCurrentLocation();

          // if site is local
          site_t siteId = siteTraverser.GetCurrentIndex();
          if (gmyResult.Blocks[blockId].Sites[siteId].targetProcessor != this->ioComms.Rank())
          {
            logging::Logger::Log<logging::Trace, logging::OnePerCore>(
              "ColloidController: site with id %i and coords (%i,%i,%i) has proc %i (non-local).\n",
              siteId,
              siteTraverser.GetCurrentLocation().x,
              siteTraverser.GetCurrentLocation().y,
              siteTraverser.GetCurrentLocation().z,
              gmyResult.Blocks[blockId].Sites[siteId].targetProcessor);
            continue;
          }

          logging::Logger::Log<logging::Trace, logging::OnePerCore>(
            "ColloidController: site with id %i and coords (%i,%i,%i) is local.\n",
            siteId,
            siteTraverser.GetCurrentLocation().x,
            siteTraverser.GetCurrentLocation().y,
            siteTraverser.GetCurrentLocation().z);

          // foreach neighbour of site
          for (Neighbourhood::const_iterator itDirectionVector = neighbourhood.begin();
               itDirectionVector != neighbourhood.end();
               itDirectionVector++)
          {
            util::Vector3D<site_t> globalLocationForNeighbourSite = globalLocationForSite +
                                                                    *itDirectionVector;

            // if neighbour is valid
            site_t neighbourBlockId, neighbourSiteId;
            proc_t neighbourRank;
            bool isValid = GetLocalInformationForGlobalSite(
                  gmyResult, globalLocationForNeighbourSite,
                  &neighbourBlockId, &neighbourSiteId, &neighbourRank);

            // if neighbour is remote
            if (!isValid || neighbourRank == this->ioComms.Rank())
              continue;

            // if new neighbourRank
            int addedAlready = std::count(neighbourProcessors.begin(),
                                          neighbourProcessors.end(),
                                          neighbourRank);
            if (addedAlready != 0)
              continue;

            // add the targetProcessor of the neighbour site to our neighbourRanks list
            this->neighbourProcessors.push_back(neighbourRank);

            // debug message so this neighbour list can be compared to the LatticeData one
            logging::Logger::Log<logging::Trace, logging::OnePerCore>(
                "ColloidController: added %i as neighbour for %i because site %i in block %i is neighbour to site %i in block %i in direction (%i,%i,%i)\n",
                (int)neighbourRank, (int)(this->ioComms.Rank()),
                (int)neighbourSiteId, (int)neighbourBlockId,
                (int)siteId, (int)blockId,
                (*itDirectionVector).x, (*itDirectionVector).y, (*itDirectionVector).z);

          } // end for itDirectionVector
        } // end for siteTraverser
      } // end for blockTraverser

    }

    //DJH// this function should probably be in geometry::ReadResult
    bool ColloidController::GetLocalInformationForGlobalSite(
                                      const geometry::Geometry& gmyResult,
                                      const util::Vector3D<site_t>& globalLocationForSite,
                                      site_t* blockIdForSite,
                                      site_t* localSiteIdForSite,
                                      proc_t* ownerRankForSite)
    {
      // obtain block information (3D location vector and 1D id number) for the site
      util::Vector3D<site_t> blockLocationForSite = globalLocationForSite / gmyResult.GetBlockSize();
      // check for global location being outside the simulation entirely
      if (!gmyResult.AreBlockCoordinatesValid(blockLocationForSite))
        return false;

      *blockIdForSite = gmyResult.GetBlockIdFromBlockCoordinates(blockLocationForSite.x,
                                                                 blockLocationForSite.y,
                                                                 blockLocationForSite.z);

      // if the block does not contain any sites then return invalid
      if (gmyResult.Blocks[*blockIdForSite].Sites.empty())
        return false;

      // obtain site information (3D location vector and 1D id number)
      // note: these are both local to the block that contains the site
      util::Vector3D<site_t> localSiteLocation = globalLocationForSite % gmyResult.GetBlockSize();

      if (!gmyResult.AreLocalSiteCoordinatesValid(localSiteLocation))
        return false;

      *localSiteIdForSite = gmyResult.GetSiteIdFromSiteCoordinates(localSiteLocation.x,
                                                                   localSiteLocation.y,
                                                                   localSiteLocation.z);

      // obtain the rank of the processor responsible for simulating the fluid at this site
      *ownerRankForSite = gmyResult.Blocks[*blockIdForSite].Sites[*localSiteIdForSite].targetProcessor;

      // site is solid not fluid so return invalid
      if (*ownerRankForSite == SITE_OR_BLOCK_SOLID)
        return false;

      // all requested information obtained and validated so return true
      return true;
    }

    // generate a vector of Vector3D objects that describe the neighbourhood
    // produces a relative vector two all sites within distance site units in all 3 directions
    // examples: if distance==1 then the vectors will describe D3Q27 lattice pattern
    //           if distance==2 then the vectors will describe a 5x5 cube pattern
    const ColloidController::Neighbourhood ColloidController::GetNeighbourhoodVectors(site_t distance)
    {
      Neighbourhood vectors;

      for (site_t xAdj = -distance; xAdj <= distance; xAdj++)
        for (site_t yAdj = -distance; yAdj <= distance; yAdj++)
          for (site_t zAdj = -distance; zAdj <= distance; zAdj++)
          {
            vectors.push_back(util::Vector3D<site_t>(xAdj, yAdj, zAdj));
          }

      return vectors;
    }

    void ColloidController::RequestComms()
    {
      // communication from step 2
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Communicating colloid particle positions");
      timers[reporting::Timers::colloidCommunicatePositions].Start();
      particleSet->CommunicateParticlePositions();
      timers[reporting::Timers::colloidCommunicatePositions].Stop();

      timers[reporting::Timers::colloidCalculateForces].Start();
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Calculating colloid body forces");
      // step 3
      particleSet->CalculateBodyForces();

      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Calculating feedback forces for colloids");
      // steps 1 & 4 combined
      particleSet->CalculateFeedbackForces();
      timers[reporting::Timers::colloidCalculateForces].Stop();
      // steps 5 and 8 performed by LBM actor
    }

    void ColloidController::EndIteration()
    {
      const LatticeTimeStep currentTimestep = simulationState.GetTimeStep();

      // step 6
      timers[reporting::Timers::colloidUpdateCalculations].Start();
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Interpolating fluid velocities for colloids");
      particleSet->InterpolateFluidVelocity();
      timers[reporting::Timers::colloidUpdateCalculations].Stop();

      // communication from step 6
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Communicating fluid velocities for colloids");
      timers[reporting::Timers::colloidCommunicateVelocities].Start();
      particleSet->CommunicateFluidVelocities();
      timers[reporting::Timers::colloidCommunicateVelocities].Stop();

      // extra step (not in original design)
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Apply boundary conditions for colloids");
      timers[reporting::Timers::colloidUpdateCalculations].Start();
      particleSet->ApplyBoundaryConditions(currentTimestep);

      // steps 7 & 2 combined
      logging::Logger::Log<logging::Debug, logging::OnePerCore>("Updating colloid positions");
      particleSet->UpdatePositions();

      timers[reporting::Timers::colloidUpdateCalculations].Stop();
    }

  }
}
