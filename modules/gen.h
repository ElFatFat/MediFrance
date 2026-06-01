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

// Contexte partagé entre toutes les exécutions de l'algo génétique.
// Regroupe les données précalculées et réutilisables (communes, voisins,
// workspaces par thread) afin de pouvoir relancer l'algo sans tout recharger.
typedef struct {
    Town* towns;
    size_t count;
    OptimizedData* precalc_data;
    unsigned char** workspaces;
    int num_threads;
} GAContext;

// Résultat d'une exécution de l'algo génétique : le meilleur individu et
// l'historique de fitness par génération (pour tracer la courbe a posteriori).
// L'appelant doit libérer best.genes, best_fitness_history et worst_fitness_history.
typedef struct {
    Individual best;
    double* best_fitness_history;  // meilleure fitness par génération (longueur = generations)
    double* worst_fitness_history; // pire fitness par génération (longueur = generations)
    int generations;               // nombre de générations effectivement exécutées
} GAResult;

OptimizedData* precalc_near(Town* restrict towns, size_t count);
void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer);
void quick_sort_population(Individual* pop, int left, int right);
void copy_individual(Individual* dest, const Individual* src, size_t count);
void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed);
void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed);

// Exécute l'algorithme génétique complet avec les paramètres fournis et
// retourne le meilleur individu trouvé. Les gènes du résultat sont alloués
// dynamiquement : l'appelant doit faire free(resultat.genes).
// En cas d'échec d'allocation, le champ .genes du résultat vaut NULL.
GAResult run_genetic_algorithm(const GAContext* ctx, int pop_size, int gen_max, int elitism_count);
#endif