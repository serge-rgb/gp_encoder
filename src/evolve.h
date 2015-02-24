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


#define NUM_ENCODERS 1

#define KILOBYTES(t) ((t) * 1024LL)
#define MEGABYTES(t) ((t) * KILOBYTES(1))

#ifdef _WIN32
// Note: __stdcall is redundant in x64 code.
#define PLATFORM_CALL __stdcall
#endif

#ifndef PLATFORM_CALL
#define PLATFORM_CALL
#endif


#define NUM_TABLES 3

typedef struct
{
    Arena*  thread_arena;  // Heap memory unique to this thread
    int table_id;  // Index into the table stacks associated with this thread.
    // == Parameters for tje_encode_main
    TJEState* state;
    int width;
    int height;
    unsigned char* data;
} ThreadArgs;

typedef struct
{
    float   mse;
    size_t  size;
} EncodeResult;

static uint8*       g_quantization_tables[NUM_TABLES];
static EncodeResult g_encode_results[NUM_TABLES];

// Threading:
//  We have a FIFO of at most NUM_ENCODERS matrices.
//  Encoder threads consume the matrices.
//  When the matrices are consumed, the results are evaluated

unsigned int PLATFORM_CALL encoder_thread(void* thread_data)
{
    int result = TJE_OK;
    ThreadArgs* args = (ThreadArgs*)(thread_data);
    // TODO: modify state with some matrix...
    result = tje_encode_main(
            args->thread_arena, args->state, args->data, args->width, args->height);
    EncodeResult er = { 0 };
    {
        er.mse = args->state->mse;
        er.size = args->state->final_size;
    }

    // TODO: acquire lock
    // Note: a lock is not required if we guarantee that every encoder in flight has a unique table_id
    g_encode_results[args->table_id] = er;
    // TODO: release

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

    Arena run_arenas[NUM_ENCODERS];  // Reset each time an encoder finishes.

    // Init encoders.
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        run_arenas[i] = arena_spawn(&jpeg_arena, memory_for_run);
        tje_init(&run_arenas[i], &state[i]);
    }

    // Init tables
    // TODO: add some randomization
    for (int i = 0; i < NUM_TABLES; ++i)
    {
        int r = i % 3;
        switch(r)
        {
        case 0:
            {
                g_quantization_tables[i] = default_qt_luma_from_spec;
                break;
            }
        case 1:
            {
                g_quantization_tables[i] = default_qt_chroma_from_paper;
                break;
            }
        case 2:
            {
                g_quantization_tables[i] = default_qt_all_ones;
                break;
            }
        default:
            {
                tje_assert(!"Case not handled");
                break;
            }
        }
    }

    // Do the evolution

    // Evaluation 1: encode image for each matrix.

    // TODO: simple job system. producer/consumer. Fixed thread number.
    // Grab matrices atomically.
    //
    // == Producer:
    // While not satisfied:
    //  mutate and crossover using fitness func.
    //  push to queue
    //  wait for processed stack.
    // == Consumer:
    //  if stack is not empty,
    //  pop stack.
    //  encode and write results
    // STACKS (same size):
    // [  Matrices   ] -- Plain data, pointers to quantization tables
    // [  STATUS     ] -- fresh, processing, done  (semaphore? .. yes..)
    // [  Results    ] -- a EncodeResult struct for every matrix.

    int is_good_enough = 0;

    while (!is_good_enough)
    {
        // Process all tables.
        int table_id = 0;
        for (int i = 0; i < NUM_ENCODERS; ++i)
        {
            // Get matrices
            state[i].qt_luma = g_quantization_tables[table_id];
            state[i].qt_chroma = g_quantization_tables[table_id];
            ThreadArgs args = {0};
            {
                args.thread_arena = &run_arenas[i];
                args.state = &state[i];
                args.width = width;
                args.height = height;
                args.data = data;
                args.table_id = table_id;
            }
            result = encoder_thread(&args);
            if (result != TJE_OK)
            {
                break;
            }
            // Clean up state
            state[i].mse = 0;
            state[i].final_size = 0;
            table_id++;
            // Wrap around encoders until we are finished with the tables.
            if (i == NUM_ENCODERS - 1 && table_id < NUM_TABLES)
            {
                i = -1;
            }
        }

        // Evaluation.
        // How much do we care about error vs size:
        //   fitness = error * (fitness_factor) + size * (1 - fitness_factor)
        float fitness_factor = 0.5f;
        // Do a bubble sort.
        bool32 sorted = 0;
        while(!sorted)
        {
            sorted = 1;
            for (int i = 0; i < NUM_TABLES; ++i)
            {
                for (int j = i + 1; j < NUM_TABLES; ++j)
                {
                    EncodeResult a = g_encode_results[i];
                    EncodeResult b = g_encode_results[j];

                    float score_a = a.mse * fitness_factor + a.size * (1 - fitness_factor);
                    float score_b = b.mse * fitness_factor + b.size * (1 - fitness_factor);
                    if (score_b < score_a)
                    {
                        // Swap
                        g_encode_results[i] = b;
                        g_encode_results[j] = a;
                        sorted = 0;
                    }
                }
            }
        }
        is_good_enough = 1;
    }

    // Do evaluation. Sort encoders.
    // Mutate / cross.

    // Reset root arena
    arena_reset(&jpeg_arena);

    // Test
    tje_encode_to_file(data, width, height, "out.jpg");


    return result;
}
