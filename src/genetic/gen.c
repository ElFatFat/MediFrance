/**
 * @file gen.c
 * @brief Moteur algorithmique de la simulation génétique et de la modélisation géospatiale.
 */

#include "gen.h"
#include "render.h"
#include "constants.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define CELL_SIZE 0.1f
/* 0.1° ≈ 11 km ; à ~48°N, 10 km en longitude ≈ 0.13° → il faut ±2 cellules */
#define CELL_SEARCH_RADIUS 2

static long g_habitants_total = 65141355;

#define LAT_TO_RAD 0.0174533f
#define LATITUDE_FACTOR 111.32f

#define PROB_DELETE 80
#define MAX_DISCOVER_HOSPITALS 10
#define MIN_DISCOVER_HOSPITALS 3
#define MAX_DELETE_HOSPITALS 10
#define MIN_DELETE_HOSPITALS 3
#define MAX_NEIGHBORS 200
#define MAX_TRY 150

#define DEFAULT_POP_SIZE 200
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

void log_precalc_summary(const OptimizedData* data, size_t count) {
    if (data == NULL || count == 0) {
        printf("[precalc] Aucune donnée.\n");
        return;
    }

    int min_neighbors = data[0].neighbor_count;
    int max_neighbors = min_neighbors;
    long long neighbor_sum = 0;
    int chru_eligible = 0;
    int max_covered_pop = data[0].max_covered_population;
    int idx_max_neighbors = 0;

    for (size_t i = 0; i < count; i++) {
        int n = data[i].neighbor_count;
        neighbor_sum += n;
        if (n < min_neighbors) {
            min_neighbors = n;
        }
        if (n > max_neighbors) {
            max_neighbors = n;
            idx_max_neighbors = (int)i;
        }
        if (data[i].is_eligible_for_chru) {
            chru_eligible++;
        }
        if (data[i].max_covered_population > max_covered_pop) {
            max_covered_pop = data[i].max_covered_population;
        }
    }

    double avg_neighbors = (double)neighbor_sum / (double)count;
    printf("\n[precalc] Grille voisins (rayon %.0f km, maille %.2f°)\n", COVERAGE_RADIUS_KM, CELL_SIZE);
    printf("[precalc]   Communes indexées     : %zu\n", count);
    printf("[precalc]   Voisins / commune     : min %d | moy %.1f | max %d\n",
           min_neighbors, avg_neighbors, max_neighbors);
    printf("[precalc]   Pic de densité       : %d voisins (commune #%d)\n",
           max_neighbors, idx_max_neighbors);
    printf("[precalc]   Communes éligibles CHRU (>%d hab.) : %d\n",
           CHRU_POP_THRESHOLD, chru_eligible);
    printf("[precalc]   Pop. max théorique couverte / site : %d hab.\n", max_covered_pop);
}

PopulationSummary summarize_population(const Individual* pop, int pop_size) {
    PopulationSummary s = {0};
    if (pop_size <= 0) {
        return s;
    }

    double sum_fitness = 0.0;
    s.best_fitness = pop[0].fitness;
    s.worst_fitness = pop[pop_size - 1].fitness;
    s.median_fitness = pop[pop_size / 2].fitness;

    s.best_desert_pop = pop[0].desert_population;
    s.best_hospitals = pop[0].hospitals_count;
    s.best_chru = pop[0].chru_count;
    s.best_beds = pop[0].beds_count;
    s.worst_hospitals = pop[pop_size - 1].hospitals_count;

    for (int i = 0; i < pop_size; i++) {
        sum_fitness += pop[i].fitness;
    }
    s.avg_fitness = sum_fitness / (double)pop_size;

    if (g_habitants_total > 0) {
        s.best_covered_pop = g_habitants_total - s.best_desert_pop;
        s.best_coverage_pct = 100.0 * (double)s.best_covered_pop / (double)g_habitants_total;
    }

    return s;
}

