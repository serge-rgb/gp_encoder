#pragma once

#include <opencl.h>

typedef struct GPUInfo_s {
    cl_context          context;
    cl_command_queue    queue;
    cl_mem              huffman_memory[2];  // Size and Code.

    // Result buffers.
    cl_mem              bitcount_array_mem;
    cl_mem              mse_mem;

    // Input buffers
    cl_mem              y_blocks_mem;
    cl_mem              qt_mem;  // AA&N post-processed quantization matrix.

    cl_kernel           kernel;
} GPUInfo;

GPUInfo* gpu_init();

int gpu_setup_buffers(GPUInfo* gpu_info,
                      uint8_t* huffsize, uint16_t* huffcode,
                      int num_blocks, DJEBlock* y_blocks);

void gpu_handle_cl_error(cl_int err);
