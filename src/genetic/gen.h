#ifndef GEN_H
#define GEN_H

#include "../../main.h"
#include "geo.h"

/**
 * @file gen.h
 * @brief Déclarations des types, structures et opérateurs du moteur d'algorithme génétique spatialisé.
 *
 * Ce module gère la structure des chromosomes, l'évaluation de la pertinence des 
 * solutions (fitness), le tri récursif, ainsi que les cycles de reproduction (crossover, mutation).
 */

/**
 * @struct Individual
 * @brief Représente un chromosome ou solution potentielle dans la population.
 * * Un individu stocke une carte complète d'implantation des hôpitaux en France ainsi que 
 * les statistiques issues de son évaluation géospatiale par la fonction de fitness.
 */
typedef struct {
    unsigned char* genes;     /**< Tableau binaire de taille NB_COMMUNES (1 si hôpital présent, 0 sinon). */
    double fitness;           /**< Score de qualité global de la solution (à maximiser). */
    int hospitals_count;      /**< Nombre total d'hôpitaux implantés dans cette solution. */
    int chru_count;           /**< Nombre d'établissements ayant obtenu le statut et le bonus CHRU. */
    long desert_population;   /**< Population totale résiduelle laissée hors de portée de soins (désert médical). */
    long beds_count;          /**< Somme cumulée du nombre de lits médicaux déployés. */
} Individual;

/**
 * @struct GAContext
 * @brief Contexte d'exécution regroupant les paramètres et buffers partagés de l'algorithme.
 * * Évite de faire passer de trop nombreux arguments aux fonctions et encapsule les structures
 * de données géographiques ainsi que les espaces de travail parallèles pour les threads OpenMP.
 */
typedef struct {
    Town* towns;                     /**< Tableau de l'intégralité des communes françaises. */
    size_t count;                    /**< Nombre total de communes chargées. */
    OptimizedData* precalc_data;     /**< Données géospatiales et matrices de voisinage précalculées. */
    unsigned char** workspaces;      /**< Espaces de travail par thread pour éviter les conflits d'écriture OpenMP. */
    int num_threads;                 /**< Nombre de threads alloués pour le calcul de la fitness. */
    int log_every;                   /**< Fréquence d'affichage des statistiques (en nombre de générations). */
} GAContext;

/**
 * @struct GAResult
 * @brief Structure de stockage du bilan d'une simulation complète.
 * * Contient le meilleur individu final ainsi que l'historique complet de convergence de la fitness.
 */
typedef struct {
    Individual best;               /**< Le meilleur individu (élite) trouvé à l'issue de la simulation. */
    double* best_fitness_history;  /**< Courbe d'évolution de la meilleure fitness par génération. */
    double* worst_fitness_history; /**< Courbe d'évolution de la pire fitness par génération. */
    int generations;               /**< Nombre total de générations effectivement exécutées. */
} GAResult;

/**
 * @brief Définit la constante de population totale française de référence.
 * @param total Nombre d'habitants total.
 */
void set_habitants_total(long total);

/**
 * @brief Récupère la population totale française utilisée dans les calculs de ratios.
 * @return Valeur courante de la population totale de référence.
 */
long get_habitants_total(void);

/**
 * @brief Calcule la valeur de fitness globale et extrait les statistiques médicales d'un individu.
 * * Cette fonction applique le modèle mathématique métier : elle évalue la population couverte 
 * à moins de 10km (en évitant le double comptage grâce à un buffer de masquage), applique les pénalités 
 * financières par hôpital ouvert, attribue des bonus pour les CHRU légitimes et calcule le besoin en lits.
 * * @note Cette fonction utilise le mot-clé `restrict` pour optimiser la vectorisation par le compilateur.
 * * @param ind             Pointeur vers l'individu à évaluer.
 * @param towns           Tableau de toutes les communes.
 * @param data            Données de voisinage précalculées.
 * @param count           Nombre total de communes.
 * @param coverage_buffer Buffer de travail isolé pour l'exclusion mutuelle du thread appelant.
 */
void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer);

/**
 * @brief Trie la population d'individus par ordre décroissant de fitness.
 * * Implémente l'algorithme Quicksort (tri rapide) par partitionnement récursif.
 * L'élément à l'index 0 contiendra toujours la meilleure solution (élite) à la fin du tri.
 * * @param pop   Tableau d'individus constituant la population.
 * @param left  Index de départ de la sous-section à trier (généralement 0).
 * @param right Index de fin de la sous-section à trier (généralement POP_SIZE - 1).
 */
void quick_sort_population(Individual* pop, int left, int right);

/**
 * @brief Copie l'intégralité des données et des gènes d'un individu source vers un individu destination.
 * * Gère la duplication profonde du tableau dynamique de gènes. Indispensable pour préserver 
 * l'élite d'une génération à l'autre sans partage de pointeurs.
 * * @param dest  Pointeur vers l'individu cible.
 * @param src   Pointeur vers l'individu d'origine à copier.
 * @param count Nombre total de gènes à copier (nombre de communes).
 */
void copy_individual(Individual* dest, const Individual* src, size_t count);

/**
 * @brief Applique des opérateurs de mutation intelligents sur le chromosome d'un individu.
 * * Combine deux approches stochastiques : l'exploration active (ouverture de nouveaux hôpitaux 
 * sur des secteurs à fort potentiel de population) et l'élagage ciblé (fermeture de structures 
 * ou de CHRU doublons pour limiter le coût financier global).
 * * @param ind   Pointeur vers l'individu à muter.
 * @param data  Données géospatiales de voisinage.
 * @param count Nombre total de gènes.
 * @param seed  Pointeur vers la seed `rand_r` propre au thread pour garantir la thread-safety.
 */
void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed);

/**
 * @brief Produit un individu enfant par croisement en deux points (crossover) à partir de deux parents.
 * * Sélectionne deux pivots aléatoires au sein du chromosome pour segmenter et recombiner 
 * les blocs géographiques des parents, favorisant la transmission de sous-systèmes d'hôpitaux viables.
 * * @param enfant Pointeur vers l'individu enfant à générer.
 * @param p1     Pointeur vers le premier parent sélectionné.
 * @param p2     Pointeur vers le second parent sélectionné.
 * @param count  Nombre total de gènes.
 * @param seed   Pointeur vers la seed `rand_r` du thread.
 */
void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed);

/**
 * @brief Exécute la boucle d'évolution complète de l'algorithme génétique.
 * * Orchestre l'intégralité de la simulation à partir d'un contexte : parallélisation OpenMP 
 * de la fitness, tri rapide, sélection par tournoi, préservation de l'élite, croisements/mutations 
 * et gestion de la mémoire par double-buffering.
 * * @param ctx     Le contexte contenant les paramètres et pointeurs de données de la simulation.
 * @param pop_size Taille fixe de la population (ex: 500).
 * @param gen_max  Nombre maximum de générations (ex: 10000).
 * @return        Une structure GAResult contenant l'élite finale et les historiques de fitness.
 */
GAResult run_evolution(GAContext* ctx, int pop_size, int gen_max);

#endif /* GEN_H */