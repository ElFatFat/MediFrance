/**
 * @file gen.c
 * @brief Moteur de l'algorithme génétique : fitness, opérateurs et boucle d'évolution.
 */

#include "gen.h"
#include "ga_log.h"
#include "../gui/render.h"
#include "../constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

static long g_habitants_total = 65141355;

#define PROB_DELETE 80
#define MAX_DISCOVER_HOSPITALS 10
#define MIN_DISCOVER_HOSPITALS 3
#define MAX_DELETE_HOSPITALS 10
#define MIN_DELETE_HOSPITALS 3
#define MAX_TRY 150

#define DEFAULT_POP_SIZE 10
#define DEFAULT_GEN_MAX 1000
#define DEFAULT_ELITISM_COUNT 15
#define STARTING_HOSPITALS_THRESHOLD 30000
#define STARTING_HOSPITALS_THRESHOLD_PROBABILITY 20
#define STARTING_HOSPITALS_PROBABILITY 2
#define SEED_OFFSET 12345
#define TOURNAMENT_PROBABILITY 80
#define TOURNAMENT_SIZE 10
#define RANDOM_PARENT_POOL_SIZE 20
#define ITERATIONS_PER_THREAD 2
#define DEFAULT_LOG_EVERY 50

void set_habitants_total(long total) {
    g_habitants_total = total;
}

long get_habitants_total(void) {
    return g_habitants_total;
}

void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer) {
    ind->hospitals_count = 0;
    ind->chru_count = 0;
    ind->beds_count = 0;

    long covered_population = 0;
    memset(coverage_buffer, 0, count);

    for (size_t i = 0; i < count; i++) {
        if (!ind->genes[i]) {
            continue;
        }

        ind->hospitals_count++;
        if (data[i].is_eligible_for_chru) {
            ind->chru_count++;
        }

        long hospitals_bed_count = 0;

        if (!coverage_buffer[i]) {
            coverage_buffer[i] = 1;
            covered_population += towns[i].population;
            hospitals_bed_count += towns[i].population;
        }

#pragma omp simd
        for (int v = 0; v < data[i].neighbor_count; v++) {
            int neighbor_index = data[i].neighbors[v];
            if (!coverage_buffer[neighbor_index]) {
                coverage_buffer[neighbor_index] = 1;
                covered_population += towns[neighbor_index].population;
                hospitals_bed_count += towns[neighbor_index].population;
            }
        }

        ind->beds_count += (long)(hospitals_bed_count * (BEDS_PER_1000 / 1000.0f));
    }

    ind->desert_population = g_habitants_total - covered_population;
    ind->fitness = (double)g_habitants_total
        - (double)ind->desert_population
        - (HOSPITAL_COST * ind->hospitals_count)
        + (CHRU_BONUS * ind->chru_count);
}

void quick_sort_population(Individual* pop, int left, int right) {
    if (left >= right) return;

    double pivot = pop[right].fitness;
    int i = left - 1;

    for (int j = left; j < right; j++) {
        if (pop[j].fitness > pivot) {
            i++;
            Individual temp = pop[i];
            pop[i] = pop[j];
            pop[j] = temp;
        }
    }
    Individual temp = pop[i + 1];
    pop[i + 1] = pop[right];
    pop[right] = temp;

    quick_sort_population(pop, left, i);
    quick_sort_population(pop, i + 2, right);
}

void copy_individual(Individual* dest, const Individual* src, size_t count) {
    dest->fitness = src->fitness;
    dest->hospitals_count = src->hospitals_count;
    dest->chru_count = src->chru_count;
    dest->desert_population = src->desert_population;
    dest->beds_count = src->beds_count;
    memcpy(dest->genes, src->genes, count * sizeof(unsigned char));
}

void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed) {
    int nb_ajouts = (rand_r(seed) % (MAX_DISCOVER_HOSPITALS - MIN_DISCOVER_HOSPITALS + 1)) + MIN_DISCOVER_HOSPITALS;
    for (int m = 0; m < nb_ajouts; m++) {
        int r = (int)(rand_r(seed) % (unsigned int)count);
        if (data[r].max_covered_population >= HOSPITAL_COST) {
            ind->genes[r] = 1;
        }
    }

    if (rand_r(seed) % 100 < PROB_DELETE) {
        int retryAttempts = 0;
        int closeCount = 0;
        int maxClosures = (rand_r(seed) % (MAX_DELETE_HOSPITALS - MIN_DELETE_HOSPITALS + 1)) + MIN_DELETE_HOSPITALS;

        while (closeCount < maxClosures && retryAttempts < MAX_TRY) {
            int r = (int)(rand_r(seed) % (unsigned int)count);

            if (ind->genes[r] == 1) {
                ind->genes[r] = 0;
                closeCount++;
            }
            retryAttempts++;
        }
    }
}

