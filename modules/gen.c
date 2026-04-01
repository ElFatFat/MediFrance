#include "gen.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>


#define CELL_SIZE 10000.0 // 10 km (Lambert 93 en metres)
#define HABITANTS_TOTAL 65141355 // Population totale de la France (constante pour le fitness)
#define HOSPITAL_COST 5000.0
#define CHRU_BONUS 4000.0
#define CHRU_POP_THRESHOLD 80000
#define BEDS_PER_1000 5.4 // Nombre de lits pour 1000 habitants

#define PROB_DELETE 80 // 80% de chances de supprimer un bâtiment lors de la mutation intelligente
#define MAX_DISCOVER_HOSPITALS 10 // Nombre maximum d'hôpitaux à découvrir lors de la mutation intelligente (pour limiter les changements drastiques)
#define MIN_DISCOVER_HOSPITALS 3 // Nombre minimum d'hôpitaux à découvrir lors de la mutation intelligente (pour assurer une exploration suffisante)
#define MAX_DELETE_HOSPITALS 10 // Nombre maximum d'hôpitaux à supprimer lors de la mutation intelligente (pour limiter les changements drastiques)
#define MIN_DELETE_HOSPITALS 3 // Nombre minimum d'hôpitaux à supprimer lors de la mutation intelligente (pour assurer une exploration suffisante)

#define MAX_NEIGHBORS 200 // Nombre maximum de voisins à stocker pour chaque commune (pour limiter la mémoire)

#define MAX_TRY 150 // Nombre maximum de tentatives pour trouver un hôpital à fermer lors de la mutation intelligente (pour éviter les boucles infinies)

// Projection Lambert 93 (EPSG:2154) depuis coordonnees geographiques (WGS84/RGF93).
static inline void project_to_lambert93(double lat_deg, double lon_deg, double* x, double* y) {
    const double deg_to_rad = M_PI / 180.0;
    const double n = 0.7256077650532670;
    const double c = 11754255.426096;
    const double xs = 700000.0;
    const double ys = 12655612.049876;
    const double lon0 = 3.0 * deg_to_rad;
    const double e = 0.081819191042816;

    const double phi = lat_deg * deg_to_rad;
    const double lambda = lon_deg * deg_to_rad;
    const double sin_phi = sin(phi);

    const double lat_iso = log(tan(M_PI_4 + (phi * 0.5)) *
                               pow((1.0 - e * sin_phi) / (1.0 + e * sin_phi), e * 0.5));
    const double r = c * exp(-n * lat_iso);
    const double gamma = n * (lambda - lon0);

    *x = xs + r * sin(gamma);
    *y = ys - r * cos(gamma);
}

OptimizedData* precalc_near(Town* restrict towns, size_t count) {
    if (towns == NULL || count == 0) {
        return NULL;
    }

    double* lambert_x = malloc(count * sizeof(double));
    double* lambert_y = malloc(count * sizeof(double));
    if (lambert_x == NULL || lambert_y == NULL) {
        perror("Erreur d'allocation Lambert 93");
        free(lambert_x);
        free(lambert_y);
        return NULL;
    }

    // Initialise les bornes avec la premiere commune projetee.
    project_to_lambert93((double)towns[0].x, (double)towns[0].y, &lambert_x[0], &lambert_y[0]);
    double minX = lambert_x[0];
    double maxX = lambert_x[0];
    double minY = lambert_y[0];
    double maxY = lambert_y[0];

    for (size_t i = 1; i < count; i++) {
        // CSV: x = latitude, y = longitude.
        project_to_lambert93((double)towns[i].x, (double)towns[i].y, &lambert_x[i], &lambert_y[i]);

        if(lambert_x[i] < minX) minX = lambert_x[i];
        if(lambert_x[i] > maxX) maxX = lambert_x[i];
        if(lambert_y[i] < minY) minY = lambert_y[i];
        if(lambert_y[i] > maxY) maxY = lambert_y[i];
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
        int c = (int)((lambert_x[i] - minX) / CELL_SIZE);
        int r = (int)((lambert_y[i] - minY) / CELL_SIZE);

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

    for (int i = 0; i < (int)count; i++) {
        data[i].neighbor_count = 0;
        data[i].neighbors = NULL;
        data[i].is_eligible_for_chru = (towns[i].population > CHRU_POP_THRESHOLD) ? 1 : 0;

        // Indices cohérents avec le remplissage de la grille : ligne depuis y, colonne depuis x
        int r = (int)((lambert_y[i] - minY) / CELL_SIZE);
        int c = (int)((lambert_x[i] - minX) / CELL_SIZE);
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

                        double distanceX = lambert_x[i] - lambert_x[j];
                        double distanceY = lambert_y[i] - lambert_y[j];
                        double distSq = distanceX * distanceX + distanceY * distanceY;

                        if (distSq <= 100000000.0 && foundCount < MAX_NEIGHBORS) { // 10km au carre
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
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++) {
            if (grid[i][j].indexArray) free(grid[i][j].indexArray);
        }
    }
    for(int i=0; i<rows; i++) free(grid[i]);
    free(grid);
    free(lambert_x);
    free(lambert_y);

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