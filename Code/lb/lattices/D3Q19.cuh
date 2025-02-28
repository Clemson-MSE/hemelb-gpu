
// This file is part of HemeLB and is Copyright (C)
// the HemeLB team and/or their institutions, as detailed in the
// file AUTHORS. This software is provided under the terms of the
// license in the file LICENSE.

#ifndef HEMELB_LB_LATTICES_D3Q19_CUH
#define HEMELB_LB_LATTICES_D3Q19_CUH

#include "units.h"

namespace hemelb
{
  namespace lb
  {
    namespace lattices
    {
      namespace GPU
      {
        namespace D3Q19
        {
          __constant__ const Direction NUMVECTORS = 19;

          __constant__ const distribn_t CXD[] = { 0, 1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0 };
          __constant__ const distribn_t CYD[] = { 0, 0, 0, 1, -1, 0, 0, 1, -1, -1, 1, 0, 0, 0, 0, 1, -1, 1, -1 };
          __constant__ const distribn_t CZD[] = { 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 1, -1, -1, 1, 1, -1, -1, 1 };

          __constant__ const distribn_t EQMWEIGHTS[] = {
            1.0 / 3.0,
            1.0 / 18.0,
            1.0 / 18.0,
            1.0 / 18.0,
            1.0 / 18.0,
            1.0 / 18.0,
            1.0 / 18.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0,
            1.0 / 36.0
          };

          __constant__ const Direction INVERSEDIRECTIONS[] = { 0, 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17 };
        }
      }
    }
  }
}

#endif
