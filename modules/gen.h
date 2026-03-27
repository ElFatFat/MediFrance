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

OptimizedData* precalc_near(Town* towns, size_t count);
void fitness(Individual* ind, Town* towns, OptimizedData* data, size_t count, unsigned char* coverage_buffer);
void quick_sort_population(Individual* pop, int left, int right);
void copy_individual(Individual* dest, const Individual* src, size_t count);
void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed);
void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed);
#endif