void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed) {
    int pivot1 = (int)(rand_r(seed) % (unsigned int)(count / 2));
    int pivot2 = pivot1 + (int)(rand_r(seed) % (unsigned int)(count / 2));

    memcpy(enfant->genes, p1->genes, (size_t)pivot1);
    memcpy(enfant->genes + pivot1, p2->genes + pivot1, (size_t)(pivot2 - pivot1));
    memcpy(enfant->genes + pivot2, p1->genes + pivot2, count - (size_t)pivot2);
}

GAResult run_genetic_algorithm(const GAContext* ctx, int pop_size, int gen_max, int elitism_count) {
    Town* towns = ctx->towns;
    size_t count = ctx->count;
    OptimizedData* precalc_data = ctx->precalc_data;
    unsigned char** workspaces = ctx->workspaces;

    GAResult result = {0};
    result.best.genes = NULL;

    if (pop_size < 2) pop_size = DEFAULT_POP_SIZE;
    if (gen_max < 1) gen_max = DEFAULT_GEN_MAX;
    if (elitism_count < 1) elitism_count = DEFAULT_ELITISM_COUNT;
    if (elitism_count > pop_size / 2) elitism_count = pop_size / 2;
    int log_every = ctx->log_every > 0 ? ctx->log_every : DEFAULT_LOG_EVERY;

    double* best_history = malloc((size_t)gen_max * sizeof(double));
    double* worst_history = malloc((size_t)gen_max * sizeof(double));
    if (best_history == NULL || worst_history == NULL) {
        free(best_history);
        free(worst_history);
        return result;
    }

    printf("Lancement de l'algorithme genetique (population=%d, generations=%d, elites=%d)\n",
           pop_size, gen_max, elitism_count);

    Individual* population = malloc((size_t)pop_size * sizeof(Individual));
    Individual* next_population = malloc((size_t)pop_size * sizeof(Individual));
    if (population == NULL || next_population == NULL) {
        perror("Echec d'allocation des tableaux de population");
        free(population);
        free(next_population);
        free(best_history);
        free(worst_history);
        return result;
    }

    int allocated = 0;
    for (int i = 0; i < pop_size; i++) {
        population[i].genes = calloc(count, sizeof(unsigned char));
        next_population[i].genes = calloc(count, sizeof(unsigned char));
        if (population[i].genes == NULL || next_population[i].genes == NULL) {
            perror("Echec d'allocation des genes");
            free(population[i].genes);
            free(next_population[i].genes);
            break;
        }
        allocated++;
    }
    if (allocated < pop_size) {
        for (int j = 0; j < allocated; j++) {
            free(population[j].genes);
            free(next_population[j].genes);
        }
        free(population);
        free(next_population);
        free(best_history);
        free(worst_history);
        return result;
    }

    for (int i = 0; i < pop_size; i++) {
        for (size_t g = 0; g < count; g++) {
            if (precalc_data[g].max_covered_population > STARTING_HOSPITALS_THRESHOLD
                && (rand() % 100 < STARTING_HOSPITALS_THRESHOLD_PROBABILITY)) {
                population[i].genes[g] = 1;
            } else {
                population[i].genes[g] = (rand() % 1000 < STARTING_HOSPITALS_PROBABILITY) ? 1 : 0;
            }
        }
    }
    log_initial_population_stats(population, pop_size, count);

    double evolution_start = omp_get_wtime();
    double initial_best_fitness = 0.0;
    int initial_best_hospitals = 0;

    for (int gen = 0; gen < gen_max; gen++) {
        double t1 = omp_get_wtime();
#pragma omp parallel for schedule(dynamic, ITERATIONS_PER_THREAD)
        for (int i = 0; i < pop_size; i++) {
            fitness(&population[i], towns, precalc_data, count, workspaces[omp_get_thread_num()]);
        }
        double t2 = omp_get_wtime();

        quick_sort_population(population, 0, pop_size - 1);
        double t3 = omp_get_wtime();

        best_history[gen] = population[0].fitness;
        worst_history[gen] = population[pop_size - 1].fitness;

        draw_fitness_point(gen, gen_max, population[0].fitness, population[pop_size - 1].fitness);

        for (int i = 0; i < elitism_count; i++) {
            copy_individual(&next_population[i], &population[i], count);
        }

#pragma omp parallel
        {
            unsigned int seed = SEED_OFFSET + (unsigned int)omp_get_thread_num() * 100U + (unsigned int)gen;

#pragma omp for schedule(static)
            for (int i = elitism_count; i < pop_size; i++) {
                if (rand_r(&seed) % 100 < TOURNAMENT_PROBABILITY) {
                    int p1 = -1;
                    int p2 = -1;
                    double best_fitness_p1 = -1e100;
                    double best_fitness_p2 = -1e100;
                    for (int t = 0; t < TOURNAMENT_SIZE; t++) {
                        int idx = rand_r(&seed) % (pop_size / 2);
                        if (population[idx].fitness > best_fitness_p1 || p1 == -1) {
                            p1 = idx;
                            best_fitness_p1 = population[idx].fitness;
                        }
                    }
                    for (int t = 0; t < TOURNAMENT_SIZE; t++) {
                        int idx = rand_r(&seed) % (pop_size / 2);
                        if (population[idx].fitness > best_fitness_p2 || p2 == -1) {
                            p2 = idx;
                            best_fitness_p2 = population[idx].fitness;
                        }
                    }
                    crossover(&next_population[i], &population[p1], &population[p2], count, &seed);
                } else {
                    int random_pool_size = (RANDOM_PARENT_POOL_SIZE < pop_size) ? RANDOM_PARENT_POOL_SIZE : pop_size;
                    copy_individual(&next_population[i], &population[rand_r(&seed) % random_pool_size], count);
                }

                mutate(&next_population[i], precalc_data, count, &seed);
            }
        }
        double t4 = omp_get_wtime();

        Individual* temp_population = population;
        population = next_population;
        next_population = temp_population;

        int is_last_gen = (gen == gen_max - 1);
        int should_log = (gen % log_every == 0) || is_last_gen;

        if (gen == 0) {
            initial_best_fitness = population[0].fitness;
            initial_best_hospitals = population[0].hospitals_count;
        }

        if (should_log) {
            PopulationSummary summary = summarize_population(population, pop_size);
            log_population_summary(&summary, gen, gen_max);
            log_generation_timings(gen, t2 - t1, t3 - t2, t4 - t3);
        }
    }

    double evolution_time = omp_get_wtime() - evolution_start;

    fitness(&population[0], towns, precalc_data, count, workspaces[0]);
    log_solution_details(&population[0], get_habitants_total(), "final");

    printf("\n========== Bilan évolution ==========\n");
    printf("[bilan] Durée évolution      : %.2f s (%.1f ms / génération)\n",
           evolution_time, (evolution_time * 1000.0) / (double)gen_max);
    printf("[bilan] Fitness initiale (g0): %.0f (%d hôpitaux)\n",
           initial_best_fitness, initial_best_hospitals);
    printf("[bilan] Fitness finale       : %.0f (%d hôpitaux)\n",
           population[0].fitness, population[0].hospitals_count);
    printf("[bilan] Gain absolu          : %.0f\n",
           population[0].fitness - initial_best_fitness);
    if (initial_best_fitness != 0.0) {
        printf("[bilan] Gain relatif         : %.2f %%\n",
               100.0 * (population[0].fitness - initial_best_fitness) / initial_best_fitness);
    }
    printf("=====================================\n");

    result.best.genes = malloc(count * sizeof(unsigned char));
    if (result.best.genes != NULL) {
        copy_individual(&result.best, &population[0], count);
        result.best_fitness_history = best_history;
        result.worst_fitness_history = worst_history;
        result.generations = gen_max;
    } else {
        perror("Echec d'allocation du resultat");
        free(best_history);
        free(worst_history);
    }

    for (int i = 0; i < pop_size; i++) {
        free(population[i].genes);
        free(next_population[i].genes);
    }
    free(population);
    free(next_population);

    return result;
}
