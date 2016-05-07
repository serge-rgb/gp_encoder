
#include <stb/stb_image.h>

#if defined(__linux__)
#include <time.h>
#endif

#include "libserg.h"

#ifndef true
#define true 1
#endif  // true
#ifndef false
#define false 0
#endif  // false

#include <libserg/libserg.h>

#include "extended_jpeg.h"

#define DJE_IMPLEMENTATION
#include "dummy_jpeg.h"

typedef uint32_t b32;

#include "gpu.h"

// ----

#include "gpu.c"

uint8_t optimal_table[64] =
{
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
};

#define INITIAL_GENERATION_COUNT 48

typedef struct
{
    uint8_t     table[64];
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

typedef enum
{
    NONE,

    MUTATION,
    CROSSOVER,
    REPRODUCTION,
} EvolutionOption;


PopulationElement* clone_population(PopulationElement* orig)
{
    PopulationElement* result = NULL;
    for (int i = 0; i < sb_count(orig); ++i) {
        sb_push(result, orig[i]);
    }
    return result;
}

PopulationElement grab_element(PopulationElement* population, int start, int* out_idx)
{
    int count = sb_count(population);
    int uniform_number = 1 + (rand() % count);

    // Cumulative
    double pareto_number = 1.0 - (1.0 / (double)uniform_number);

    int pareto_index = count - (int) (pareto_number * (double)count ) - 2;

    if (pareto_index < 0) {
        pareto_index = 0;
    }
    if ( pareto_index < start ) {
        pareto_index = start;
    }

    if ( out_idx )
        *out_idx = pareto_index;

    PopulationElement elem = population[pareto_index];

    return elem;
}

int main()
{
#if 1
    int use_gpu = true;
#else
    int use_gpu = false;
#endif

    GPUInfo* gpu_info = NULL;
    if (use_gpu) {
        gpu_info = gpu_init();
        if (!gpu_info) {
            sgl_log("Could not init GPGPU.\n");
            exit(EXIT_FAILURE);
        }
    }

    size_t memsz = 1L * 1024 * 1024 * 1024;
    Arena root_arena = arena_init(sgl_calloc(memsz, 1), memsz);
    if (!root_arena.ptr) {
        sgl_log("Can't allocate memory. Exiting\n");
        exit(EXIT_FAILURE);
    }

    int w, h, ncomp;
    char* fname =
            "diego.bmp";
            //"pluto.bmp";
            //"in.bmp";
            //"in_klay.bmp";
    unsigned char* data = stbi_load(fname, &w, &h, &ncomp, 0);

    if ( !data ) {
        printf("stb_image failed to load image.\n");
        exit(EXIT_FAILURE);
    }

    srand((unsigned int)(*data));

    FILE* plot_file = fopen("evo.dat", "w");
    assert (plot_file);

    size_t size = (size_t)2 * 1024 * 1024 * 1024;
    void* memory = sgl_calloc(size, 1);
    if ( !memory ) {
        sgl_log("Could not allocate enough memory for the parallel encoder.n");
        return EXIT_FAILURE;
    }

    DJEState base_state = dje_init(&root_arena, gpu_info, w, h, ncomp, data);


    // Optimal state -- The result obtained from using a 1-table. Minimum
    // compression. Maximum quality. The best quality possible for baseline
    // JPEG.
    DJEState optimal_state = base_state;
    dje_encode_main(&optimal_state, gpu_info, optimal_table);

    PopulationElement* population = NULL;

    // ---- Fill initial population.

    for (int i = 0; i < INITIAL_GENERATION_COUNT; ++i) {
        PopulationElement e = {0};

        if (i == 0) {
            memcpy(e.table, optimal_table, 64*sizeof(uint8_t));
        }
        else for ( int ti = 1; ti < 64; ++ti )
        {
            e.table[ti] = 1 + (rand() % 25);
        }
        e.table[0] = 1;

        e.fitness = FLT_MAX;

        sb_push(population, e);
    }

    // Arena used once per item every generation
    Arena iter_arena = arena_push(&root_arena, arena_available_space(&root_arena));

    uint32_t base_bit_count   = optimal_state.bit_count / 8;
    float last_winner_fitness = FLT_MAX;

    int convergence_hits = 0;
#define CONVERGENCE_LIMIT 10  // If we are withing the convergence threshold 4 times in a row, end evolution loop.

    int num_generations = 500;

#ifdef _WIN32
    LARGE_INTEGER run_measure_begin;
    QueryPerformanceCounter(&run_measure_begin);
#elif __linux__
    struct timespec ts = {0};
    clock_gettime(CLOCK_REALTIME, &ts);
#endif

    // Evolution loop ----
    for ( int gen_i = 0; ; ++gen_i ) {
        PopulationElement* old_population = clone_population(population);
        sb_free(population);
        population = NULL;


        // --- Evaluate fitness

        float fitness_sum = 0;
        for ( int elem_i = 0; elem_i < sb_count(old_population); ++elem_i ) {
            arena_reset(&iter_arena);
            DJEState state = base_state;
            state.arena = &iter_arena;
            dje_encode_main(&state, gpu_info, old_population[elem_i].table);

            uint32_t other_bit_count = state.bit_count / 8;

            float compression_ratio = (float)other_bit_count / (float)base_bit_count;
            float error_ratio       = (float)(state.mse) / optimal_state.mse;

            float fitness = error_ratio + 10*(compression_ratio);
            if (error_ratio < 1.0f) {
                fitness += 1000;
            }

            old_population[elem_i].fitness = fitness;
        }

        // Sort by fitness.
        qsort(old_population, sb_count(old_population), sizeof(PopulationElement), pe_comp);
        float winner_fitness = old_population[0].fitness;

        // --- Output

        // Find worst (our horrible janky hack adds 1000 to elements with numerical errors.)
        while (sb_peek(old_population).fitness > 900)
            sb_pop(old_population);

        // Output best and worst.
        sgl_log("Gen %d \nBest: %f\nWorst: %f\n",
                gen_i+1, old_population[0].fitness, old_population[sb_count(old_population) - 1].fitness);

        float fdiff = winner_fitness - last_winner_fitness;
        sgl_log("(Diff is %f)\n", fdiff);

        // Break criterion.

        if ( ABS(fdiff) < 0.0001f ) {
            ++convergence_hits;
        } else {
            convergence_hits = 0;
        }

        if ( gen_i == num_generations || convergence_hits == CONVERGENCE_LIMIT) {
            population = old_population;
            break;
        }

        last_winner_fitness = winner_fitness;

        char buffer[1024];
        snprintf(buffer, 1024, "%d %f %f\n", gen_i+1, old_population[0].fitness, sb_peek(old_population).fitness);
        fwrite(buffer, strlen(buffer), 1, plot_file);

        assert ( population == NULL );


        // ---- Create new population

        /* int prob_mutation = 70; */
        /* int prob_crossover = 25; */
        /* int prob_reproduction = 5; */
#if 1
        int prob_mutation = 80;
        int prob_crossover = 15;
        int prob_reproduction = 5;

        for ( size_t ei = 0; ei < INITIAL_GENERATION_COUNT; ++ei ) {
            EvolutionOption process_select = NONE;

            int dieroll = rand() % 100;

            if (dieroll < prob_reproduction) {
                process_select = REPRODUCTION;
            } else if (dieroll < prob_crossover) {
                process_select = CROSSOVER;
            } else {
                process_select = MUTATION;
            }
#else
        int prob_crossover = 85;
        int prob_reproduction = 10;
        int prob_mutation = 5;

        for ( size_t ei = 0; ei < INITIAL_GENERATION_COUNT; ++ei ) {
            EvolutionOption process_select = NONE;

            int dieroll = rand() % 100;

            if (dieroll < prob_mutation) {
                process_select = MUTATION;
            } else if (dieroll < prob_reproduction) {
                process_select = REPRODUCTION;
            } else {
                process_select = CROSSOVER;
            }
#endif

            switch (process_select) {
            case MUTATION:
                {
                    PopulationElement e = grab_element(old_population, 0, NULL);
                    size_t idx = rand() % 64;
                    int val = e.table[idx];
                    val += -4 + rand() % 8;
                    if (val <= 0)
                    {
                        val = 1;
                    }
                    else if ( val > 255 )
                    {
                        val = 255;
                    }
                    e.table[idx] = (uint8_t)val;
                    sb_push(population, e);
                } break;
            case CROSSOVER: {

#if 0
                PopulationElement children[2] = { 0 };
                PopulationElement parents[2];
                int idx = 0;
                parents[0] = grab_element(old_population, 0, &idx);
                parents[1] = grab_element(old_population, idx+1, NULL);

                size_t crosspoint = rand() % 64;
                for ( int i = 0; i < 64; ++i ) {
                    if ( ei < crosspoint ) {
                        children[0].table[djei_zig_zag[i]] = parents[0].table[djei_zig_zag[i]];
                        children[1].table[djei_zig_zag[i]] = parents[1].table[djei_zig_zag[i]];
                    } else {
                        children[0].table[djei_zig_zag[i]] = parents[1].table[djei_zig_zag[i]];
                        children[1].table[djei_zig_zag[i]] = parents[0].table[djei_zig_zag[i]];
                    }
                }
                sb_push(population, children[0]);
                sb_push(population, children[1]);
#else
                int idx = 0;
                PopulationElement child = {0};
                PopulationElement parents[2];
                parents[0] = grab_element(old_population, 0, &idx);
                parents[1] = grab_element(old_population, idx+1, NULL);
                for ( int i = 0; i < 64; ++i )
                {
                    int d = rand() % 2;
                    child.table[djei_zig_zag[i]] = parents[d].table[djei_zig_zag[i]];
                }
                sb_push(population, child);
#endif

            } break;
            case REPRODUCTION: {
                sb_push(population, grab_element(old_population, 0, NULL));
            } break;
            default:
                break;
            }
            for (int j = 0; j < 64; ++j) {
                if (sb_peek(population).table[j] <= 0) {
                    sgl_assert(!"FAIL");
                }
            }
        }

        sgl_log("Population count: %d\n", sb_count(population));

        // Safety. No invalid tables because JPEG is fragile.
        for (int i = 0; i < sb_count(population); ++i) {
            for (int ei = 0; ei < 64; ++ei) {
                if (population[i].table[ei] <= 0) {
                    sgl_assert ( !"FAIL" );
                }
            }
        }

    }
    fclose(plot_file);

#if defined(_WIN32_)
    LARGE_INTEGER run_measure_end;
    QueryPerformanceCounter(&run_measure_end);
    sgl_log("Total run time: %" PRIu64 "ns \n", run_measure_end.QuadPart - run_measure_begin.QuadPart);
#elif __linux__
    struct timespec ts_end = {0};
    clock_gettime(CLOCK_REALTIME, &ts_end);
    sgl_log("Total run time: %" PRIu64 "us \n", (ts_end.tv_nsec - ts.tv_nsec) / 1000);
#endif


    // Sort by fitness.
    qsort(population, INITIAL_GENERATION_COUNT, sizeof(PopulationElement), pe_comp);

    tje_encode_to_file_with_qt("out_evolved.jpg", population[0].table, w, h, ncomp, data);

    // print winning table
    uint8_t* table = population[0].table;
    for (int j = 0; j < 8; ++j) {
        for (int i = 0; i < 8; ++i) {
            sgl_log("%3i%s", table[j*8+i], (i<8) ? " ": "");
        }
        sgl_log("\n");
    }

    gpu_deinit(gpu_info);
    stbi_image_free(data);
    sgl_free(root_arena.ptr);
    sgl_free(gpu_info);

    return EXIT_SUCCESS;
}
