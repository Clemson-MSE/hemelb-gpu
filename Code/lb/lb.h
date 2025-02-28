
// This file is part of HemeLB and is Copyright (C)
// the HemeLB team and/or their institutions, as detailed in the
// file AUTHORS. This software is provided under the terms of the
// license in the file LICENSE.

#ifndef HEMELB_LB_LB_H
#define HEMELB_LB_LB_H

#include "configuration/SimConfig.h"
#include "lb/SimulationState.h"
#include "lb/iolets/BoundaryValues.h"
#include "lb/iolets/InOutLetCosine.cuh"
#include "lb/BuildSystemInterface.h"
#include "lb/MacroscopicPropertyCache.h"
#include "net/IOCommunicator.h"
#include "net/IteratedAction.h"
#include "net/net.h"
#include "reporting/Timers.h"
#include "util/UnitConverter.h"
#include <typeinfo>

namespace hemelb
{
  /**
   * Namespace 'lb' contains classes for the scientific core of the Lattice Boltzman simulation
   */
  namespace lb
  {
    /**
     * Class providing core Lattice Boltzmann functionality.
     * Implements the IteratedAction interface.
     */
    template<class LatticeType>
    class LBM : public net::IteratedAction
    {
      private:
        // Use the kernel, collider, and boundary conditions specified by the build system
        typedef typename HEMELB_KERNEL<LatticeType>::Type KernelType;
        typedef typename collisions::Normal<KernelType> CollisionType;
        typedef typename HEMELB_WALL_INLET_BOUNDARY<CollisionType>::Type StreamerType;

      public:
        /**
         * Constructor, stage 1.
         * Object so initialized is not ready for simulation.
         * Must have Initialise(...) called also. Constructor separated due to need to access
         * the partially initialized LBM in order to initialize the arguments to the second construction phase.
         */
        LBM(hemelb::configuration::SimConfig *iSimulationConfig,
            net::Net* net,
            geometry::LatticeData* latDat,
            SimulationState* simState,
            reporting::Timers &atimings,
            geometry::neighbouring::NeighbouringDataManager *neighbouringDataManager);
        ~LBM();

        void RequestComms(); ///< part of IteratedAction interface.
        void PreSend(); ///< part of IteratedAction interface.
        void PreReceive(); ///< part of IteratedAction interface.
        void PostReceive(); ///< part of IteratedAction interface.
        void EndIteration(); ///< part of IteratedAction interface.

        site_t TotalFluidSiteCount() const;
        void SetTotalFluidSiteCount(site_t);

        int InletCount() const
        {
          return inletCount;
        }
        int OutletCount() const
        {
          return outletCount;
        }

        /**
         * Second constructor.
         *
         */
        void Initialise(vis::Control* iControl,
                        iolets::BoundaryValues* iInletValues,
                        iolets::BoundaryValues* iOutletValues,
                        const util::UnitConverter* iUnits);

        void ReadVisParameters();

        void CalculateMouseFlowField(const ScreenDensity densityIn,
                                     const ScreenStress stressIn,
                                     const LatticeDensity density_threshold_min,
                                     const LatticeDensity density_threshold_minmax_inv,
                                     const LatticeStress stress_threshold_max_inv,
                                     PhysicalPressure &mouse_pressure,
                                     PhysicalStress &mouse_stress);

        hemelb::lb::LbmParameters *GetLbmParams();
        lb::MacroscopicPropertyCache& GetPropertyCache();

      private:
        void SetInitialConditions();

        void InitCollisions();

        void InitialiseGPU();

        // The following function pair simplify initialising the site ranges for each collider object.
        void InitInitParamsSiteRanges(kernels::InitParams& initParams, unsigned& state);
        void AdvanceInitParamsSiteRanges(kernels::InitParams& initParams, unsigned& state);

        /**
         * Ensure that the BoundaryValues objects have all necessary fields populated.
         */
        void PrepareBoundaryObjects();

        void ReadParameters();

        void handleIOError(int iError);

        // Collision objects
        StreamerType* mMidFluidStreamer;
        StreamerType* mWallStreamer;
        StreamerType* mInletStreamer;
        StreamerType* mOutletStreamer;
        StreamerType* mInletWallStreamer;
        StreamerType* mOutletWallStreamer;

        void StreamAndCollide(StreamerType* streamer, const site_t iFirstIndex, const site_t iSiteCount)
        {
          if (mVisControl->IsRendering())
          {
            streamer->template StreamAndCollide<true> (iFirstIndex, iSiteCount, &mParams, mLatDat, propertyCache);
          }
          else
          {
            streamer->template StreamAndCollide<false> (iFirstIndex, iSiteCount, &mParams, mLatDat, propertyCache);
          }
        }

        void PostStep(StreamerType* streamer, const site_t iFirstIndex, const site_t iSiteCount)
        {
          if (mVisControl->IsRendering())
          {
            streamer->template DoPostStep<true> (iFirstIndex, iSiteCount, &mParams, mLatDat, propertyCache);
          }
          else
          {
            streamer->template DoPostStep<false> (iFirstIndex, iSiteCount, &mParams, mLatDat, propertyCache);
          }
        }

        unsigned int inletCount;
        unsigned int outletCount;

        configuration::SimConfig *mSimConfig;
        net::Net* mNet;
        geometry::LatticeData* mLatDat;
        SimulationState* mState;
        iolets::BoundaryValues *mInletValues, *mOutletValues;

        iolets::InOutLetCosineGPU* inlets_dev;
        iolets::InOutLetCosineGPU* outlets_dev;

        LbmParameters mParams;
        vis::Control* mVisControl;

        const util::UnitConverter* mUnits;

        hemelb::reporting::Timers &timings;

        MacroscopicPropertyCache propertyCache;

        geometry::neighbouring::NeighbouringDataManager *neighbouringDataManager;
    };

  } // Namespace lb
} // Namespace hemelb
#endif // HEMELB_LB_LB_H
