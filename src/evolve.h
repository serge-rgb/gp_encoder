/**
 * evolve.cc
 *
 * 2015 Sergio Gonzalez
 *
 */

#include <libserg/memory.h>

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"


#define NUM_ENCODERS 16

#define KILOBYTES(t) ((t) * 1024LL)
#define MEGABYTES(t) ((t) * KILOBYTES(1))

#ifdef _WIN32
// Note: __stdcall is redundant in x64 code.
#define PLATFORM_CALL __stdcall
#endif

#ifndef PLATFORM_CALL
#define PLATFORM_CALL
#endif


typedef struct
{
    Arena*  thread_arena;
    struct processed_qt* pqt;
    TJEState* state;
    int width;
    int height;
    unsigned char* data;
} ThreadArgs;

// Threading:
//  We have a FIFO of at most NUM_ENCODERS matrices.
//  Encoder threads consume the matrices.
//  When the matrices are consumed, the results are evaluated

unsigned int PLATFORM_CALL encoder_thread(void* thread_data)
{
    int result = TJE_OK;
    ThreadArgs* args = (ThreadArgs*)(thread_data);
    // TODO: modify state with some matrix...
    result = tje_encode_main(args->thread_arena, args->state, args->pqt, args->data, args->width, args->height);
    return result;
}

int evolve_main(void* big_chunk_of_memory, size_t size)
{
    static Arena jpeg_arena;

    jpeg_arena = arena_init(big_chunk_of_memory, size);

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

    int result = TJE_OK;

    TJEState state[NUM_ENCODERS] = {0};
    const size_t memory_for_run = size / NUM_ENCODERS;
    {
        Arena run_arenas[NUM_ENCODERS];  // Reset each time an encoder finishes.
        struct processed_qt pqts[NUM_ENCODERS];
        for (int i = 0; i < NUM_ENCODERS; ++i)
        {
            run_arenas[i] = arena_spawn(&jpeg_arena, memory_for_run);

            state[i].qt_luma   = default_qt_luma;
            state[i].qt_chroma = default_qt_chroma;
            pqts[i] = tje_init(&run_arenas[i], &state[i]);
        }

        // Do the evolution
        for (int i = 0; i < NUM_ENCODERS; ++i)
        {
            ThreadArgs args = {0};
            {
                args.thread_arena = &run_arenas[i];
                args.pqt = &pqts[i];
                args.state = &state[i];
                args.width = width;
                args.height = height;
                args.data = data;
            }
            result = encoder_thread(&args);
            if (result != TJE_OK)
            {
                break;
            }
        }

        // Do evaluation. Sort encoders.
        // Mutate / cross.

        // Reset root arena
        arena_reset(&jpeg_arena);
    }

    // Test
    tje_encode_to_file(data, width, height, "out.jpg");


    return result;
}
