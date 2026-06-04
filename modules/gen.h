#ifndef GEN_H
#define GEN_H

#include "../main.h"

typedef struct {
    int* neighbors;    // Tableau d'indices des communes à < 10km
    int neighbor_count;
    int is_eligible_for_chru; // 1 si pop > 80 000, sinon 0
    int max_covered_population;
} OptimizedData;

typedef struct {
    unsigned char* genes; // Tableau de taille NB_COMMUNES (0 ou 1)
    double fitness;
    int hospitals_count;
    int chru_count;
    long desert_population;
    long beds_count;
} Individual;

typedef struct {
    int* indexArray;
    int count;
    int capacity;
} Cell;

typedef struct {
    unsigned char* coverage_buffer; // Tableau de 36000 octets
} ThreadWorkspace;

#define COVERAGE_RADIUS_KM 10.0f

typedef struct {
    double best_fitness;
    double worst_fitness;
    double median_fitness;
    double avg_fitness;
    long best_covered_pop;
    long best_desert_pop;
    double best_coverage_pct;
    int best_hospitals;
    int best_chru;
    long best_beds;
    int worst_hospitals;
} PopulationSummary;

void set_habitants_total(long total);
long get_habitants_total(void);
float town_distance_km(const Town* a, const Town* b);

void log_precalc_summary(const OptimizedData* data, size_t count);
PopulationSummary summarize_population(const Individual* pop, int pop_size);
void log_population_summary(const PopulationSummary* s, int gen, int gen_max);
void log_generation_timings(int gen, double fitness_s, double sort_s, double repro_s);
void log_solution_details(const Individual* ind, long total_pop, const char* label);
int audit_coverage(const Town* towns, size_t count, const OptimizedData* data, const Individual* ind);

OptimizedData* precalc_near(Town* restrict towns, size_t count);
void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer);
void quick_sort_population(Individual* pop, int left, int right);
void copy_individual(Individual* dest, const Individual* src, size_t count);
void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed);
void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed);
#endif