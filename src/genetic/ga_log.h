#ifndef GA_LOG_H
#define GA_LOG_H

#include "gen.h"

/**
 * @file ga_log.h
 * @brief Journalisation et statistiques : précalcul, générations, solution finale et audit de couverture.
 */

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

void log_precalc_summary(const OptimizedData* data, size_t count);
PopulationSummary summarize_population(const Individual* pop, int pop_size);
void log_population_summary(const PopulationSummary* s, int gen, int gen_max);
void log_generation_timings(int gen, double fitness_s, double sort_s, double repro_s);
void log_solution_details(const Individual* ind, long total_pop, const char* label);
void log_initial_population_stats(const Individual* population, int pop_size, size_t gene_count);
int audit_coverage(const Town* towns, size_t count, const OptimizedData* data, const Individual* ind);

#endif /* GA_LOG_H */
