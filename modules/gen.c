#include "gen.h"
#include "gui.h" // Pour draw_fitness_point (tracé temps réel de la courbe)
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>


#define CELL_SIZE 0.1 // Environ 11km, parfait pour un rayon de 10km
#define HABITANTS_TOTAL 65141355 // Population totale de la France (constante pour le fitness)
#define HOSPITAL_COST 5000.0
#define CHRU_BONUS 4000.0
#define CHRU_POP_THRESHOLD 80000
#define LAT_TO_RAD 0.0174533 // Conversion degrés en radians pour le calcul de la distance
#define LATITUDE_FACTOR 111.32 // Facteur de conversion pour les degrés de latitude en km
#define BEDS_PER_1000 5.4 // Nombre de lits pour 1000 habitants

#define PROB_DELETE 80 // 80% de chances de supprimer un bâtiment lors de la mutation intelligente
#define MAX_DISCOVER_HOSPITALS 10 // Nombre maximum d'hôpitaux à découvrir lors de la mutation intelligente (pour limiter les changements drastiques)
#define MIN_DISCOVER_HOSPITALS 3 // Nombre minimum d'hôpitaux à découvrir lors de la mutation intelligente (pour assurer une exploration suffisante)
#define MAX_DELETE_HOSPITALS 10 // Nombre maximum d'hôpitaux à supprimer lors de la mutation intelligente (pour limiter les changements drastiques)
#define MIN_DELETE_HOSPITALS 3 // Nombre minimum d'hôpitaux à supprimer lors de la mutation intelligente (pour assurer une exploration suffisante)

#define MAX_NEIGHBORS 200 // Nombre maximum de voisins à stocker pour chaque commune (pour limiter la mémoire)

#define MAX_TRY 150 // Nombre maximum de tentatives pour trouver un hôpital à fermer lors de la mutation intelligente (pour éviter les boucles infinies)

// --- Paramètres d'orchestration de l'algorithme génétique ---
#define DEFAULT_POP_SIZE 500 // Valeur de repli si la taille de population fournie est invalide
#define DEFAULT_GEN_MAX 100 // Valeur de repli si le nombre de générations fourni est invalide
#define STARTING_HOSPITALS_THRESHOLD 30000 // Choix arbitraire pour amorcer la population avec des hôpitaux sur les zones très peuplées
#define STARTING_HOSPITALS_THRESHOLD_PROBABILITY 20 // Probabilité (en %) de placer un hôpital sur une zone très peuplée au démarrage
#define STARTING_HOSPITALS_PROBABILITY 2 // Probabilité (en dixièmes de %, donc 0.X%) de placer un hôpital sur une zone au démarrage (hors zones très peuplées)
#define SEED_OFFSET 12345 // Offset pour la seed de base, afin d'avoir des résultats différents à chaque exécution
#define TOURNAMENT_PROBABILITY 80 // Probabilité (en %) de faire du tournoi pour sélectionner les parents plutôt que du random pur
#define TOURNAMENT_SIZE 10 // Taille du tournoi (nombre de candidats comparés)
#define RANDOM_PARENT_POOL_SIZE 20 // Taille du pool de parents aléatoires (dans le cas où on ne fait pas de tournoi)
#define ITERATIONS_PER_THREAD 2 // Nombre d'individus traités par chaque thread avant de synchroniser (pour limiter la contention sur les workspaces)
#define PRINT_EVERY_X_GEN 1000 // Affichage des chronos toutes les X générations (pour ne pas polluer le terminal)

