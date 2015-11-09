#pragma once

#include <opencl.h>
#include <libserg/libserg.h>

typedef struct GPUInfo_s {
    cl_context context;
} GPUInfo;

GPUInfo* gpu_init();
