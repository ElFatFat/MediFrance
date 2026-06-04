#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

#include "modules/gui.h"
#include "modules/gen.h"
#include "modules/export.h"

#include <MLV/MLV_all.h>

#define POP_SIZE 200
#define GEN_MAX 1000
#define CSV_PATH "data/communes-france-metrople-2025.csv"

#define STARTING_HOSPITALS_THRESHOLD 30000 //Choix arbitraire pour amorcer la population avec des hôpitaux sur les zones très peuplées
#define STARTING_HOSPITALS_THRESHOLD_PROBABILITY 20 //Probabilité (en %) de placer un hôpital sur une zone très peuplée au démarrage
#define STARTING_HOSPITALS_PROBABILITY 2 //Probabilité (en dixièmes de %, donc 0.X%) de placer un hôpital sur une zone au démarrage (en dehors des zones très peuplées)
#define SEED_OFFSET 12345 //Offset pour la seed de base, afin d'avoir des résultats différents à chaque exécution
#define TOURNAMENT_PROBABILITY 80 //Probabilité (en %) de faire du tournoi pour sélectionner les parents plutôt que du random pur
#define TOURNAMENT_SIZE 10 //Taille du tournoi (nombre de candidats comparés)
#define RANDOM_PARENT_POOL_SIZE 20 //Taille du pool de parents aléatoires (dans le cas où on ne fait pas de tournoi)
#define ELITISM_COUNT 15 //Nombre d'individus élitistes (qui sont copiés tels quels à la génération suivante sans mutation)

#define MAX_THREADS 12 // Nombre maximum de threads à utiliser (pour limiter la mémoire utilisée par les workspaces)
#define ITERATIONS_PER_THREAD 2 // Nombre d'individus traités par chaque thread avant de synchroniser (pour limiter la contention sur les workspaces)


#define PRINT_EVERY_X_GEN 50



static void log_run_configuration(int num_threads, int log_every) {
    printf("\n========== MediFrance — configuration ==========\n");
    printf("[config] Communes CSV      : %s\n", CSV_PATH);
    printf("[config] Population GA     : %d individus × %d générations\n", POP_SIZE, GEN_MAX);
    printf("[config] Threads OpenMP    : %d (max %d)\n", num_threads, MAX_THREADS);
    printf("[config] Élitisme            : %d | Tournoi %d%% (taille %d)\n",
           ELITISM_COUNT, TOURNAMENT_PROBABILITY, TOURNAMENT_SIZE);
    printf("[config] Rayon couverture    : %.0f km\n", COVERAGE_RADIUS_KM);
    printf("[config] Coût / hôpital      : 5000 | Bonus CHRU : 4000\n");
    printf("[config] Logs tous les       : %d génération(s) (MEDIFRANCE_LOG_EVERY)\n", log_every);
    printf("================================================\n\n");
}

static void log_initial_population_stats(Individual* population, int pop_size, size_t gene_count) {
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

int main(void) {
    int available_threads = omp_get_num_procs();
    int num_threads = (MAX_THREADS < available_threads) ? MAX_THREADS : available_threads;
    omp_set_num_threads(num_threads);

    int log_every = PRINT_EVERY_X_GEN;
    const char* log_env = getenv("MEDIFRANCE_LOG_EVERY");
    if (log_env != NULL && log_env[0] != '\0') {
        int parsed = atoi(log_env);
        if (parsed > 0) {
            log_every = parsed;
        }
    }

    log_run_configuration(num_threads, log_every);
    printf("[init] Threads : %d (processeurs disponibles : %d)\n",
           num_threads, available_threads);

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

    long total_population = 0;
    for (size_t i = 0; i < count; i++) {
        total_population += towns[i].population;
    }
    set_habitants_total(total_population);
    printf("Population totale (métropole): %ld\n", total_population);

    const int headless = getenv("MEDIFRANCE_HEADLESS") != NULL;
    if (headless) {
        printf("Mode headless (MEDIFRANCE_HEADLESS) — pas d'affichage MLV\n");
    }

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
    printf("[init] Précalcul voisins terminé en %.2f s\n", precalc_time);
    log_precalc_summary(precalc_data, count);

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
    for (int i = 0; i < POP_SIZE; i++) {
        for (size_t g = 0; g < count; g++) {
            if (precalc_data[g].max_covered_population > STARTING_HOSPITALS_THRESHOLD && (rand() % 100 < STARTING_HOSPITALS_THRESHOLD_PROBABILITY)) {
                population[i].genes[g] = 1;
            } else {
                population[i].genes[g] = (rand() % 1000 < STARTING_HOSPITALS_PROBABILITY) ? 1 : 0;
            }
        }
    }
    log_initial_population_stats(population, POP_SIZE, count);

    if (!headless) {
        init_window();
    }

    double evolution_start = omp_get_wtime();
    double initial_best_fitness = 0.0;
    int initial_best_hospitals = 0;

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

        int is_last_gen = (gen == GEN_MAX - 1);
        int should_log = (gen % log_every == 0) || is_last_gen;

        if (gen == 0) {
            initial_best_fitness = population[0].fitness;
            initial_best_hospitals = population[0].hospitals_count;
        }

        if (should_log) {
            PopulationSummary summary = summarize_population(population, POP_SIZE);
            log_population_summary(&summary, gen, GEN_MAX);
            log_generation_timings(gen, t2 - t1, t3 - t2, t4 - t3);
        }
        if (!headless) {
            fitness_graph(population[0].fitness, GEN_MAX, gen, MLV_COLOR_BLUE);
            fitness_graph(population[POP_SIZE - 1].fitness, GEN_MAX, gen, MLV_COLOR_YELLOW);
        }
    }

    double evolution_time = omp_get_wtime() - evolution_start;

    fitness(&population[0], towns, precalc_data, count, workspaces[0]);
    log_solution_details(&population[0], total_population, "final");

    printf("\n========== Bilan évolution ==========\n");
    printf("[bilan] Durée évolution      : %.2f s (%.1f ms / génération)\n",
           evolution_time, (evolution_time * 1000.0) / (double)GEN_MAX);
    printf("[bilan] Fitness initiale (g0): %.0f (%d hôpitaux)\n",
           initial_best_fitness, initial_best_hospitals);
    printf("[bilan] Fitness finale       : %.0f (%d hôpitaux)\n",
           population[0].fitness, population[0].hospitals_count);
    printf("[bilan] Gain absolu          : %.0f\n",
           population[0].fitness - initial_best_fitness);
    if (initial_best_fitness != 0.0) {
        printf("[bilan] Gain relatif         : %.2f %%\n",
               100.0 * (population[0].fitness - initial_best_fitness) / initial_best_fitness);
    }
    printf("=====================================\n");

    audit_coverage(towns, count, precalc_data, &population[0]);

    if (export_resultats_csv("data/resultats_hopitaux.csv", towns, count, precalc_data, &population[0]) != 0) {
        fprintf(stderr, "Avertissement : export CSV échoué\n");
    }

    if (!headless) {
        MLV_clear_window(MLV_COLOR_BLACK);
        create_cloud(towns, count);
        settings_menu(towns, count, population[0]);
        close_window();
    }
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