OptimizedData* precalc_near(Town* restrict towns, size_t count) {
    // --- ÉTAPE 1 : Trouver les bornes et créer la grille ---
    float minX = towns[0].x;
    float maxX = minX;
    float minY = towns[0].y;
    float maxY = minY;
    for(size_t i=1; i<count; i++) {
        if(towns[i].x < minX) minX = towns[i].x;
        if(towns[i].x > maxX) maxX = towns[i].x;
        if(towns[i].y < minY) minY = towns[i].y;
        if(towns[i].y > maxY) maxY = towns[i].y;
    }

    int cols = (int)((maxX - minX) / CELL_SIZE) + 1;
    int rows = (int)((maxY - minY) / CELL_SIZE) + 1;

    Cell** grid = malloc(rows * sizeof(Cell*));
    for(int i=0; i<rows; i++) {
        grid[i] = calloc(cols, sizeof(Cell));
        if (grid[i] == NULL) {
            perror("Erreur d'allocation de la grille");
            // Gérer l'erreur proprement ici
            return NULL;
        }
    }


// --- ÉTAPE 2 : Remplir la grille ---
    for (int i = 0; i < (int)count; i++) {
        // Calcul des indices avec un "clamp" pour la sécurité
        int c = (int)((towns[i].x - minX) / CELL_SIZE);
        int r = (int)((towns[i].y - minY) / CELL_SIZE);

        // Sécurité anti-débordement (si x == maxX ou y == maxY)
        if (c >= cols) c = cols - 1;
        if (r >= rows) r = rows - 1;
        if (c < 0) c = 0;
        if (r < 0) r = 0;

        Cell* cell = &grid[r][c]; // On accède à grid[row][col]

        if (cell->count >= cell->capacity) {
            // Initialisation de capacity si c'est le premier passage
            int new_cap = (cell->capacity == 0) ? 10 : cell->capacity * 2;
            int* new_indices = realloc(cell->indexArray, new_cap * sizeof(int));
            
            if (new_indices == NULL) {
                perror("Erreur realloc dans la grille");
                // Gérer l'erreur proprement ici
                return NULL;
            }
            
            cell->indexArray = new_indices;
            cell->capacity = new_cap;
        }
        cell->indexArray[cell->count++] = i;
    }

    // --- ÉTAPE 3 : Calculer les voisins (Double passe pour la mémoire) ---
    OptimizedData* data = malloc(count * sizeof(OptimizedData));

    if(data == NULL) {
        perror("Erreur d'allocation des données optimisées");
        free(data);
        // Nettoyage de la grille avant de quitter
        for(int i=0; i<rows; i++) { if(grid[i]->indexArray) free(grid[i]->indexArray); }
        for(int i=0; i<rows; i++) free(grid[i]);
        free(grid);
        return NULL;
    }

    for (int i = 0; i < (int)count; i++) {
        data[i].neighbor_count = 0;
        data[i].neighbors = NULL;
        data[i].is_eligible_for_chru = (towns[i].population > CHRU_POP_THRESHOLD) ? 1 : 0;

        // Indices cohérents avec le remplissage de la grille : ligne depuis y, colonne depuis x
        int r = (int)((towns[i].y - minY) / CELL_SIZE);
        int c = (int)((towns[i].x - minX) / CELL_SIZE);
        // On s'assure que les indices restent dans les bornes valides
        if (r < 0) r = 0; else if (r >= rows) r = rows - 1;
        if (c < 0) c = 0; else if (c >= cols) c = cols - 1;

        // On crée un tampon temporaire pour stocker les voisins trouvés
        int tempNeighbors[MAX_NEIGHBORS]; // Une ville a rarement plus de 500 voisines à 10km
        int foundCount = 0;

        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int tr = r + dr; int tc = c + dc;
                if (tr >= 0 && tr < rows && tc >= 0 && tc < cols) {
                    Cell* cell = &grid[tr][tc];
                    for (int k = 0; k < cell->count; k++) {
                        int j = cell->indexArray[k];
                        if (i == j) continue;

                        // On convertit la LATITUDE (y) en radians pour le cosinus
                        float latitudeInRadians = towns[i].y * LAT_TO_RAD; 

                        // Le cosinus s'applique sur l'axe X (Longitude) !
                        float distanceX = (towns[i].x - towns[j].x) * LATITUDE_FACTOR * cos(latitudeInRadians);
                        float distanceY = (towns[i].y - towns[j].y) * LATITUDE_FACTOR; // 1 degré de latitude ≈ 111.32 km, on peut aussi utiliser une constante

                        float distSq = distanceX*distanceX + distanceY*distanceY;

                        if (distSq <= 100.0 && foundCount < MAX_NEIGHBORS) { // 10km au carré
                            tempNeighbors[foundCount++] = j;
                        }
                    }
                }
            }
        }

        // Allocation finale et copie
        if (foundCount > 0) {
            data[i].neighbor_count = foundCount;
            data[i].neighbors = malloc(foundCount * sizeof(int));
            for(int n=0; n<foundCount; n++) data[i].neighbors[n] = tempNeighbors[n];
        }

        data[i].max_covered_population = towns[i].population;
        for(int v = 0; v < data[i].neighbor_count; v++) {
            data[i].max_covered_population += towns[data[i].neighbors[v]].population;
        }
    }

    // Nettoyage de la grille temporaire
    for(int i=0; i<rows; i++) { if(grid[i]->indexArray) free(grid[i]->indexArray); }
    for(int i=0; i<rows; i++) free(grid[i]);
    free(grid);


    return data;
}

