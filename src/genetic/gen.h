#ifndef GEN_H
#define GEN_H

#include "../../main.h"
#include "geo.h"

/**
 * @file gen.h
 * @brief Types et prototypes du moteur d'algorithme génétique spatialisé.
 */

/**
 * @struct Individual
 * @brief Représente un chromosome ou solution potentielle de notre population.
 */
typedef struct {
    unsigned char* genes;     ///< Tableau binaire de taille NB_COMMUNES (1 si hôpital présent, 0 sinon)
    double fitness;           ///< Note/Score de qualité de la solution (à optimiser)
    int hospitals_count;      ///< Compteur global du nombre d'hôpitaux ouverts
    int chru_count;           ///< Compteur du nombre d'établissements CHRU
    long desert_population;   ///< Population totale laissée sans aucune couverture médicale
    long beds_count;          ///< Somme cumulée du nombre de lits disponibles
} Individual;

/**
 * @struct GAContext
 * @brief Données partagées entre plusieurs exécutions de l'algorithme génétique.
 */
typedef struct {
    Town* towns;
    size_t count;
    OptimizedData* precalc_data;
    unsigned char** workspaces;
    int num_threads;
    int log_every;
} GAContext;

/**
 * @struct GAResult
 * @brief Résultat d'une exécution de l'algorithme génétique.
 */
typedef struct {
    Individual best;
    double* best_fitness_history;
    double* worst_fitness_history;
    int generations;
} GAResult;

void set_habitants_total(long total);
long get_habitants_total(void);

/**
 * @brief Calcule la valeur de fitness globale et les statistiques d'un individu.
 */
void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer);

/**
 * @brief Trie par partitionnement récursif (Quicksort) la population d'individus par ordre décroissant.
 */
void quick_sort_population(Individual* pop, int left, int right);

/**
 * @brief Copie un individu vers un autre.
 */
void copy_individual(Individual* dest, const Individual* src, size_t count);

/**
 * @brief Applique des mutations sur un individu.
 */
void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed);

/**
 * @brief Produit un descendant par croisement en deux points.
 */
void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed);

/**
 * @brief Exécute l'algorithme génétique complet et retourne le meilleur individu trouvé.
 */
GAResult run_genetic_algorithm(const GAContext* ctx, int pop_size, int gen_max, int elitism_count);

#endif
