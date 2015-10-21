#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define LIBSERG_IMPLEMENTATION
#include <libserg/libserg.h>

#define DJE_IMPLEMENTATION
#include "dummy_jpeg.h"

typedef uint32_t b32;
#ifndef true
#define true 1
#endif  // true
#ifndef false
#define false 0
#endif  // false

#include "gpu.h"



#include "gpu.c"

uint8_t optimal_table[64] = {
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
};


int main()
{
    GPUInfo gpu_info;
    if ( !gpu_init(&gpu_info)) {
        sgl_log("Could not init GPGPU.\n");
        exit(EXIT_FAILURE);
    }

    size_t memsz = 1L * 1024 * 1024 * 1024;
    Arena root_arena = arena_init(sgl_calloc(memsz, 1), memsz);

    if ( !root_arena.ptr ) {
        sgl_log("Can't allocate memory. Exiting\n");
        exit(EXIT_FAILURE);
    }


    int w, h, ncomp;
    //unsigned char* data = stbi_load("pluto.bmp", &w, &h, &ncomp, 0);
    unsigned char* data = stbi_load("in.bmp", &w, &h, &ncomp, 0);

    if ( !data ) {
        puts("Could not load file");
        return EXIT_FAILURE;
    }

    size_t size = (size_t)2 * 1024 * 1024 * 1024;
    void* memory = sgl_calloc(size, 1);
    if ( !memory ) {
        sgl_log("Could not allocate enough memory for the parallel encoder.n");
        return EXIT_FAILURE;
    }

    // Highest quality by default.
    DJEState state = dje_dummy_encode(&root_arena, optimal_table, w, h, ncomp, data);

    uint32_t base_bit_count = state.bit_count / 8;

    arena_reset(&root_arena);
    DJEState other_state = dje_dummy_encode(&root_arena, djei_default_qt_chroma_from_paper, w, h, ncomp, data);


    uint32_t other_bit_count = other_state.bit_count / 8;

    float compression_ratio = (float)other_bit_count / (float)base_bit_count;


    sgl_log("First image size: %d\n"
            "Second image size: %d\n"
            "Normalized compression ratio: %f\n",
            base_bit_count, other_bit_count, compression_ratio);

    stbi_image_free(data);

    return EXIT_SUCCESS;
}
