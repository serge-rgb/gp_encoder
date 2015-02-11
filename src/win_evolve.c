/**
 * evolve.cc
 *
 * 2015 Sergio Gonzalez
 *
 */

#pragma once

#ifdef _WIN32

#include <windows.h>

#include "evolve.h"


int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{

    size_t sz = 1 * (1024LL * 1024 * 1024);  // One gigabyte.
    void* big_chunk_of_memory =
        VirtualAlloc(
#ifdef TJE_DEBUG
                (LPVOID)(1024LL * 1024 * 1024 * 1024), //  lpAddress,
#else
                NULL, //  lpAddress,
#endif
                sz,//  dwSize,
                MEM_COMMIT | MEM_RESERVE, //  flAllocationType,
                PAGE_READWRITE//  flProtect
                );

    int result = evolve_main(big_chunk_of_memory, sz);
    return result;
}
#endif
