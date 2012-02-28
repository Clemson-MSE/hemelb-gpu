#ifndef HEMELB_UNITTESTS_GEOMETRY_DECOMPOSITIONTESTS_H
#define HEMELB_UNITTESTS_GEOMETRY_DECOMPOSITIONTESTS_H
#include "geometry/Decomposition.hpp"
#include "unittests/geometry/Mocks.h"
#include <cppunit/TestFixture.h>

// Let vectors be output to an ostream, so that CPPUNIT can assert equality on them

namespace CPPUNIT_NS {
  template<class T> struct assertion_traits<std::vector<T> >
  {
    static bool equal( const std::vector<T>& x, const std::vector<T>& y )
    {
      return x == y;
    }

    static std::string toString( const std::vector<T>& values )
    {
      std::stringstream output;
      output << "[ "<< std::flush;
      for (typename std::vector<T>::const_iterator value=values.begin(); value!=values.end(); value++ ){
        output << *value << ", " << std::flush;
      }
      output << "]" << std::flush;
      return output.str();
    }
  };
}

namespace hemelb
{
  namespace unittests
  {
    namespace geometry
    {
      using namespace hemelb::geometry;
      typedef DecompositionBase<NetMock> MockedDecomposition;
      class DecompositionTests : public CppUnit::TestFixture
      {
          CPPUNIT_TEST_SUITE(DecompositionTests);
          CPPUNIT_TEST(TestReadingOne);
          CPPUNIT_TEST(TestNonReading);
          CPPUNIT_TEST_SUITE_END();
          
        public:
          DecompositionTests():mockedDecomposition(NULL){}
          
          void setUp()
          {
            netMock=new NetMock();
          }

          void tearDown()
          {
            delete[] inputNeededBlocks;
            delete mockedDecomposition;
          }

          void TestReadingOne()
          {
            SetupMocks(6,2,5,0);
            // Start to record the expected communications calls.
            // First will come, sending to the reading cores, each of the lengths.
            // Then, I would expect to send to the other reading core, my count of needed cores
            unsigned int core_0_requires_count=2;
            unsigned int core_1_requires_count=3;
            unsigned int core_2_requires_count=3;
            unsigned int core_3_requires_count=3;
            unsigned int core_4_requires_count=3;
            netMock->RequireSend(&core_0_requires_count,1,1,"Count");
            // And I would expect the reading core to post a receive from each of the other cores, asking for its count of needed blocks
            netMock->RequireReceive(&core_1_requires_count,1,1,"Count");
            netMock->RequireReceive(&core_2_requires_count,1,2,"Count");
            netMock->RequireReceive(&core_3_requires_count,1,3,"Count");
            netMock->RequireReceive(&core_4_requires_count,1,4,"Count");

            // Then, I would expect to send to the other reading core, my needs
            std::vector<site_t> core_0_requires;
            core_0_requires.push_back(0);
            core_0_requires.push_back(1);
            netMock->RequireSend(&core_0_requires[0],2,1,"Needs");
             
            // Then, I would expect to receive the lists of needed blocks themselves.
            // From core 1, the other reading core, I expect it to need blocks 0,1,2
            std::vector<site_t> core_1_requires;
            core_1_requires.push_back(0);
            core_1_requires.push_back(1);
            core_1_requires.push_back(2);
            netMock->RequireReceive(&core_1_requires[0],3,1,"Needs");
            // From core 2, I expect it to need blocks 1,2,3
            std::vector<site_t> core_2_requires;
            core_2_requires.push_back(1);
            core_2_requires.push_back(2);
            core_2_requires.push_back(3);
            netMock->RequireReceive(&core_2_requires[0],3,2,"Needs");
            // Then, I would expect to receive the lists of needed blocks themselves.
            // From core 3,  I expect it to need blocks 2,3,4
            std::vector<site_t> core_3_requires;
            core_3_requires.push_back(2);
            core_3_requires.push_back(3);
            core_3_requires.push_back(4);
            netMock->RequireReceive(&core_3_requires[0],3,3,"Needs");
            // Then, I would expect to receive the lists of needed blocks themselves.
            // From core 4, I expect it to need blocks 3,4,5
            std::vector<site_t> core_4_requires;
            core_4_requires.push_back(3);
            core_4_requires.push_back(4);
            core_4_requires.push_back(5);
            netMock->RequireReceive(&core_4_requires[0],3,4,"Needs");
            ShareMockNeeds();
            // Finally, I would expect the resulting array of needs on core one to be as planned:
            std::vector<proc_t> needing_block_0;
            needing_block_0.push_back(1);
            std::vector<proc_t> needing_block_1;
            needing_block_1.push_back(1);
            needing_block_1.push_back(2);
            std::vector<proc_t> needing_block_2;
            needing_block_2.push_back(1);
            needing_block_2.push_back(2);
            needing_block_2.push_back(3);
            std::vector<proc_t> needing_block_3;
            needing_block_3.push_back(2);
            needing_block_3.push_back(3);
            needing_block_3.push_back(4);
            std::vector<proc_t> needing_block_4;
            needing_block_4.push_back(3);
            needing_block_4.push_back(4);
            std::vector<proc_t> needing_block_5;
            needing_block_5.push_back(4);
            /*CPPUNIT_ASSERT_EQUAL(needing_block_0,mockedDecomposition->ProcessorsNeedingBlock(0));
            CPPUNIT_ASSERT_EQUAL(needing_block_1,mockedDecomposition->ProcessorsNeedingBlock(1));
            CPPUNIT_ASSERT_EQUAL(needing_block_2,mockedDecomposition->ProcessorsNeedingBlock(2));
            CPPUNIT_ASSERT_EQUAL(needing_block_3,mockedDecomposition->ProcessorsNeedingBlock(3));
            CPPUNIT_ASSERT_EQUAL(needing_block_4,mockedDecomposition->ProcessorsNeedingBlock(4));
            CPPUNIT_ASSERT_EQUAL(needing_block_5,mockedDecomposition->ProcessorsNeedingBlock(5));*/
          }

