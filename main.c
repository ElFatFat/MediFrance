#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#include "modules/render.h"
#include "modules/settings.h"
#include "modules/gen.h"
#include "modules/export.h"
#include "modules/loader.h"
#include "modules/constants.h"

#include <MLV/MLV_all.h>

#define POP_SIZE 200
#define GEN_MAX 1000
#define ELITISM_COUNT 15
#define CSV_PATH "data/communes-france-metrople-2025.csv"
#define MAX_THREADS 12
#define PRINT_EVERY_X_GEN 50

static void log_run_configuration(int num_threads, int log_every) {
    printf("\n========== MediFrance — configuration ==========\n");
    printf("[config] Communes CSV      : %s\n", CSV_PATH);
    printf("[config] Population GA     : %d individus × %d générations\n", POP_SIZE, GEN_MAX);
    printf("[config] Threads OpenMP    : %d (max %d)\n", num_threads, MAX_THREADS);
    printf("[config] Élitisme            : %d\n", ELITISM_COUNT);
    printf("[config] Rayon couverture    : %.0f km\n", COVERAGE_RADIUS_KM);
    printf("[config] Coût / hôpital      : %.0f | Bonus CHRU : %.0f\n", HOSPITAL_COST, CHRU_BONUS);
    printf("[config] Logs tous les       : %d génération(s) (MEDIFRANCE_LOG_EVERY)\n", log_every);
    printf("================================================\n\n");
}

static int resolve_log_every(void) {
    int log_every = PRINT_EVERY_X_GEN;
    const char* log_env = getenv("MEDIFRANCE_LOG_EVERY");
    if (log_env != NULL && log_env[0] != '\0') {
        int parsed = atoi(log_env);
        if (parsed > 0) {
            log_every = parsed;
        }
    }
    return log_every;
}

int main(void) {
    int available_threads = omp_get_num_procs();
    int num_threads = (MAX_THREADS < available_threads) ? MAX_THREADS : available_threads;
    omp_set_num_threads(num_threads);

    int log_every = resolve_log_every();
    log_run_configuration(num_threads, log_every);

    const char* csv_path = CSV_PATH;
    double start_time = omp_get_wtime();
    size_t count = 0;
    struct Town* towns = load_towns_csv(csv_path, &count);
    if (towns == NULL) {
        return 1;
    }
    double load_time = omp_get_wtime() - start_time;

    long total_population = 0;
    for (size_t i = 0; i < count; i++) {
        total_population += towns[i].population;
    }
    set_habitants_total(total_population);

    printf("Loaded %zu communes from %s\n", count, csv_path);
    printf("Time taken to load data: %.2f seconds\n", load_time);

    unsigned char** workspaces = aligned_alloc(64, (size_t)num_threads * sizeof(unsigned char*));
    if (workspaces == NULL) {
        perror("Failed to allocate workspaces array");
        free(towns);
        return 1;
    }
    for (int i = 0; i < num_threads; i++) {
        workspaces[i] = calloc(count, sizeof(unsigned char));
        if (workspaces[i] == NULL) {
            perror("Failed to allocate per-thread workspace");
            for (int j = 0; j < i; j++) {
                free(workspaces[j]);
            }
            free(workspaces);
            free(towns);
            return 1;
        }
    }

    double precalc_start_time = omp_get_wtime();
    OptimizedData* precalc_data = precalc_near(towns, count);
    if (precalc_data == NULL) {
        fprintf(stderr, "Failed to precalculate near data\n");
        for (int i = 0; i < num_threads; i++) {
            free(workspaces[i]);
        }
        free(workspaces);
        free(towns);
        return 1;
    }
    double precalc_time = omp_get_wtime() - precalc_start_time;
    printf("[init] Précalcul voisins terminé en %.2f s\n", precalc_time);
    log_precalc_summary(precalc_data, count);

    init_window();

    GAContext ctx = { towns, count, precalc_data, workspaces, num_threads, log_every };

    MLV_clear_window(MLV_COLOR_BLACK);
    create_cloud(towns, count);
    MLV_actualise_window();

    GAResult res = run_genetic_algorithm(&ctx, POP_SIZE, GEN_MAX, ELITISM_COUNT);
    if (res.best.genes == NULL) {
        fprintf(stderr, "Failed to run genetic algorithm\n");
        close_window();
        for (size_t i = 0; i < count; i++) {
            if (precalc_data[i].neighbors) free(precalc_data[i].neighbors);
        }
        free(precalc_data);
        free(towns);
        for (int i = 0; i < num_threads; i++) free(workspaces[i]);
        free(workspaces);
        return 1;
    }

    audit_coverage(towns, count, precalc_data, &res.best);
    if (export_resultats_csv("data/resultats_hopitaux.csv", towns, count, precalc_data, &res.best) != 0) {
        fprintf(stderr, "Avertissement : export CSV échoué\n");
    }

    settings_menu(towns, count, &res, &ctx);

    close_window();
    for (size_t i = 0; i < count; i++) {
        if (precalc_data[i].neighbors) free(precalc_data[i].neighbors);
    }
    free(precalc_data);
    free(towns);
    free(res.best.genes);
    free(res.best_fitness_history);
    free(res.worst_fitness_history);
    for (int i = 0; i < num_threads; i++) free(workspaces[i]);
    free(workspaces);
    return 0;
}
