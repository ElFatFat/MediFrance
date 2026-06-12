/**
 * @file export.c
 * @brief Implémentation de la logique d'écriture et d'évaluation finale pour l'export CSV.
 */

#include "export.h"
#include "../constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int export_resultats_csv(const char* path,
                         const Town* towns,
                         size_t count,
                         const OptimizedData* precalc_data,
                         const Individual* best) {

    /* --- Carte de couverture globale --- */
    unsigned char* covered = calloc(count, 1);
    if (!covered) { perror("export: calloc covered"); return -1; }

    for (size_t i = 0; i < count; i++) {
        if (!best->genes[i]) continue;
        covered[i] = 1;
        for (int v = 0; v < precalc_data[i].neighbor_count; v++) {
            covered[precalc_data[i].neighbors[v]] = 1;
        }
    }

    /* --- Calcul du nombre de lits par hôpital (indexé sur les communes) --- */
    int* beds_per_town = calloc(count, sizeof(int));
    unsigned char* bed_assign = calloc(count, 1);
    if (!beds_per_town || !bed_assign) {
        perror("export: calloc beds_per_town");
        free(covered);
        free(beds_per_town);
        free(bed_assign);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        if (!best->genes[i]) {
            continue;
        }

        long pop_cov = 0;

        if (!bed_assign[i]) {
            bed_assign[i] = 1;
            pop_cov += towns[i].population;
        }

        for (int v = 0; v < precalc_data[i].neighbor_count; v++) {
            int j = precalc_data[i].neighbors[v];
            if (!bed_assign[j]) {
                bed_assign[j] = 1;
                pop_cov += towns[j].population;
            }
        }

        if (pop_cov > 0) {
            int beds = (int)(pop_cov * (BEDS_PER_1000 / 1000.0f));
            if (beds < 10) {
                beds = 10;
            }
            beds_per_town[i] = beds;
        }
    }

    free(bed_assign);

    /* --- Écriture du CSV --- */
    FILE* f = fopen(path, "w");
    if (!f) {
        perror("export: fopen");
        free(covered);
        free(beds_per_town);
        return -1;
    }

    /* En-tête : colonnes attendues par le script Python */
    fprintf(f, "nom_dep,ville,population,has_hospital,is_desert,nb_beds,latitude,longitude\n");

    for (size_t i = 0; i < count; i++) {
        int has_hospital = best->genes[i] ? 1 : 0;
        int is_desert    = covered[i]     ? 0 : 1;
        int nb_beds      = beds_per_town[i];

        fprintf(f, "%s,%s,%d,%d,%d,%d,%f,%f\n",
            towns[i].department_name,
            towns[i].name,
            towns[i].population,
            has_hospital,
            is_desert,
            nb_beds,
            towns[i].y,
            towns[i].x
        );
    }

    int desert_towns = 0;
    int hospital_towns = 0;
    long desert_pop = 0;
    long export_beds = 0;

    for (size_t i = 0; i < count; i++) {
        if (!covered[i]) {
            desert_towns++;
            desert_pop += towns[i].population;
        }
        if (best->genes[i]) {
            hospital_towns++;
        }
        export_beds += beds_per_town[i];
    }

    fclose(f);
    free(covered);
    free(beds_per_town);

    long total_pop = 0;
    for (size_t i = 0; i < count; i++) {
        total_pop += towns[i].population;
    }
    double coverage_pct = total_pop > 0
        ? 100.0 * (double)(total_pop - desert_pop) / (double)total_pop
        : 0.0;

    printf("\n[export] Fichier écrit : %s\n", path);
    printf("[export]   Communes exportées  : %zu\n", count);
    printf("[export]   Hôpitaux placés     : %d communes\n", hospital_towns);
    printf("[export]   Déserts médicaux    : %d communes (%ld hab.)\n",
           desert_towns, desert_pop);
    printf("[export]   Couverture pop.     : %.2f %%\n", coverage_pct);
    printf("[export]   Lits déclarés (somme): %ld\n", export_beds);
    return 0;
}