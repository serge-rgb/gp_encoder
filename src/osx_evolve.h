/**
 * evolve.cc
 *
 * 2015 Sergio Gonzalez
 *
 */

#pragma once

#if defined(__MACH__)

#include <sys/mman.h>

int main()
{
    size_t sz = 1 * (1024LL * 1024 * 1024);  // One gigabyte.
    void* big_chunk_of_memory = mmap(
#ifdef TJE_DEBUG
            (void*)(1024LL * 1024 * 1024 * 1024), //  lpAddress,
#else
            NULL, //  lpAddress,
#endif
            size,
            PROT_READ | PROT_WRITE,
            MAP_ANON,
            -1, //fd
            0 //offset
            );
    int result = evolve_main(big_chunk_of_memory);
    return result;
}
