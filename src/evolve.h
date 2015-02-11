/**
 * evolve.cc
 *
 * 2015 Sergio Gonzalez
 *
 */


#include "memory.h"

static Arena g_jpeg_arena;

void* arena_malloc_(size_t size)
{
    return arena_push_(&g_jpeg_arena, size);
}

#define tje_malloc arena_malloc_
#define tje_free(ptr)  // No freeing
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"


static const int NUM_ENCODERS = 1;

int evolve_main(void* big_chunk_of_memory)
{
    // Reserve 20 megs for jpeg code.
    g_jpeg_arena = arena_create(big_chunk_of_memory, 20 * 1024 * 1024);

    int width;
    int height;
    int num_components;
    unsigned char* data = stbi_load("in.bmp", &width, &height, &num_components, 0);

    if (!data)
    {
        tje_log("Could not load bmp file.\n");
        const char* err = stbi_failure_reason();
        tje_log(err);
        return 1;
    }

    tje_assert (num_components == 3);

/*     TJEState state[NUM_ENCODERS]; */

/*     for (int i = 0; i < NUM_ENCODERS; ++i) */
/*     { */
/*         tje_init(&state[i]); */
/*     } */

    int result = tje_encode_to_file(
            data,
            width,
            height,
            "out.jpg");
    return result;
}
