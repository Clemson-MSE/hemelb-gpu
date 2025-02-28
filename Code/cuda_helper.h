
#ifndef HEMELB_CUDA_HELPER_H
#define HEMELB_CUDA_HELPER_H

#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>
#include "units.h"



#if defined(__NVCC__)
#define CUDA_HOST_DEVICE __host__ __device__
#else
#define CUDA_HOST_DEVICE
#endif



#define CUDA_SAFE_CALL(x)                         \
{                                                 \
  cudaError_t error = x;                          \
  if ( error != cudaSuccess ) {                   \
    const char *name = cudaGetErrorName(error);   \
    const char *str = cudaGetErrorString(error);  \
    std::cerr << "\n"                             \
              << "CUDA Error at " #x "\n"         \
              << name << ": " << str << "\n";     \
    exit(1);                                      \
  }                                               \
}



#endif
