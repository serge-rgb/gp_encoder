#pragma once

#include <opencl.h>
#include <libserg/libserg.h>

typedef struct GPUInfo_s {
    cl_context context;
} GPUInfo;

b32 gpu_init(GPUInfo*);
