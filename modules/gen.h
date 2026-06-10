#ifndef GEN_H
#define GEN_H

#include "../main.h"

/**
 * @file gen.h
 * @brief Déclarations des types de données et prototypes pour l'algorithme génétique spatialisé.
 */

/**
 * @struct OptimizedData
 * @brief Structure regroupant les données de voisinage précalculées à moins de 10km.
 */
typedef struct {
    int* neighbors;            ///< Tableau d'indices des communes situées à moins de 10km
    int neighbor_count;        ///< Nombre total de communes voisines trouvées
    int is_eligible_for_chru;  ///< Indicateur d'éligibilité CHRU : 1 si population > 80 000, sinon 0
    int max_covered_population;///< Population maximale théoriquement couverte par ce nœud
} OptimizedData;

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
 * @struct Cell
 * @brief Cellule dynamique utilisée pour la sectorisation de la grille de hachage spatial.
 */
typedef struct {
    int* indexArray;          ///< Indices des communes résidentes dans la cellule
    int count;                ///< Nombre effectif de communes recensées
    int capacity;             ///< Capacité maximale allouée pour le tableau dynamique
} Cell;

/**
 * @struct ThreadWorkspace
 * @brief Espace mémoire local pour assurer l'exécution Thread-Safe et l'isolation des tampons de couverture.
 */
typedef struct {
    unsigned char* coverage_buffer; ///< Tableau temporaire d'analyse de couverture spatiale
} ThreadWorkspace;

#define COVERAGE_RADIUS_KM 10.0f

/**
 * @struct PopulationSummary
 * @brief Statistiques agrégées sur une génération de la population.
 */
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

GAResult run_genetic_algorithm(const GAContext* ctx, int pop_size, int gen_max, int elitism_count);

#endif