void log_population_summary(const PopulationSummary* s, int gen, int gen_max) {
    printf("\n[gen %4d / %d] Population (triée, meilleur en tête)\n", gen, gen_max);
    printf("[gen %4d]   Fitness  : meilleur %.0f | médiane %.0f | pire %.0f | moyenne %.0f\n",
           gen, s->best_fitness, s->median_fitness, s->worst_fitness, s->avg_fitness);
    printf("[gen %4d]   Couverture: %.2f %% pop. (%ld hab.) | désert %ld hab.\n",
           gen, s->best_coverage_pct, s->best_covered_pop, s->best_desert_pop);
    printf("[gen %4d]   Hôpitaux  : meilleur %d | pire %d | CHRU %d | lits (meilleur) %ld\n",
           gen, s->best_hospitals, s->worst_hospitals, s->best_chru, s->best_beds);
}

void log_generation_timings(int gen, double fitness_s, double sort_s, double repro_s) {
    double total = fitness_s + sort_s + repro_s;
    printf("[gen %4d]   Temps    : fitness %.3fs | tri %.3fs | repro %.3fs | total %.3fs\n",
           gen, fitness_s, sort_s, repro_s, total);
}

void log_solution_details(const Individual* ind, long total_pop, const char* label) {
    long covered = total_pop - ind->desert_population;
    double coverage_pct = total_pop > 0 ? 100.0 * (double)covered / (double)total_pop : 0.0;
    double cost_hospitals = HOSPITAL_COST * (double)ind->hospitals_count;
    double bonus_chru = CHRU_BONUS * (double)ind->chru_count;
    double bed_rate = total_pop > 0 ? 1000.0 * (double)ind->beds_count / (double)total_pop : 0.0;

    printf("\n[%s] Détail de la meilleure solution\n", label);
    printf("[%s]   Fitness totale        : %.0f\n", label, ind->fitness);
    printf("[%s]   Population couverte   : %ld / %ld (%.2f %%)\n",
           label, covered, total_pop, coverage_pct);
    printf("[%s]   Désert médical        : %ld hab.\n", label, ind->desert_population);
    printf("[%s]   Hôpitaux              : %d (pénalité -%.0f)\n",
           label, ind->hospitals_count, cost_hospitals);
    printf("[%s]   CHRU                  : %d (bonus +%.0f)\n", label, ind->chru_count, bonus_chru);
    printf("[%s]   Lits (5,4 / 1000)     : %ld (%.2f lits / 1000 hab.)\n",
           label, ind->beds_count, bed_rate);
}

float town_distance_km(const Town* a, const Town* b) {
    float lat_rad = ((a->y + b->y) * 0.5f) * LAT_TO_RAD;
    float dx = (a->x - b->x) * LATITUDE_FACTOR * cosf(lat_rad);
    float dy = (a->y - b->y) * LATITUDE_FACTOR;
    return sqrtf(dx * dx + dy * dy);
}