void fitness(Individual* restrict ind, Town* restrict towns, OptimizedData* restrict data, size_t count, unsigned char* restrict coverage_buffer) {
    ind->hospitals_count = 0;
    ind->chru_count = 0;
    ind->beds_count = 0;
    
    // On part du principe que TOUT le monde est au désert
    // (Utilise la constante de population totale de ton fichier)
    long covered_population = 0; 
    memset(coverage_buffer, 0, count);

    for (size_t i = 0; i < count; i++) {
        if (ind->genes[i]) {
            ind->hospitals_count++;
            if (data[i].is_eligible_for_chru) ind->chru_count++;

            long hospitals_bed_count = 0;

            // Si pas encore couverte, on ajoute sa pop
            if (!coverage_buffer[i]) {
                coverage_buffer[i] = 1;
                covered_population += towns[i].population;
                hospitals_bed_count += towns[i].population;
            }

            // Vectorize neighbor loop
            #pragma omp simd
            for (int v = 0; v < data[i].neighbor_count; v++) {
                int neighbor_index = data[i].neighbors[v];
                if (!coverage_buffer[neighbor_index]) {
                    coverage_buffer[neighbor_index] = 1;
                    covered_population += towns[neighbor_index].population;
                    hospitals_bed_count += towns[neighbor_index].population;
                }
            }

            // 5.4 lits pour 1000 habitants
            ind->beds_count += (long)(hospitals_bed_count * (BEDS_PER_1000 / 1000.0));
        }
    }

    ind->desert_population = HABITANTS_TOTAL - covered_population; // 65M - les gens qu'on a sauvés
    ind->fitness = HABITANTS_TOTAL - (double)ind->desert_population - (HOSPITAL_COST * ind->hospitals_count) + (CHRU_BONUS * ind->chru_count);
}
void quick_sort_population(Individual* pop, int left, int right) {
    if (left >= right) return;

    double pivot = pop[right].fitness;
    int i = left - 1;

    for (int j = left; j < right; j++) {
        if (pop[j].fitness > pivot) { // Tri décroissant
            i++;
            Individual temp = pop[i];
            pop[i] = pop[j];
            pop[j] = temp;
        }
    }
    Individual temp = pop[i + 1];
    pop[i + 1] = pop[right];
    pop[right] = temp;

    quick_sort_population(pop, left, i);
    quick_sort_population(pop, i + 2, right);
}

// Copie un individu vers un autre
void copy_individual(Individual* dest, const Individual* src, size_t count) {
    dest->fitness = src->fitness;
    dest->hospitals_count = src->hospitals_count;
    dest->chru_count = src->chru_count;
    dest->desert_population = src->desert_population;
    dest->beds_count = src->beds_count;
    // On copie les gènes (mémoire déjà allouée)
    memcpy(dest->genes, src->genes, count * sizeof(unsigned char));
}


void mutate(Individual* ind, const OptimizedData* data, size_t count, unsigned int* seed) {
    
    // 1. AJOUT MASSIF (On force l'exploration)
    int nb_ajouts = (rand_r(seed) % (MAX_DISCOVER_HOSPITALS - MIN_DISCOVER_HOSPITALS + 1)) + MIN_DISCOVER_HOSPITALS; // On ajoute de 5 à 19 hôpitaux
    for (int m = 0; m < nb_ajouts; m++) {
        int r = rand_r(seed) % count;
        // On vérifie toujours que la zone PEUT être rentable théoriquement
        if (data[r].max_covered_population >= HOSPITAL_COST) {
            ind->genes[r] = 1;
        }
    }

    // 2. ÉLAGAGE MASSIF (On autorise à fermer les CHRU doublons !)
    if (rand_r(seed) % 100 < PROB_DELETE) { // PROB_DELETE% de chances de nettoyer
        int retryAttempts = 0;
        int closeCount = 0;
        int maxClosures = (rand_r(seed) % (MAX_DELETE_HOSPITALS - MIN_DELETE_HOSPITALS + 1)) + MIN_DELETE_HOSPITALS; // On tente d'en fermer 5 à 19

        while (closeCount < maxClosures && retryAttempts < MAX_TRY) {
            int r = rand_r(seed) % count;
            
            // On ferme N'IMPORTE QUEL hôpital, même si c'est un potentiel CHRU !
            // La fitness décidera si c'était une bonne idée ou non.
            if (ind->genes[r] == 1) {
                ind->genes[r] = 0; 
                closeCount++;
            }
            retryAttempts++;
        }
    }
}

