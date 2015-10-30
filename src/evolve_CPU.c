#define LIBSERG_IMPLEMENTATION
#include <libserg/libserg.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define DJE_IMPLEMENTATION
#include "dummy_jpeg.h"

#include "extended_jpeg.h"

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

#define NUM_TABLES_PER_GENERATION 32

typedef struct {
    uint8_t*    table;
    float       fitness;
} PopulationElement;

int pe_comp(const void* va, const void* vb)
{
    PopulationElement* a = (PopulationElement*)va;
    PopulationElement* b = (PopulationElement*)vb;

    float precision = 100000.0f;

    int c = (int)(a->fitness*precision - b->fitness*precision);
    return c;
}


int main()
{
    // Uncomment when doing the actual port to opencl

    /* GPUInfo gpu_info; */
    /* if ( !gpu_init(&gpu_info)) { */
    /*     sgl_log("Could not init GPGPU.\n"); */
    /*     exit(EXIT_FAILURE); */
    /* } */

    size_t memsz = 1L * 1024 * 1024 * 1024;
    Arena root_arena = arena_init(sgl_calloc(memsz, 1), memsz);
    if ( !root_arena.ptr ) {
        sgl_log("Can't allocate memory. Exiting\n");
        exit(EXIT_FAILURE);
    }


    int w, h, ncomp;
    char* fname =
            //"diego.bmp";
            //"pluto.bmp";
            //"in.bmp";
            "in_klay.bmp";
    unsigned char* data = stbi_load(fname, &w, &h, &ncomp, 0);

    srand((unsigned int)(*data));

    FILE* plot_file = fopen("evo.dat", "w");
    assert (plot_file);

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

    DJEState base_state = dje_init(&root_arena, w, h, ncomp, data);

    // Optimal state -- The result obtained from using a 1-table. Minimum
    // compression. Maximum quality. The best quality possible for baseline
    // JPEG.
    DJEState optimal_state = base_state;
    dje_encode_main(&optimal_state, optimal_table);

    uint8_t tables[NUM_TABLES_PER_GENERATION][64];

    memcpy(tables[0], optimal_table, 64 * sizeof(uint8_t));
    // Starting from 1 because tables[0] gets filled with ones.
    for (int i = 1; i < NUM_TABLES_PER_GENERATION; ++i) {
        uint8_t* table = tables[i];
        for ( int ti = 0; ti < 64; ++ti ) {
            table[ti] = 1 + (uint8_t)(63.0f * (rand() / (float)RAND_MAX));
        }
    }


    // Fill initial population.
    PopulationElement* population = arena_alloc_array(&root_arena, NUM_TABLES_PER_GENERATION, PopulationElement);
    for (int i = 0; i < NUM_TABLES_PER_GENERATION; ++i) {
        population[i] = (PopulationElement) {
            .table = tables[i],
            .fitness = FLT_MAX,
        };
    }


    Arena iter_arena = arena_push(&root_arena, arena_available_space(&root_arena));

    uint32_t base_bit_count = optimal_state.bit_count / 8;
    float base_mse = (float)optimal_state.mse;
    float last_winner_fitness = FLT_MAX;

    int convergence_hits = 0;
#define CONVERGENCE_LIMIT 4  // If we are withing the convergence threshold 3 times in a row, end evolution loop.

    int num_generations = 100;

    for (int gen_i = 0; gen_i < num_generations; ++gen_i) {
        // Determine fitness.

        for ( int table_i = 0; table_i < NUM_TABLES_PER_GENERATION; ++table_i ) {
            arena_reset(&iter_arena);
            DJEState state = base_state;
            state.arena = &iter_arena;
            dje_encode_main(&state, population[table_i].table);

            uint32_t other_bit_count = state.bit_count / 8;

            float compression_ratio = (float)other_bit_count / (float)base_bit_count;
            float error_ratio       = (float)(state.mse) / optimal_state.mse;
            //float error_weight = 0.23f;
            float error_weight = 0.30f;

            // ----
            //
            float fitness = error_ratio + 2.00f*(1+compression_ratio);
            if (error_ratio < 1.0f) {
                fitness += 1000;
            }


            population[table_i].fitness = fitness;

#if 0
            sgl_log("====\n"
                    "QT1 image size: %d\n"
                    "Second image size: %d\n"
                    "Normalized compression ratio: %f\n"
                    "====\n"
                    "QT1 image error %" PRIu64 "\n"
                    "Second image error %" PRIu64 "\n"
                    "Normalized image error ratio %f\n"
                    "Fitness: %f\n\n",
                    base_bit_count, other_bit_count, compression_ratio,
                    optimal_state.mse, state.mse, error_ratio,
                    fitness);
#endif
        }

        // Sort by fitness.
        qsort(population, NUM_TABLES_PER_GENERATION, sizeof(PopulationElement), pe_comp);

        // Select two survivors.
        PopulationElement survivors[2] = {
            population[0], population[1],
        };

        int num_mutated = (NUM_TABLES_PER_GENERATION - 2) / 2;
        // Note: NUM_TABLES_PER_GENERATION = 2 + num_mutated + num_crossed
        int num_crossed = NUM_TABLES_PER_GENERATION - 2 - num_mutated;

        int population_index = 2;
        int mutation_wiggle = 4;

        for (int i = 0; i < num_mutated; ++i) {
            uint8_t* table = population[population_index++].table;
            uint8_t* parent = (rand() % 2) ? population[0].table : population[1].table;
            for (int ei = 0; ei < 64; ++ei) {
                table[ei] = parent[ei];
                int dice = (rand() % 32) == 0;
                if ( dice ) {
                    int new_val = table[ei] + ((rand() % (2*mutation_wiggle)) - mutation_wiggle);
                    new_val = max(new_val, 0);
                    table[ei] = (uint8_t)new_val;
                }
            }
        }

        for (int i = 0; i < num_crossed; ++i) {
            uint8_t* table = population[population_index++].table;

            for ( int ei = 0; ei < 64; ++ei ) {
                int dice = rand() % 2;
                table[ei] = survivors[dice].table[ei];
            }
        }
        float winner_fitness = population[0].fitness;

        assert (population_index == NUM_TABLES_PER_GENERATION);
        // Safety. No invalid tables because JPEG is fragile.
        for (int i = 0; i < population_index; ++i) {
            for (int ei = 0; ei < 64; ++ei) {
                if (population[i].table[ei] <= 0) {
                    population[i].table[ei] = (uint8_t)(1 + (rand() % mutation_wiggle));
                }
            }
        }


        // Find worst (our horrible janky hack adds 1000 to elements with numerical errors.)
        int last_i = population_index - 1;
        while (population[last_i].fitness > 900) --last_i;

        // Output best and worst.
        sgl_log("Gen %d \nBest: %f\nWorst: %f\n",
                gen_i+1, population[0].fitness, population[last_i].fitness);

        float fdiff = winner_fitness - last_winner_fitness;
        sgl_log("(Diff is %f)\n", fdiff);

        // Break criterion.

        if ( ABS(fdiff) < 0.001f )
            ++convergence_hits;
        else
            convergence_hits = 0;
        if ( convergence_hits == CONVERGENCE_LIMIT )
            break;

        last_winner_fitness = winner_fitness;

        char buffer[1024];
        snprintf(buffer, 1024, "%d %f %f\n", gen_i+1, population[0].fitness, population[last_i].fitness);
        fwrite(buffer, strlen(buffer), 1, plot_file);
    }
    fclose(plot_file);

    // Sort by fitness.
    qsort(population, NUM_TABLES_PER_GENERATION, sizeof(PopulationElement), pe_comp);

    tje_encode_to_file_with_qt("out_evolved.jpg", population[0].table, w, h, ncomp, data);

    stbi_image_free(data);
    sgl_free(root_arena.ptr);

    return EXIT_SUCCESS;
}
