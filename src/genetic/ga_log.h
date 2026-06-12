#ifndef GA_LOG_H
#define GA_LOG_H

#include "gen.h"

/**
 * @file ga_log.h
 * @brief Outils de journalisation statistique, de chronométrie et d'audit de couverture médicale.
 *
 * Ce module permet de suivre l'évolution de la convergence de l'algorithme génétique,
 * d'afficher des résumés de performances et d'effectuer des vérifications de cohérence 
 * géospatiale sur les solutions trouvées.
 */

/**
 * @struct PopulationSummary
 * @brief Structure contenant les métriques statistiques agrégées d'une génération.
 * * Rassemble des indicateurs clés (fitness, taux de couverture, lits, hôpitaux) pour donner 
 * un aperçu global de la santé de la population à une itération donnée.
 */
typedef struct {
    double best_fitness;      /**< Meilleure fitness de la génération (première après tri). */
    double worst_fitness;     /**< Moins bonne fitness de la génération. */
    double median_fitness;    /**< Fitness de l'individu médian. */
    double avg_fitness;       /**< Moyenne des fitness de toute la population. */
    long best_covered_pop;    /**< Nombre d'habitants sauvés/couverts par le meilleur individu. */
    long best_desert_pop;     /**< Population laissée dans un désert médical par la meilleure solution. */
    double best_coverage_pct; /**< Pourcentage de la population française couverte par la meilleure solution. */
    int best_hospitals;       /**< Nombre total d'hôpitaux ouverts dans la meilleure solution. */
    int best_chru;            /**< Nombre de CHRU (centres à fort bonus) ouverts dans la meilleure solution. */
    long best_beds;           /**< Nombre total de lits déployés à l'échelle nationale par la meilleure solution. */
    int worst_hospitals;      /**< Nombre d'hôpitaux ouverts par le moins bon individu de la génération. */
} PopulationSummary;

/**
 * @brief Affiche le compte-rendu du précalcul de la grille de voisinage.
 * * Donne des statistiques descriptives sur le maillage spatial : minimum, moyenne et maximum 
 * de voisins trouvés, ainsi que l'identification des pics de densité.
 * * @param data  Pointeur vers le tableau de données géographiques précalculées.
 * @param count Nombre total de communes indexées.
 */
void log_precalc_summary(const OptimizedData* data, size_t count);

/**
 * @brief Calcule les indicateurs statistiques d'une population.
 * * Parcourt la population (supposée triée au préalable) et extrait les données nécessaires 
 * au remplissage de la structure `PopulationSummary`.
 * * @param pop      Tableau des individus de la population courante.
 * @param pop_size Nombre d'individus dans la population.
 * @return         Une structure PopulationSummary complétée avec les statistiques calculées.
 */
PopulationSummary summarize_population(const Individual* pop, int pop_size);

/**
 * @brief Imprime les statistiques de la population dans le terminal.
 * * @param s       Pointeur vers le résumé statistique à afficher.
 * @param gen     Index de la génération courante.
 * @param gen_max Nombre maximum de générations prévu par l'exécution.
 */
void log_population_summary(const PopulationSummary* s, int gen, int gen_max);

/**
 * @brief Journalise les temps de calcul précis (profiling) des phases internes d'une génération.
 * * Permet d'identifier si les goulots d'étranglement se situent dans le calcul parallèle de la fitness, 
 * le tri rapide ou la phase séquentielle/parallèle de reproduction.
 * * @param gen       Index de la génération ciblée.
 * @param fitness_s Temps écoulé (en secondes) pour l'évaluation des fitness.
 * @param sort_s    Temps écoulé (en secondes) pour l'étape de tri de la population.
 * @param repro_s   Temps écoulé (en secondes) pour le croisement, sélection et mutation.
 */
void log_generation_timings(int gen, double fitness_s, double sort_s, double repro_s);

/**
 * @brief Affiche les détails complets de la structure de coûts et gains de la solution optimale finale.
 * * Découpe analytiquement la fitness finale : part de population couverte, pénalités financières 
 * dues au nombre d'hôpitaux ouverts, bonus liés aux CHRU installés et ratio de lits pour 1000 habitants.
 * * @param ind       Pointeur vers l'individu d'élite final à analyser.
 * @param total_pop Constante de population totale de référence (ex: 65 Millions).
 * @param label     Étiquette de texte pour personnaliser l'en-tête du log (ex: "FIN DE RECHERCHE").
 */
void log_solution_details(const Individual* ind, long total_pop, const char* label);

/**
 * @brief Log les statistiques initiales d'ouverture de gènes avant la première évaluation.
 * * Permet de vérifier si le taux d'amorçage aléatoire et intelligent des hôpitaux est 
 * conforme aux configurations du fichier `main.c`.
 * * @param population Tableau d'individus de départ.
 * @param pop_size   Taille globale de la population.
 * @param gene_count Nombre de gènes (communes) par chromosome.
 */
void log_initial_population_stats(const Individual* population, int pop_size, size_t gene_count);

/**
 * @brief Réalise un audit géospatial géométrique complet pour valider le modèle mathématique.
 * * Cette fonction effectue une double passe stricte : elle calcule pour chaque commune de France la vraie 
 * distance la séparant de l'hôpital le plus proche ouvert par l'algorithme génétique. 
 * Elle lève une anomalie si une ville est marquée "au désert" alors qu'un hôpital physique se trouve 
 * géométriquement à $\le 10$ km, ou inversement.
 * * @param towns   Tableau de l'intégralité des communes du projet.
 * @param count   Nombre de communes.
 * @param data    Données géographiques de voisinage précalculées.
 * @param ind     L'individu d'élite à soumettre à l'audit réglementaire.
 * @return        Le nombre total d'anomalies géographiques constatées (idéalement 0).
 */
int audit_coverage(const Town* towns, size_t count, const OptimizedData* data, const Individual* ind);

#endif /* GA_LOG_H */