OptimizedData* precalc_near(Town* restrict towns, size_t count) {
    float minX = towns[0].x;
    float maxX = minX;
    float minY = towns[0].y;
    float maxY = minY;
    for (size_t i = 1; i < count; i++) {
        if (towns[i].x < minX) minX = towns[i].x;
        if (towns[i].x > maxX) maxX = towns[i].x;
        if (towns[i].y < minY) minY = towns[i].y;
        if (towns[i].y > maxY) maxY = towns[i].y;
    }

    int cols = (int)((maxX - minX) / CELL_SIZE) + 1;
    int rows = (int)((maxY - minY) / CELL_SIZE) + 1;

    Cell** grid = malloc((size_t)rows * sizeof(Cell*));
    if (grid == NULL) {
        perror("Erreur d'allocation de la grille");
        return NULL;
    }
    for (int i = 0; i < rows; i++) {
        grid[i] = calloc((size_t)cols, sizeof(Cell));
        if (grid[i] == NULL) {
            perror("Erreur d'allocation de la grille");
            for (int j = 0; j < i; j++) {
                free(grid[j]);
            }
            free(grid);
            return NULL;
        }
    }

    for (int i = 0; i < (int)count; i++) {
        int c = (int)((towns[i].x - minX) / CELL_SIZE);
        int r = (int)((towns[i].y - minY) / CELL_SIZE);

        if (c >= cols) c = cols - 1;
        if (r >= rows) r = rows - 1;
        if (c < 0) c = 0;
        if (r < 0) r = 0;

        Cell* cell = &grid[r][c];

        if (cell->count >= cell->capacity) {
            int new_cap = (cell->capacity == 0) ? 10 : cell->capacity * 2;
            int* new_indices = realloc(cell->indexArray, (size_t)new_cap * sizeof(int));

            if (new_indices == NULL) {
                perror("Erreur realloc dans la grille");
                for (int rr = 0; rr < rows; rr++) {
                    if (grid[rr]->indexArray) {
                        free(grid[rr]->indexArray);
                    }
                    free(grid[rr]);
                }
                free(grid);
                return NULL;
            }

            cell->indexArray = new_indices;
            cell->capacity = new_cap;
        }
        cell->indexArray[cell->count++] = i;
    }

    OptimizedData* data = malloc(count * sizeof(OptimizedData));
    if (data == NULL) {
        perror("Erreur d'allocation des données optimisées");
        for (int rr = 0; rr < rows; rr++) {
            if (grid[rr]->indexArray) {
                free(grid[rr]->indexArray);
            }
            free(grid[rr]);
        }
        free(grid);
        return NULL;
    }

    for (int i = 0; i < (int)count; i++) {
        data[i].neighbor_count = 0;
        data[i].neighbors = NULL;
        data[i].is_eligible_for_chru = (towns[i].population > CHRU_POP_THRESHOLD) ? 1 : 0;

        int r = (int)((towns[i].y - minY) / CELL_SIZE);
        int c = (int)((towns[i].x - minX) / CELL_SIZE);
        if (r < 0) r = 0;
        else if (r >= rows) r = rows - 1;
        if (c < 0) c = 0;
        else if (c >= cols) c = cols - 1;

        int tempNeighbors[MAX_NEIGHBORS];
        int foundCount = 0;

        for (int dr = -CELL_SEARCH_RADIUS; dr <= CELL_SEARCH_RADIUS; dr++) {
            for (int dc = -CELL_SEARCH_RADIUS; dc <= CELL_SEARCH_RADIUS; dc++) {
                int tr = r + dr;
                int tc = c + dc;
                if (tr < 0 || tr >= rows || tc < 0 || tc >= cols) {
                    continue;
                }
                Cell* cell = &grid[tr][tc];
                for (int k = 0; k < cell->count; k++) {
                    int j = cell->indexArray[k];
                    if (i == j) {
                        continue;
                    }

                    float dist = town_distance_km(&towns[i], &towns[j]);
                    if (dist <= COVERAGE_RADIUS_KM && foundCount < MAX_NEIGHBORS) {
                        tempNeighbors[foundCount++] = j;
                    }
                }
            }
        }

        if (foundCount > 0) {
            data[i].neighbor_count = foundCount;
            data[i].neighbors = malloc((size_t)foundCount * sizeof(int));
            if (data[i].neighbors == NULL) {
                perror("Erreur d'allocation voisins");
                for (int n = 0; n < i; n++) {
                    free(data[n].neighbors);
                }
                free(data);
                for (int rr = 0; rr < rows; rr++) {
                    if (grid[rr]->indexArray) {
                        free(grid[rr]->indexArray);
                    }
                    free(grid[rr]);
                }
                free(grid);
                return NULL;
            }
            memcpy(data[i].neighbors, tempNeighbors, (size_t)foundCount * sizeof(int));
        }

        data[i].max_covered_population = towns[i].population;
        for (int v = 0; v < data[i].neighbor_count; v++) {
            data[i].max_covered_population += towns[data[i].neighbors[v]].population;
        }
    }

    for (int i = 0; i < rows; i++) {
        if (grid[i]->indexArray) {
            free(grid[i]->indexArray);
        }
        free(grid[i]);
    }
    free(grid);

    return data;
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

int audit_coverage(const Town* towns, size_t count, const OptimizedData* data, const Individual* ind) {
    int desert_but_near_hospital = 0;
    long desert_but_near_pop = 0;
    int covered_but_far = 0;
    int covered_towns = 0;
    int desert_towns = 0;
    long covered_pop = 0;
    long desert_pop = 0;

    unsigned char* covered = calloc(count, 1);
    if (!covered) {
        fprintf(stderr, "[audit] Échec allocation mémoire\n");
        return -1;
    }

    for (size_t h = 0; h < count; h++) {
        if (!ind->genes[h]) {
            continue;
        }
        covered[h] = 1;
        for (int v = 0; v < data[h].neighbor_count; v++) {
            covered[data[h].neighbors[v]] = 1;
        }
    }

    for (size_t i = 0; i < count; i++) {
        if (covered[i]) {
            covered_towns++;
            covered_pop += towns[i].population;
        } else {
            desert_towns++;
            desert_pop += towns[i].population;
        }

        float nearest_hospital_km = -1.0f;
        for (size_t h = 0; h < count; h++) {
            if (!ind->genes[h]) {
                continue;
            }
            float d = town_distance_km(&towns[i], &towns[h]);
            if (nearest_hospital_km < 0.0f || d < nearest_hospital_km) {
                nearest_hospital_km = d;
            }
        }

        int is_desert = covered[i] ? 0 : 1;
        if (is_desert && nearest_hospital_km >= 0.0f && nearest_hospital_km <= COVERAGE_RADIUS_KM) {
            desert_but_near_hospital++;
            desert_but_near_pop += towns[i].population;
        }
        if (!is_desert && (nearest_hospital_km < 0.0f || nearest_hospital_km > COVERAGE_RADIUS_KM + 0.05f)) {
            covered_but_far++;
        }
    }

    free(covered);

    double pct_towns = count > 0 ? 100.0 * (double)covered_towns / (double)count : 0.0;
    double pct_pop = g_habitants_total > 0
        ? 100.0 * (double)covered_pop / (double)g_habitants_total
        : 0.0;

    printf("\n[audit] Contrôle couverture (rayon %.0f km)\n", COVERAGE_RADIUS_KM);
    printf("[audit]   Communes couvertes    : %d / %zu (%.1f %%)\n",
           covered_towns, count, pct_towns);
    printf("[audit]   Communes en désert    : %d\n", desert_towns);
    printf("[audit]   Pop. couverte         : %ld (%.2f %%)\n", covered_pop, pct_pop);
    printf("[audit]   Pop. en désert        : %ld\n", desert_pop);
    printf("[audit]   Anomalies géographiques :\n");
    printf("[audit]     - désert mais hôpital ≤%.0f km : %d communes (%ld hab.)\n",
           COVERAGE_RADIUS_KM, desert_but_near_hospital, desert_but_near_pop);
    printf("[audit]     - couvert mais hôpital >%.0f km : %d communes\n",
           COVERAGE_RADIUS_KM, covered_but_far);
    if (desert_but_near_hospital == 0 && covered_but_far == 0) {
        printf("[audit]   Résultat : OK (cohérent avec le modèle)\n");
    } else {
        printf("[audit]   Résultat : %d anomalie(s) à investiguer\n",
               desert_but_near_hospital + covered_but_far);
    }
    return desert_but_near_hospital + covered_but_far;
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

static void log_initial_population_stats(Individual* population, int pop_size, size_t gene_count) {
    long long total_hospitals = 0;
    int min_h = (int)gene_count;
    int max_h = 0;

    for (int i = 0; i < pop_size; i++) {
        int h = 0;
        for (size_t g = 0; g < gene_count; g++) {
            if (population[i].genes[g]) {
                h++;
            }
        }
        total_hospitals += h;
        if (h < min_h) {
            min_h = h;
        }
        if (h > max_h) {
            max_h = h;
        }
    }

    printf("[init] Population initiale (gènes aléatoires, avant fitness) :\n");
    printf("[init]   Hôpitaux / individu : min %d | moy %.1f | max %d\n",
           min_h, (double)total_hospitals / (double)pop_size, max_h);
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
