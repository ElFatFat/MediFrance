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

void set_habitants_total(long total);
long get_habitants_total(void);
float town_distance_km(const Town* a, const Town* b);

void log_precalc_summary(const OptimizedData* data, size_t count);
PopulationSummary summarize_population(const Individual* pop, int pop_size);
void log_population_summary(const PopulationSummary* s, int gen, int gen_max);
void log_generation_timings(int gen, double fitness_s, double sort_s, double repro_s);
void log_solution_details(const Individual* ind, long total_pop, const char* label);
int audit_coverage(const Town* towns, size_t count, const OptimizedData* data, const Individual* ind);

/**
 * @brief Effectue la sectorisation spatiale par grille de hachage et liste les communes à < 10km.
 * @param towns Tableau brut de toutes les communes.
 * @param count Quantité globale de communes.
 * @return      Pointeur vers le tableau de structures optimisées alloué dynamiquement.
 */
OptimizedData* precalc_near(Town* restrict towns, size_t count);

/**
 * @brief Calcule la valeur de fitness globale et les statistiques d'un individu.
 * @param ind             Pointeur vers l'individu à évaluer.
 * @param towns           Tableau de référence des communes.
 * @param data            Structures précalculées de voisinage.
 * @param count           Nombre total de communes.
 * @param coverage_buffer Zone tampon intermédiaire pour éviter le double-comptage.
 */
void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer);

/**
 * @brief Trie par partitionnement récursif (Quicksort) la population d'individus par ordre décroissant.
 * @param pop   Pointeur vers le tableau d'individus.
 * @param left  Index de borne gauche.
 * @param right Index de borne droite.
 */
void quick_sort_population(Individual* pop, int left, int right);

/**
 * @brief Réalise une copie complète bloc à bloc des attributs et des gènes d'un individu source vers une destination.
 * @param dest  Pointeur de l'individu récepteur.
 * @param src   Pointeur de l'individu émetteur.
 * @param count Nombre total de gènes (NB_COMMUNES).
 */
void copy_individual(Individual* dest, const Individual* src, size_t count);

/**
 * @brief Induit des perturbations génotypiques via l'ajout stratégique et l'élagage aléatoire d'hôpitaux.
 * @param ind   L'individu sujet à mutation.
 * @param data  Informations de voisinage géospatial.
 * @param count Taille du chromosome.
 * @param seed  Pointeur vers la graine du générateur de nombres pseudo-aléatoires (`rand_r`).
 */
void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed);

/**
 * @brief Produit un descendant hybride par combinaison croisée en deux points à partir de deux parents distincts.
 * @param enfant Pointeur vers la structure destinée à recueillir le génome enfant.
 * @param p1     Pointeur vers le premier parent.
 * @param p2     Pointeur vers le second parent.
 * @param count  Nombre d'éléments génotypiques à croiser.
 * @param seed   Pointeur vers la graine pseudo-aléatoire.
 */
void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed);

#endif
