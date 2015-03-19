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

#define NUM_ENCODERS 8

#define KILOBYTES(t) ((t) * 1024LL)
#define MEGABYTES(t) ((t) * KILOBYTES(1))

#define NUM_TABLES 8

typedef struct
{
    Arena*  thread_arena;  // Heap memory unique to this thread
    int table_id;  // Index into the table stacks associated with this thread.
    // ==== Parameters for tje_encode_main:
    TJEState* state;
    int width;
    int height;
    unsigned char* data;
} ThreadArgs;

typedef struct
{
    float   mse;
    float   compression_ratio;
    int     table_id;
} EncodeResult;

static uint8*       g_quantization_tables[NUM_TABLES];
static EncodeResult g_encode_results[NUM_TABLES];

// Threading:
//  We have an array of at most NUM_TABLES matrices.
//  Encoder threads consume the matrices.
//  When the matrices are consumed, the results are evaluated

unsigned int __stdcall encoder_thread(void* thread_data)
{
    int result = TJE_OK;
    ThreadArgs* args = (ThreadArgs*)(thread_data);

    result = tje_encode_main(
            args->thread_arena, args->state, args->data, args->width, args->height);

    EncodeResult er = { 0 };
    {
        er.mse = args->state->mse;
        er.compression_ratio = args->state->compression_ratio;
        er.table_id = args->table_id;
    }

    // Note: a lock is not required because we guarantee that every encoder in
    // flight has a unique table_id
    g_encode_results[args->table_id] = er;

    return result;
}

static void gen_quality_table(uint8* table)
{
    for (int i = 0; i < 64; ++i)
    {
        table[i] = 8;
    }
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

    // Init tables
    for (int i = 0; i < NUM_TABLES; ++i)
    {
        g_quantization_tables[i] = arena_alloc_array(&jpeg_arena, 64, uint8);
        gen_quality_table(g_quantization_tables[i]);
    }

    const size_t memory_for_encoder = (jpeg_arena.size - jpeg_arena.count) / NUM_ENCODERS;

    // We use init_arenas for tje_init, but then we pass all the memory to
    // run_arenas, which will be constantly reset by encoder threads
    Arena init_arenas[NUM_ENCODERS];
    Arena run_arenas[NUM_ENCODERS];

    // Init encoders.
    for (int i = 0; i < NUM_ENCODERS; ++i)
    {
        init_arenas[i] = arena_spawn(&jpeg_arena, memory_for_encoder);
        tje_init(&init_arenas[i], &state[i]);
        // Inherit the rest of the memory to run_arenas
        run_arenas[i] = arena_spawn(&init_arenas[i], init_arenas[i].size - init_arenas[i].count);
    }

    // Do the evolution
    const int num_generations = 25;
    float max_ratio = 1.0f;
    for(int generation_i = 0; generation_i < num_generations; ++generation_i)
    {
        int table_id = 0;
        // Process all tables.
        while(table_id != NUM_TABLES)
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
            }
            // Wait for encoders to finish..
            WaitForMultipleObjects(NUM_ENCODERS, threads, TRUE, INFINITE);

            // Cleanup
            for (int i = 0; i < NUM_ENCODERS && table_id < NUM_TABLES; ++i)
            {
                arena_reset(&run_arenas[i]);
                state[i].mse = 0;
            }
        }

        // Evaluation.
        // How much do we care about error vs size:
        //   fitness = error * (fitness_factor) + size * (1 - fitness_factor)
        float fitness_factor = 0.01f;
        // Find maximum error on the first generation.
        if (max_ratio == 1.0f)
        {
            // When this excecutes, 0-index contains the default_qt_all_ones table.
            max_ratio = g_encode_results[0].compression_ratio;
        }
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

                    float score_a =
                        (a.mse * fitness_factor) + (1 - fitness_factor) * (a.compression_ratio / max_ratio);
                    float score_b =
                        (b.mse * fitness_factor) + (1 - fitness_factor) * (b.compression_ratio / max_ratio);
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
        // Keep the fittest. Mutate the rest.
        const int num_survivors = 1;
        const int mutation_wiggle = 4;
        // (+/-)mutation_wiggle for each table index.
        for (int i = num_survivors; i < NUM_TABLES; ++i)
        {
            // Pick a parent
            int parent_i = g_encode_results[(rand() % num_survivors)].table_id;
            int child_i = g_encode_results[i].table_id;
            // Mutate child a bit from parent
            for (int j = 0; j < 64; ++j)
            {
                int should_mutate = (rand() % 60) == 0;
                if (!should_mutate)
                {
                    g_quantization_tables[child_i][j] = g_quantization_tables[parent_i][j];
                    continue;
                }
                uint32 new_value =
                   g_quantization_tables[parent_i][j] + (rand() % mutation_wiggle);

                g_quantization_tables[child_i][j] = (uint8) new_value;
            }
        }
    }

    // Test with our own winning table.
    {
        int win_index = g_encode_results[0].table_id;
        TJEState state = { 0 };

        state.qt_luma   = g_quantization_tables[win_index];
        state.qt_chroma = g_quantization_tables[win_index];

        // Reuse one of the arenas from the evolution looop.
        arena_reset(&init_arenas[0]);
        arena_reset(&run_arenas[0]);

        tje_init(&init_arenas[0], &state);

        int result = tje_encode_main(&run_arenas[0], &state, data, width, height);
        tje_assert(result == TJE_OK);

        FILE* file_out = fopen("out1.jpg", "wb");
        fwrite(state.buffer.ptr, state.buffer.count, 1, file_out);

        result |= fclose(file_out);
    }

    // Test
    tje_encode_to_file(data, width, height, "out.jpg");


    return result;
}
