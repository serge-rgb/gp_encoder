#define LIBSERG_IMPLEMENTATION
#include <libserg/libserg.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

typedef int32_t b32;
#define true 1
#define false 0

#include "gpu.h"

// TODO: copypasta
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"



#include "gpu.c"

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

    //unsigned char* data = stbi_load("in.bmp", &w, &h, &ncomp, 0);
    if ( !data ) {
        puts("Could not load file");
        return EXIT_FAILURE;
    }

    // Highest quality by default.
    if ( !tje_encode_to_file("out.jpg", w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }
/*
    data = stbi_load("out.jpg", &w, &h, &ncomp, 0);
    if (!data) {
        return EXIT_FAILURE;
    }

    // Q3 -- Highest quality
    if ( 0 == tje_encode_to_file_at_quality("out_q3.jpg", 3, w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }

    // Q2 -- Almost as good as 3, about twice as much compression.
    if ( 0 == tje_encode_to_file_at_quality("out_q2.jpg", 2, w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }

    // Q1 -- About 3 times smaller than Q2. Noticeable artifacts.
    if ( 0 == tje_encode_to_file_at_quality("out_q1.jpg", 1, w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }
    */
    stbi_image_free(data);

    return EXIT_SUCCESS;
}

