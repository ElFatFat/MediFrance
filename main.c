#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#include "modules/gui.h"
#include "modules/gen.h"

#include <MLV/MLV_all.h>

#define POP_SIZE 500



int main(void) {
    #pragma omp parallel
{
    if (omp_get_thread_num() == 0) {
        printf("Running with %d threads\n", omp_get_num_threads());
    }
}
    const char* csv_path = "resources/communes-france-metrople-2025.csv";
    FILE* file = fopen(csv_path, "r");
    struct Commune* communes = NULL;
    size_t capacity = 0;
    size_t count = 0;
    char line[512];

    if (file == NULL) {
        perror("Unable to open CSV file");
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        struct Commune commune;
        int parsed = sscanf(
            line,
            "%d,%49[^,],%d,%49[^,],%d,%49[^,],%d,%d,%f,%f",
            &commune.code_insee,
            commune.nom,
            &commune.region,
            commune.region_name,
            &commune.departement,
            commune.departement_name,
            &commune.code_postal,
            &commune.population,
            &commune.x,
            &commune.y
        );

        if (parsed != 10) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 1024 : capacity * 2;
            struct Commune* resized = realloc(communes, new_capacity * sizeof(*communes));

            if (resized == NULL) {
                perror("Memory allocation failed");
                free(communes);
                fclose(file);
                return 1;
            }

            communes = resized;
            capacity = new_capacity;
        }
        commune.visited = 0; // Initialisation du flag de visite
        communes[count] = commune;
        count++;
    }

    

    fclose(file);

    printf("Loaded %zu communes from %s\n", count, csv_path);

    //Creating workspaces for each thread to avoid false sharing
    int max_threads = omp_get_max_threads();
    unsigned char** workspaces = malloc(max_threads * sizeof(unsigned char*));
    if (workspaces == NULL) {
        perror("Failed to allocate workspaces array");
        free(communes);
        return 1;
    }
    for (int i = 0; i < max_threads; i++) {
        workspaces[i] = calloc(count, sizeof(unsigned char));
        if (workspaces[i] == NULL) {
            perror("Failed to allocate per-thread workspace");
            for (int j = 0; j < i; j++) {
                free(workspaces[j]);
            }
            free(workspaces);
            free(communes);
            return 1;
        }
    }


    DataOptimisee* precalc_data = precalc_near(communes, count);
    if (precalc_data == NULL) return 1;

    Individu population[POP_SIZE];
    for (int i = 0; i < POP_SIZE; i++) {
        population[i].genes = calloc(count, sizeof(unsigned char));
        for (size_t g = 0; g < count; g++) {
            // On place quelques hôpitaux au hasard sur les très grosses zones pour amorcer
            if (precalc_data[g].max_pop_couverte > 30000 && (rand() % 100 < 20)) {
                population[i].genes[g] = 1;
            } else {
                // Le reste commence quasiment vide
                population[i].genes[g] = (rand() % 1000 < 2) ? 1 : 0; 
            }
        }
    }

    for (int gen = 0; gen < 1000; gen++) {

    // --- ÉTAPE 1 : FITNESS ---
    double t1 = omp_get_wtime();
    #pragma omp parallel for schedule(dynamic, 10)
    for (int i = 0; i < POP_SIZE; i++) {
        fitness(&population[i], communes, precalc_data, count, workspaces[omp_get_thread_num()]);
    }
    double t2 = omp_get_wtime();

    // --- ÉTAPE 2 : TRI ---
    quick_sort_population(population, 0, POP_SIZE - 1);
    double t3 = omp_get_wtime();

    // --- ÉTAPE 3 : REPRODUCTION & MUTATION ---
    #pragma omp parallel 
    {
        // On crée une graine unique par thread et par génération
        unsigned int seed = 12345 + omp_get_thread_num() * 100 + gen;

        #pragma omp for schedule(static)
        for (int i = 5; i < POP_SIZE; i++) {
            
            if (rand_r(&seed) % 100 < 70) {
                // On choisit parmi le Top 50% de la population au lieu du Top 4% (20)
                // On favorise quand même les meilleurs grâce à un petit "Tournoi" à 2
                int p1_a = rand_r(&seed) % (POP_SIZE / 2);
                int p1_b = rand_r(&seed) % (POP_SIZE / 2);
                int p1 = (population[p1_a].fitness > population[p1_b].fitness) ? p1_a : p1_b;

                int p2_a = rand_r(&seed) % (POP_SIZE / 2);
                int p2_b = rand_r(&seed) % (POP_SIZE / 2);
                int p2 = (population[p2_a].fitness > population[p2_b].fitness) ? p2_a : p2_b;

                crossover(&population[i], &population[p1], &population[p2], count, &seed);
            } else {
                copier_individu(&population[i], &population[rand_r(&seed) % 20], count);
            }

            // On mute avec la seed
            muter_intelligente(&population[i], precalc_data, count, &seed);
        }
    }
    double t4 = omp_get_wtime();

    // Affichage des chronos toutes les 10 générations (pour ne pas polluer le terminal)
    if (gen % 10 == 0) {
        printf("\n--- Gen %d ---\n", gen);
        printf("Fitness: %.3fs | Tri: %.3fs | Repro: %.3fs | Total: %.3fs\n", 
                t2 - t1, t3 - t2, t4 - t3, t4 - t1);
        printf("Meilleure Fitness: %.0f (Desert: %ld) Hopitaux: %d CHRU: %d\n", population[0].fitness, population[0].hab_desert, population[0].nb_hopitaux, population[0].nb_chru);
    }
}
    init_window();

    create_cloud(communes, count);
    MLV_wait_seconds(5);
    close_window();
    for(size_t i = 0; i < count; i++) {
        if(precalc_data[i].voisins) free(precalc_data[i].voisins);
    }
    free(precalc_data);
    free(communes);
    for(int i=0; i<POP_SIZE; i++) free(population[i].genes);
    for(int i=0; i<max_threads; i++) free(workspaces[i]);
    free(workspaces);
    return 0;
}