void crossover(Individual* enfant, const Individual* p1, const Individual* p2, size_t count, unsigned int* seed) {
    int pivot1 = rand_r(seed) % (count / 2);
    int pivot2 = pivot1 + (rand_r(seed) % (count / 2));

    // Parent 1 (Début)
    memcpy(enfant->genes, p1->genes, pivot1);
    // Parent 2 (Milieu)
    memcpy(enfant->genes + pivot1, p2->genes + pivot1, pivot2 - pivot1);
    // Parent 1 (Fin)
    memcpy(enfant->genes + pivot2, p1->genes + pivot2, count - pivot2);
}

GAResult run_genetic_algorithm(const GAContext* ctx, int pop_size, int gen_max, int elitism_count) {
    Town* towns = ctx->towns;
    size_t count = ctx->count;
    OptimizedData* precalc_data = ctx->precalc_data;
    unsigned char** workspaces = ctx->workspaces;

    GAResult result;
    result.best.genes = NULL; // Sentinelle : NULL signale un échec à l'appelant
    result.best_fitness_history = NULL;
    result.worst_fitness_history = NULL;
    result.generations = 0;

    // --- Garde-fous sur les paramètres (saisie utilisateur potentiellement invalide) ---
    if (pop_size < 2) pop_size = DEFAULT_POP_SIZE;
    if (gen_max < 1) gen_max = DEFAULT_GEN_MAX;
    // Au moins 1 élite pour garantir que le meilleur individu est conservé d'une génération à l'autre
    if (elitism_count < 1) elitism_count = 1;
    if (elitism_count > pop_size / 2) elitism_count = pop_size / 2;

    // Historique de fitness (une valeur par génération) pour tracer la courbe a posteriori.
    // En cas d'échec d'allocation, on continue sans historique (la courbe ne sera pas tracée).
    double* best_history = malloc(gen_max * sizeof(double));
    double* worst_history = malloc(gen_max * sizeof(double));
    if (best_history == NULL || worst_history == NULL) {
        free(best_history);
        free(worst_history);
        best_history = NULL;
        worst_history = NULL;
    }

    printf("Lancement de l'algorithme genetique (population=%d, generations=%d, elites=%d)\n",
           pop_size, gen_max, elitism_count);

    Individual* population = malloc(pop_size * sizeof(Individual));
    Individual* next_population = malloc(pop_size * sizeof(Individual));
    if (population == NULL || next_population == NULL) {
        perror("Echec d'allocation des tableaux de population");
        free(population);
        free(next_population);
        free(best_history);
        free(worst_history);
        return result;
    }

    int allocated = 0;
    for (int i = 0; i < pop_size; i++) {
        population[i].genes = calloc(count, sizeof(unsigned char));
        next_population[i].genes = calloc(count, sizeof(unsigned char));
        if (population[i].genes == NULL || next_population[i].genes == NULL) {
            perror("Echec d'allocation des genes");
            free(population[i].genes);
            free(next_population[i].genes);
            break;
        }
        allocated++;
    }
    if (allocated < pop_size) {
        for (int j = 0; j < allocated; j++) {
            free(population[j].genes);
            free(next_population[j].genes);
        }
        free(population);
        free(next_population);
        free(best_history);
        free(worst_history);
        return result;
    }

    // --- Amorçage de la population ---
    for (int i = 0; i < pop_size; i++) {
        for (size_t g = 0; g < count; g++) {
            if (precalc_data[g].max_covered_population > STARTING_HOSPITALS_THRESHOLD && (rand() % 100 < STARTING_HOSPITALS_THRESHOLD_PROBABILITY)) {
                population[i].genes[g] = 1;
            } else {
                population[i].genes[g] = (rand() % 1000 < STARTING_HOSPITALS_PROBABILITY) ? 1 : 0;
            }
        }
    }

    for (int gen = 0; gen < gen_max; gen++) {
        // --- ÉTAPE 1 : FITNESS ---
        double t1 = omp_get_wtime();
        #pragma omp parallel for schedule(dynamic, ITERATIONS_PER_THREAD)
        for (int i = 0; i < pop_size; i++) {
            fitness(&population[i], towns, precalc_data, count, workspaces[omp_get_thread_num()]);
        }
        double t2 = omp_get_wtime();

        // --- ÉTAPE 2 : TRI ---
        quick_sort_population(population, 0, pop_size - 1);
        double t3 = omp_get_wtime();

        // Enregistre la meilleure et la pire fitness de cette génération (population triée)
        if (best_history) best_history[gen] = population[0].fitness;
        if (worst_history) worst_history[gen] = population[pop_size - 1].fitness;

        // Tracé temps réel de la courbe sous la carte (les hôpitaux ne sont pas redessinés ici)
        draw_fitness_point(gen, gen_max, population[0].fitness, population[pop_size - 1].fitness);

        // Copie de l'elite vers le buffer de prochaine generation.
        for (int i = 0; i < elitism_count; i++) {
            copy_individual(&next_population[i], &population[i], count);
        }

        // --- ÉTAPE 3 : REPRODUCTION & MUTATION ---
        #pragma omp parallel
        {
            // On crée une graine unique par thread et par génération
            unsigned int seed = SEED_OFFSET + omp_get_thread_num() * 100 + gen;

            #pragma omp for schedule(static)
            for (int i = elitism_count; i < pop_size; i++) {

                // On fait du tournoi pour sélectionner les parents (TOURNAMENT_PROBABILITY% de chances), sinon du random pur
                if (rand_r(&seed) % 100 < TOURNAMENT_PROBABILITY) {
                    // Sélectionne TOURNAMENT_SIZE candidats pour chaque parent et prend le meilleur
                    int p1 = -1, p2 = -1;
                    double best_fitness_p1 = -1e100, best_fitness_p2 = -1e100;
                    for (int t = 0; t < TOURNAMENT_SIZE; t++) {
                        int idx = rand_r(&seed) % (pop_size / 2);
                        if (population[idx].fitness > best_fitness_p1 || p1 == -1) {
                            p1 = idx;
                            best_fitness_p1 = population[idx].fitness;
                        }
                    }
                    for (int t = 0; t < TOURNAMENT_SIZE; t++) {
                        int idx = rand_r(&seed) % (pop_size / 2);
                        if (population[idx].fitness > best_fitness_p2 || p2 == -1) {
                            p2 = idx;
                            best_fitness_p2 = population[idx].fitness;
                        }
                    }
                    crossover(&next_population[i], &population[p1], &population[p2], count, &seed);
                } else {
                    int random_pool_size = (RANDOM_PARENT_POOL_SIZE < pop_size) ? RANDOM_PARENT_POOL_SIZE : pop_size;
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

        // Affichage des chronos toutes les PRINT_EVERY_X_GEN générations (pour ne pas polluer le terminal)
        if (gen % PRINT_EVERY_X_GEN == 0) {
            printf("\n--- Gen %d ---\n", gen);
            printf("Fitness: %.3fs | Tri: %.3fs | Repro: %.3fs | Total: %.3fs\n",
                    t2 - t1, t3 - t2, t4 - t3, t4 - t1);
            printf("Meilleure Fitness: %.0f (Desert: %ld) Hopitaux: %d CHRU: %d Lits_Total: %ld\n",
                    population[0].fitness, population[0].desert_population,
                    population[0].hospitals_count, population[0].chru_count, population[0].beds_count);
        }
    }

    // Le meilleur individu (élite conservée) est en tête de population après la dernière bascule.
    result.best.genes = malloc(count * sizeof(unsigned char));
    if (result.best.genes != NULL) {
        copy_individual(&result.best, &population[0], count);
        result.best_fitness_history = best_history;
        result.worst_fitness_history = worst_history;
        result.generations = gen_max;
    } else {
        perror("Echec d'allocation du resultat");
        free(best_history);
        free(worst_history);
    }

    for (int i = 0; i < pop_size; i++) {
        free(population[i].genes);
        free(next_population[i].genes);
    }
    free(population);
    free(next_population);

    return result;
}