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


#define NUM_ENCODERS 4

int evolve_main(void* big_chunk_of_memory)
{
    // Reserve 20 megs for jpeg code.
    g_jpeg_arena = create_arena_from_array(big_chunk_of_memory, 300 * 1024 * 1024);

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

    TJEState state[NUM_ENCODERS] = {0};
    Arena run_arenas[NUM_ENCODERS];  // Reset each time an encoder finishes.

    int result = TJE_OK;
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        state[i].qt_luma   = default_qt_luma;
        state[i].qt_chroma = default_qt_chroma;
        ImgDataBuffer buffer;// = {};
        {
            // Assume that we will achieve at least 50% reduction
            size_t sz = (size_t)((width * height * 3) / 2);
            buffer.data = tje_malloc(sz);
            buffer.used = 0;
            buffer.size = sz;
        }
        state[i].buffer = &buffer;
        // One arena for each.
        run_arenas[i] = create_arena(&g_jpeg_arena, 10 * 1024 * 1024);
        tje_init(&run_arenas[i], &state[i]);

    }

    // Do the evolution
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
    }
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        result = tje_encode_main(&run_arenas[i], &state[i], data, width, height);
        if (result != TJE_OK)
        {
            break;
        }
    }

    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        tje_deinit(&state[i]);
    }


    // Test
    tje_encode_to_file(data, width, height, "out.jpg");
    return result;
}
