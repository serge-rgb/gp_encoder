/**
 * linux_evolve.cc
 *
 * 2015 Sergio Gonzalez
 */

#include <stdlib.h>
#include "evolve.h"

int main(int argc, char** argv)
{
    size_t sz = 1 * (1024LL * 1024 * 1024);
    void* big_chunk_of_memory = calloc(1, sz);
    int result = evolve_main(big_chunk_of_memory, sz);
    free(big_chunk_of_memory);
    return result;
}
