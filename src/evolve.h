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

#include <process.h>

#define NUM_ENCODERS 16

#define KILOBYTES(t) ((t) * 1024LL)
#define MEGABYTES(t) ((t) * KILOBYTES(1))

#define NUM_TABLES 128

static volatile LONG g_num_tables_processed;  // Atomically incremented each time an encoder uses a table.

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
    int table_id;
} EncodeResult;

static uint8*       g_quantization_tables[NUM_TABLES];
static EncodeResult g_encode_results[NUM_TABLES];

// Threading:
//  We have a FIFO of at most NUM_ENCODERS matrices.
//  Encoder threads consume the matrices.
//  When the matrices are consumed, the results are evaluated

unsigned int __stdcall encoder_thread(void* thread_data)
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
        er.table_id = args->table_id;
    }

    // TODO: acquire lock
    // Note: a lock is not required if we guarantee that every encoder in flight has a unique table_id
    g_encode_results[args->table_id] = er;
    // TODO: release

    InterlockedIncrement(&g_num_tables_processed);

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

    Arena init_arenas[NUM_ENCODERS];  // Reset each time an encoder finishes.

    // Init encoders.
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        init_arenas[i] = arena_spawn(&jpeg_arena, memory_for_run);
        tje_init(&init_arenas[i], &state[i]);
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

    int is_good_enough = 0;

    Arena run_arenas[NUM_ENCODERS] = { 0 };
    // Re-usable memory for threads.
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        run_arenas[i] =
            arena_spawn(&init_arenas[i], init_arenas[i].size - init_arenas[i].count);
    }

    while (!is_good_enough)
    {
        // Get semaphore
        // run thread
        //

        int table_id = 0;
        // Process all tables.
        while(g_num_tables_processed != NUM_TABLES)
        {
            HANDLE threads[NUM_ENCODERS] = { 0 };
            // Launch a thread for each encoder.
            for (int i = 0; i < NUM_ENCODERS && table_id < NUM_TABLES; ++i)
            {
                state[i].qt_luma = g_quantization_tables[table_id];
                state[i].qt_chroma = g_quantization_tables[table_id];
                ThreadArgs* args = arena_alloc_elem(&run_arenas[i], ThreadArgs);
                {
                    args->thread_arena = &run_arenas[i];
                    args->state = &state[i];
                    args->width = width;
                    args->height = height;
                    args->data = data;
                    args->table_id = table_id;
                }

                threads[i] = (HANDLE)_beginthreadex(NULL, 0, encoder_thread, args, 0, NULL);

                ++table_id;
                // Clean up state
                // TODO remove?
                state[i].mse = 0;
                state[i].final_size = 0;
            }
            // Wait for encoders to finish..
            WaitForMultipleObjects(NUM_ENCODERS, threads, TRUE, INFINITE);

            for (int i = 0; i < NUM_ENCODERS && table_id < NUM_TABLES; ++i)
            {
                arena_reset(&run_arenas[i]);
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
