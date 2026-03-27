#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#include "modules/gui.h"
#include "modules/gen.h"

#include <MLV/MLV_all.h>

#define POP_SIZE 100
#define GEN_MAX 1000
#define CSV_PATH "resources/communes-france-metrople-2025.csv"

#define STARTING_HOSPITALS_THRESHOLD 30000 //Choix arbitraire pour amorcer la population avec des hôpitaux sur les zones très peuplées
#define STARTING_HOSPITALS_THRESHOLD_PROBABILITY 20 //Probabilité (en %) de placer un hôpital sur une zone très peuplée au démarrage
#define STARTING_HOSPITALS_PROBABILITY 2 //Probabilité (en %) de placer un hôpital sur une zone au démarrage (en dehors des zones très peuplées)
#define SEED_OFFSET 12345 //Offset pour la seed de base, afin d'avoir des résultats différents à chaque exécution
#define TOURNAMENT_PROBABILITY 70 //Probabilité (en %) de faire du tournoi pour sélectionner les parents plutôt que du random pur
#define TOURNAMENT_SIZE 5 //Taille du tournoi (nombre de candidats comparés)
#define RANDOM_PARENT_POOL_SIZE 20 //Taille du pool de parents aléatoires (dans le cas où on ne fait pas de tournoi)

#define MAX_THREADS 8 // Nombre maximum de threads à utiliser (pour limiter la mémoire utilisée par les workspaces)


#define PRINT_EVERY_X_GEN 10



int main(void) {
    int available_threads = omp_get_num_procs();
    int num_threads = (MAX_THREADS < available_threads) ? MAX_THREADS : available_threads;
    omp_set_num_threads(num_threads);
    printf("Running with %d threads (limit: %d, available: %d)\n", num_threads, MAX_THREADS, available_threads);

    const char* csv_path = CSV_PATH;
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

    unsigned char** workspaces = malloc(num_threads * sizeof(unsigned char*));
    if (workspaces == NULL) {
        perror("Failed to allocate workspaces array");
        free(communes);
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
            if (precalc_data[g].max_pop_couverte > STARTING_HOSPITALS_THRESHOLD && (rand() % 100 < STARTING_HOSPITALS_THRESHOLD_PROBABILITY)) {
                population[i].genes[g] = 1;
            } else {
                // Le reste commence quasiment vide
                population[i].genes[g] = (rand() % 1000 < STARTING_HOSPITALS_PROBABILITY) ? 1 : 0; 
            }
        }
    }

    for (int gen = 0; gen < GEN_MAX; gen++) {
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
            unsigned int seed = SEED_OFFSET + omp_get_thread_num() * 100 + gen;

            #pragma omp for schedule(static)
            for (int i = 5; i < POP_SIZE; i++) {


                // On fait du tournoi pour sélectionner les parents (70% de chances de faire du tournoi, 30% de faire du random)
                if (rand_r(&seed) % 100 < TOURNAMENT_PROBABILITY) {
                    // Sélectionne TOURNAMENT_SIZE candidats pour chaque parent et prend le meilleur
                    int p1 = -1, p2 = -1;
                    double best_fitness_p1 = -1e100, best_fitness_p2 = -1e100;
                    for (int t = 0; t < TOURNAMENT_SIZE; t++) {
                        int idx = rand_r(&seed) % (POP_SIZE / 2);
                        if (population[idx].fitness > best_fitness_p1 || p1 == -1) {
                            p1 = idx;
                            best_fitness_p1 = population[idx].fitness;
                        }
                    }
                    for (int t = 0; t < TOURNAMENT_SIZE; t++) {
                        int idx = rand_r(&seed) % (POP_SIZE / 2);
                        if (population[idx].fitness > best_fitness_p2 || p2 == -1) {
                            p2 = idx;
                            best_fitness_p2 = population[idx].fitness;
                        }
                    }
                    crossover(&population[i], &population[p1], &population[p2], count, &seed);
                } else {
                    copier_individu(&population[i], &population[rand_r(&seed) % RANDOM_PARENT_POOL_SIZE], count);
                }

                // On mute avec la seed
                muter_intelligente(&population[i], precalc_data, count, &seed);
            }
        }
        double t4 = omp_get_wtime();

        // Affichage des chronos toutes les 10 générations (pour ne pas polluer le terminal)
        if (gen % PRINT_EVERY_X_GEN == 0) {
            printf("\n--- Gen %d ---\n", gen);
            printf("Fitness: %.3fs | Tri: %.3fs | Repro: %.3fs | Total: %.3fs\n", 
                    t2 - t1, t3 - t2, t4 - t3, t4 - t1);
            printf("Meilleure Fitness: %.0f (Desert: %ld) Hopitaux: %d CHRU: %d Lits_Total: %ld\n", population[0].fitness, population[0].hab_desert, population[0].nb_hopitaux, population[0].nb_chru, population[0].nb_lits);
        }
    }
    free(precalc_data);
    free(communes);
    for(int i=0; i<POP_SIZE; i++) free(population[i].genes);
    for(int i=0; i<num_threads; i++) free(workspaces[i]);
    free(workspaces);
    return 0;
}