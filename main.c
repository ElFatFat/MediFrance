#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#include "modules/gui.h"
#include "modules/gen.h"

#include <MLV/MLV_all.h>

#define POP_SIZE 500       // Taille de population par défaut pour le premier lancement
#define GEN_MAX 100        // Nombre de générations par défaut pour le premier lancement
#define ELITISM_COUNT 15   // Nombre d'individus élitistes par défaut (copiés tels quels à la génération suivante)
#define CSV_PATH "resources/communes-france-metrople-2025.csv"

#define MAX_THREADS 8 // Nombre maximum de threads à utiliser (pour limiter la mémoire utilisée par les workspaces)



int main(void) {
    int available_threads = omp_get_num_procs();
    int num_threads = (MAX_THREADS < available_threads) ? MAX_THREADS : available_threads;
    omp_set_num_threads(num_threads);
    printf("Running with %d threads (limit: %d, available: %d)\n", num_threads, MAX_THREADS, available_threads);

    const char* csv_path = CSV_PATH;
    FILE* file = fopen(csv_path, "r");
    struct Town* towns = NULL;
    size_t capacity = 0;
    size_t count = 0;
    char line[512];

    if (file == NULL) {
        perror("Unable to open CSV file");
        return 1;
    }
    double start_time = omp_get_wtime();
    while (fgets(line, sizeof(line), file) != NULL) {
        struct Town town;
        int parsed = sscanf(
            line,
            "%d,%49[^,],%d,%49[^,],%d,%49[^,],%d,%d,%f,%f",
            &town.insee_code,
            town.name,
            &town.region,
            town.region_name,
            &town.departement,
            town.department_name,
            &town.postal_code,
            &town.population,
            &town.y,
            &town.x
        );

        if (parsed != 10) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 1024 : capacity * 2;
            struct Town* resized = realloc(towns, new_capacity * sizeof(*towns));

            if (resized == NULL) {
                perror("Memory allocation failed");
                free(towns);
                fclose(file);
                return 1;
            }

            towns = resized;
            capacity = new_capacity;
        }
        town.visited = 0; // Initialisation du flag de visite
        towns[count] = town;
        count++;
    }

    double load_time = omp_get_wtime() - start_time;

    fclose(file);

    printf("Loaded %zu communes from %s\n", count, csv_path);
    printf("Time taken to load data: %.2f seconds\n", load_time);

    // Align workspaces array to 64 bytes to reduce false sharing
    unsigned char** workspaces = NULL;
    workspaces = aligned_alloc(64, num_threads * sizeof(unsigned char*));

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
        //cleaning in case of precalc failure
        fprintf(stderr, "Failed to precalculate near data\n");
        for (int i = 0; i < num_threads; i++) {
            free(workspaces[i]);
        }
        free(workspaces);
        free(towns);
        return 1;
    }
    double precalc_time = omp_get_wtime() - precalc_start_time;
    printf("Time taken for precalculation: %.2f seconds\n", precalc_time);

    init_window();

    // Contexte réutilisé pour chaque lancement de l'algo (initial et relances depuis l'interface).
    GAContext ctx = { towns, count, precalc_data, workspaces, num_threads };

    // La carte sert de fond ; la courbe de fitness se dessine en temps réel dessous
    // pendant l'évolution (les hôpitaux ne sont affichés qu'à la fin du calcul).
    MLV_clear_window(MLV_COLOR_BLACK);
    create_cloud(towns, count);
    MLV_actualise_window();

    // Premier lancement avec les paramètres par défaut.
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

    // Le menu affiche la carte + la courbe de fitness et peut relancer l'algo avec les
    // paramètres saisis (population / générations / élites). res est passé par pointeur
    // pour être mis à jour à chaque relance.
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
