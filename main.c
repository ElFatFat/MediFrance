#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#include "modules/gui.h"
#include "modules/gen.h"

#include <MLV/MLV_all.h>

#define POP_SIZE 500
#define GEN_MAX 10000
#define CSV_PATH "resources/communes-france-metrople-2025.csv"

#define STARTING_HOSPITALS_THRESHOLD 30000 //Choix arbitraire pour amorcer la population avec des hôpitaux sur les zones très peuplées
#define STARTING_HOSPITALS_THRESHOLD_PROBABILITY 20 //Probabilité (en %) de placer un hôpital sur une zone très peuplée au démarrage
#define STARTING_HOSPITALS_PROBABILITY 2 //Probabilité (en dixièmes de %, donc 0.X%) de placer un hôpital sur une zone au démarrage (en dehors des zones très peuplées)
#define SEED_OFFSET 12345 //Offset pour la seed de base, afin d'avoir des résultats différents à chaque exécution
#define TOURNAMENT_PROBABILITY 80 //Probabilité (en %) de faire du tournoi pour sélectionner les parents plutôt que du random pur
#define TOURNAMENT_SIZE 10 //Taille du tournoi (nombre de candidats comparés)
#define RANDOM_PARENT_POOL_SIZE 20 //Taille du pool de parents aléatoires (dans le cas où on ne fait pas de tournoi)
#define ELITISM_COUNT 15 //Nombre d'individus élitistes (qui sont copiés tels quels à la génération suivante sans mutation)

#define MAX_THREADS 8 // Nombre maximum de threads à utiliser (pour limiter la mémoire utilisée par les workspaces)
#define ITERATIONS_PER_THREAD 2 // Nombre d'individus traités par chaque thread avant de synchroniser (pour limiter la contention sur les workspaces)


#define PRINT_EVERY_X_GEN 1000



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

    Individual* population = malloc(POP_SIZE * sizeof(Individual));
    if (population == NULL) {
        perror("Failed to allocate population array");
        free(precalc_data);
        free(towns);
        for(int i=0; i<num_threads; i++) free(workspaces[i]);
        free(workspaces);
        return 1;
    }
    Individual* next_population = malloc(POP_SIZE * sizeof(Individual));
    if (next_population == NULL) {
        perror("Failed to allocate next population array");
        free(population);
        free(precalc_data);
        free(towns);
        for(int i=0; i<num_threads; i++) free(workspaces[i]);
        free(workspaces);
        return 1;
    }
    for (int i = 0; i < POP_SIZE; i++) {
        population[i].genes = calloc(count, sizeof(unsigned char));
        if (population[i].genes == NULL) {
            perror("Failed to allocate genes array");
            for (int j = 0; j < i; j++) free(population[j].genes);
            free(population);
            free(next_population);
            free(precalc_data);
            free(towns);
            for(int k=0; k<num_threads; k++) free(workspaces[k]);
            free(workspaces);
            return 1;
        }
        next_population[i].genes = calloc(count, sizeof(unsigned char));
        if (next_population[i].genes == NULL) {
            perror("Failed to allocate next genes array");
            for (int j = 0; j <= i; j++) free(population[j].genes);
            for (int j = 0; j < i; j++) free(next_population[j].genes);
            free(population);
            free(next_population);
            free(precalc_data);
            free(towns);
            for(int k=0; k<num_threads; k++) free(workspaces[k]);
            free(workspaces);
            return 1;
        }
    }
    double init_time = omp_get_wtime();
    // Initialize population genes outside the main loop
    for (int i = 0; i < POP_SIZE; i++) {
        for (size_t g = 0; g < count; g++) {
            if (precalc_data[g].max_covered_population > STARTING_HOSPITALS_THRESHOLD && (rand() % 100 < STARTING_HOSPITALS_THRESHOLD_PROBABILITY)) {
                population[i].genes[g] = 1;
            } else {
                population[i].genes[g] = (rand() % 1000 < STARTING_HOSPITALS_PROBABILITY) ? 1 : 0;
            }
        }
    }

    init_window();

    for (int gen = 0; gen < GEN_MAX; gen++) {
        // --- ÉTAPE 1 : FITNESS ---
        double t1 = omp_get_wtime();
        #pragma omp parallel for schedule(dynamic, ITERATIONS_PER_THREAD)
        for (int i = 0; i < POP_SIZE; i++) {
            fitness(&population[i], towns, precalc_data, count, workspaces[omp_get_thread_num()]);
        }
        double t2 = omp_get_wtime();

        // --- ÉTAPE 2 : TRI ---
        quick_sort_population(population, 0, POP_SIZE - 1);
        double t3 = omp_get_wtime();

        // Copie de l'elite vers le buffer de prochaine generation.
        for (int i = 0; i < ELITISM_COUNT; i++) {
            copy_individual(&next_population[i], &population[i], count);
        }

        // --- ÉTAPE 3 : REPRODUCTION & MUTATION ---
        #pragma omp parallel 
        {
            // On crée une graine unique par thread et par génération
            unsigned int seed = SEED_OFFSET + omp_get_thread_num() * 100 + gen;

            #pragma omp for schedule(static)
            for (int i = ELITISM_COUNT; i < POP_SIZE; i++) {


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
                    crossover(&next_population[i], &population[p1], &population[p2], count, &seed);
                } else {
                    int random_pool_size = (RANDOM_PARENT_POOL_SIZE < POP_SIZE) ? RANDOM_PARENT_POOL_SIZE : POP_SIZE;
                    copy_individual(&next_population[i], &population[rand_r(&seed) % random_pool_size], count);
                }

                // On mute avec la seed
                mutate(&next_population[i], precalc_data, count, &seed);
            }
        }
        double t4 = omp_get_wtime();

        // Bascule les buffers: prochaine generation devient la population courante.
        Individual* temp_population = population;
        population = next_population;
        next_population = temp_population;

        // Affichage des chronos toutes les 10 générations (pour ne pas polluer le terminal)
        if (gen % PRINT_EVERY_X_GEN == 0) {
            printf("\n--- Gen %d ---\n", gen);
            printf("Fitness: %.3fs | Tri: %.3fs | Repro: %.3fs | Total: %.3fs\n", 
                    t2 - t1, t3 - t2, t4 - t3, t4 - t1);
            printf("Meilleure Fitness: %.0f (Desert: %ld) Hopitaux: %d CHRU: %d Lits_Total: %ld\n", population[0].fitness, population[0].desert_population, population[0].hospitals_count, population[0].chru_count, population[0].beds_count);
        }
        fitness_graph(population[0].fitness, GEN_MAX, gen, MLV_COLOR_BLUE);
    
        fitness_graph(population[POP_SIZE-1].fitness, GEN_MAX, gen, MLV_COLOR_YELLOW);
    }
    MLV_clear_window(MLV_COLOR_BLACK);	
    
    create_cloud(towns, count);

    settings_menu(towns, count, population[0]);
    close_window();
    for(size_t i = 0; i < count; i++) {
        if(precalc_data[i].neighbors) free(precalc_data[i].neighbors);
    }
    
    free(precalc_data);
    free(towns);
    for(int i=0; i<POP_SIZE; i++) free(population[i].genes);
    free(population);
    for(int i=0; i<POP_SIZE; i++) free(next_population[i].genes);
    free(next_population);
    for(int i=0; i<num_threads; i++) free(workspaces[i]);
    free(workspaces);
    return 0;
}