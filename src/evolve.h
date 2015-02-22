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


// Threading:
//  We have a FIFO of at most NUM_ENCODERS matrices.
//  Encoder threads consume the matrices.
//  When the matrices are consumed, the results are evaluated

// Note: __stdcall is redundant in x64 code.
unsigned int __stdcall encoder_thread(void* thread_args)
{
    // translate args.
    // get matrix.
    //
    return 0;
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
            result = tje_encode_main(&run_arenas[i], &state[i], &pqts[i], data, width, height);
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