          void TestNonReading()
          {
            SetupMocks(6,2,5,2);
            // Start to record the expected communications calls.
             // First will come, sending to the reading cores, each of the lengths.
             // So I would expect the non-reading core to post a send to each of the reading cores, its count of needed blocks
             unsigned int core_2_requires_count=3;
             netMock->RequireSend(&core_2_requires_count,1,0);
             netMock->RequireSend(&core_2_requires_count,1,1);
             // Then, I would expect to send my list of needed blocks
             // From core 2, the other reading core, I expect it to need blocks 1,2,3
             std::vector<site_t> core_2_requires;
             core_2_requires.push_back(1);
             core_2_requires.push_back(2);
             core_2_requires.push_back(3);
             netMock->RequireSend(&core_2_requires[0],3,0);
             netMock->RequireSend(&core_2_requires[0],3,1);
            
             ShareMockNeeds();
             // Finally, I would expect the resulting array of needs to be empty
             /*std::vector<proc_t> empty_needs_array;
             
             CPPUNIT_ASSERT_EQUAL(empty_needs_array,mockedDecomposition->ProcessorsNeedingBlock(0));
             CPPUNIT_ASSERT_EQUAL(empty_needs_array,mockedDecomposition->ProcessorsNeedingBlock(1));
             CPPUNIT_ASSERT_EQUAL(empty_needs_array,mockedDecomposition->ProcessorsNeedingBlock(2));
             CPPUNIT_ASSERT_EQUAL(empty_needs_array,mockedDecomposition->ProcessorsNeedingBlock(3));
             CPPUNIT_ASSERT_EQUAL(empty_needs_array,mockedDecomposition->ProcessorsNeedingBlock(4));
             CPPUNIT_ASSERT_EQUAL(empty_needs_array,mockedDecomposition->ProcessorsNeedingBlock(5));*/
          }

          void SetupMocks(const site_t block_count, const proc_t reading_cores, const proc_t core_count, const proc_t current_core){
            blockCount=block_count;
            readingCores=reading_cores;
            rank=current_core;
            size=core_count;
            inputNeededBlocks=new bool[block_count];
            for (site_t i=0;i<block_count;i++){
              // Mock with a tridiagonal needs example
              inputNeededBlocks[i]=(current_core==i || current_core+1==i || current_core-1==i);
            }
          }

          void ShareMockNeeds(){
            mockedDecomposition=new MockedDecomposition(blockCount,inputNeededBlocks,readingCores,*netMock,MPI_COMM_WORLD,rank,size);
          }

        private:
          proc_t readingCores;
          site_t blockCount;
          proc_t size;
          proc_t rank;
          bool *inputNeededBlocks;
          MockedDecomposition *mockedDecomposition;
          NetMock *netMock;

      };

      CPPUNIT_TEST_SUITE_REGISTRATION(DecompositionTests);
    }
  }
}
#endif // ONCE
