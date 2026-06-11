/**
 * @file ga_log.c
 * @brief Implémentation de la journalisation : précalcul, générations, solution finale et audit de couverture.
 */

#include "ga_log.h"
#include "geo.h"
#include "../constants.h"
#include <stdio.h>
#include <stdlib.h>

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

    long habitants_total = get_habitants_total();
    if (habitants_total > 0) {
        s.best_covered_pop = habitants_total - s.best_desert_pop;
        s.best_coverage_pct = 100.0 * (double)s.best_covered_pop / (double)habitants_total;
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

void log_initial_population_stats(const Individual* population, int pop_size, size_t gene_count) {
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
    long habitants_total = get_habitants_total();
    double pct_pop = habitants_total > 0
        ? 100.0 * (double)covered_pop / (double)habitants_total
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
