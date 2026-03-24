#include "gen.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>


#define CELL_SIZE 0.1 // Environ 11km, parfait pour un rayon de 10km

DataOptimisee* precalc_near(Commune* communes, size_t count) {
    // --- ÉTAPE 1 : Trouver les bornes et créer la grille ---
    float minX = communes[0].x, maxX = communes[0].x;
    float minY = communes[0].y, maxY = communes[0].y;
    for(size_t i=1; i<count; i++) {
        if(communes[i].x < minX) minX = communes[i].x;
        if(communes[i].x > maxX) maxX = communes[i].x;
        if(communes[i].y < minY) minY = communes[i].y;
        if(communes[i].y > maxY) maxY = communes[i].y;
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
        int c = (int)((communes[i].x - minX) / CELL_SIZE);
        int r = (int)((communes[i].y - minY) / CELL_SIZE);

        // Sécurité anti-débordement (si x == maxX ou y == maxY)
        if (c >= cols) c = cols - 1;
        if (r >= rows) r = rows - 1;
        if (c < 0) c = 0;
        if (r < 0) r = 0;

        Cell* cell = &grid[r][c]; // On accède à grid[row][col]

        if (cell->count >= cell->capacity) {
            // Initialisation de capacity si c'est le premier passage
            int new_cap = (cell->capacity == 0) ? 10 : cell->capacity * 2;
            int* new_indices = realloc(cell->indices, new_cap * sizeof(int));
            
            if (new_indices == NULL) {
                perror("Erreur realloc dans la grille");
                // Gérer l'erreur proprement ici
                return NULL;
            }
            
            cell->indices = new_indices;
            cell->capacity = new_cap;
        }
        cell->indices[cell->count++] = i;
    }

    // --- ÉTAPE 3 : Calculer les voisins (Double passe pour la mémoire) ---
    DataOptimisee* data = malloc(count * sizeof(DataOptimisee));

    for (int i = 0; i < (int)count; i++) {
        data[i].nb_voisins = 0;
        data[i].voisins = NULL;
        data[i].est_eligible_chru = (communes[i].population > 80000) ? 1 : 0;

        int r = (int)((communes[i].x - minX) / CELL_SIZE);
        int c = (int)((communes[i].y - minY) / CELL_SIZE);

        // On crée un tampon temporaire pour stocker les voisins trouvés
        int temp_voisins[500]; // Une ville a rarement plus de 500 voisines à 10km
        int nb_trouves = 0;

        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int tr = r + dr; int tc = c + dc;
                if (tr >= 0 && tr < rows && tc >= 0 && tc < cols) {
                    Cell* cell = &grid[tr][tc];
                    for (int k = 0; k < cell->count; k++) {
                        int j = cell->indices[k];
                        if (i == j) continue;

                        // On convertit la LATITUDE (y) en radians pour le cosinus
                        float lat_rad = communes[i].y * 0.0174533; 

                        // Le cosinus s'applique sur l'axe X (Longitude) !
                        float dx = (communes[i].x - communes[j].x) * 111.32 * cos(lat_rad);
                        float dy = (communes[i].y - communes[j].y) * 111.32; 

                        float distSq = dx*dx + dy*dy;

                        if (distSq <= 100.0) { // 10km au carré
                            temp_voisins[nb_trouves++] = j;
                        }
                    }
                }
            }
        }

        // Allocation finale et copie
        if (nb_trouves > 0) {
            data[i].nb_voisins = nb_trouves;
            data[i].voisins = malloc(nb_trouves * sizeof(int));
            for(int n=0; n<nb_trouves; n++) data[i].voisins[n] = temp_voisins[n];
        }

        data[i].max_pop_couverte = communes[i].population;
        for(int v = 0; v < data[i].nb_voisins; v++) {
            data[i].max_pop_couverte += communes[data[i].voisins[v]].population;
        }
    }

    // Nettoyage de la grille temporaire
    for(int i=0; i<rows; i++) { if(grid[i]->indices) free(grid[i]->indices); }
    for(int i=0; i<rows; i++) free(grid[i]);
    free(grid);

    return data;
}

void fitness(Individu* ind, Commune* communes, DataOptimisee* data, size_t count, unsigned char* couvert_local) {
    ind->nb_hopitaux = 0;
    ind->nb_chru = 0;
    
    // On part du principe que TOUT le monde est au désert
    // (Utilise la constante de population totale de ton fichier)
    long pop_couverte = 0; 
    memset(couvert_local, 0, count);

    for (size_t i = 0; i < count; i++) {
        if (ind->genes[i]) {
            ind->nb_hopitaux++;
            if (data[i].est_eligible_chru) ind->nb_chru++;

            // Si pas encore couverte, on ajoute sa pop
            if (!couvert_local[i]) {
                couvert_local[i] = 1;
                pop_couverte += communes[i].population;
            }

            for (int v = 0; v < data[i].nb_voisins; v++) {
                int idx_v = data[i].voisins[v];
                if (!couvert_local[idx_v]) {
                    couvert_local[idx_v] = 1;
                    pop_couverte += communes[idx_v].population;
                }
            }
        }
    }

    ind->hab_desert = 65141355 - pop_couverte; // 65M - les gens qu'on a sauvés
    ind->fitness = 65141355.0 - (double)ind->hab_desert - (5000.0 * ind->nb_hopitaux) + (4000.0 * ind->nb_chru);
}
void quick_sort_population(Individu* pop, int left, int right) {
    if (left >= right) return;

    double pivot = pop[right].fitness;
    int i = left - 1;

    for (int j = left; j < right; j++) {
        if (pop[j].fitness > pivot) { // Tri décroissant
            i++;
            Individu temp = pop[i];
            pop[i] = pop[j];
            pop[j] = temp;
        }
    }
    Individu temp = pop[i + 1];
    pop[i + 1] = pop[right];
    pop[right] = temp;

    quick_sort_population(pop, left, i);
    quick_sort_population(pop, i + 2, right);
}

// Copie un individu vers un autre
void copier_individu(Individu* dest, Individu* src, size_t count) {
    dest->fitness = src->fitness;
    dest->nb_hopitaux = src->nb_hopitaux;
    dest->nb_chru = src->nb_chru;
    dest->hab_desert = src->hab_desert;
    // On copie les gènes (mémoire déjà allouée)
    memcpy(dest->genes, src->genes, count * sizeof(unsigned char));
}

void muter(Individu* ind, size_t count, unsigned int* seed) {
    // 0.1% de mutations sur 36000 gènes = environ 36 mutations
    // On calcule un nombre de mutations autour de cette moyenne
    int nb_mutations = (count / 1000) + (rand_r(seed) % 10); 
    
    for (int m = 0; m < nb_mutations; m++) {
        int r = rand_r(seed) % count;
        ind->genes[r] = !ind->genes[r];
    }
}

void muter_intelligente(Individu* ind, DataOptimisee* data, size_t count, unsigned int* seed) {
    
    // 1. AJOUT MASSIF (On force l'exploration)
    int nb_ajouts = (rand_r(seed) % 15) + 5; // On ajoute de 5 à 19 hôpitaux
    for (int m = 0; m < nb_ajouts; m++) {
        int r = rand_r(seed) % count;
        // On vérifie toujours que la zone PEUT être rentable théoriquement
        if (data[r].max_pop_couverte >= 5000) {
            ind->genes[r] = 1;
        }
    }

    // 2. ÉLAGAGE MASSIF (On autorise à fermer les CHRU doublons !)
    if (rand_r(seed) % 100 < 80) { // 80% de chances de nettoyer
        int tentative = 0;
        int fermetures = 0;
        int max_fermetures = (rand_r(seed) % 15) + 5; // On tente d'en fermer 5 à 19

        while (fermetures < max_fermetures && tentative < 150) {
            int r = rand_r(seed) % count;
            
            // On ferme N'IMPORTE QUEL hôpital, même si c'est un potentiel CHRU !
            // La fitness décidera si c'était une bonne idée ou non.
            if (ind->genes[r] == 1) {
                ind->genes[r] = 0; 
                fermetures++;
            }
            tentative++;
        }
    }
}

void crossover(Individu* enfant, Individu* p1, Individu* p2, size_t count, unsigned int* seed) {
    int pivot1 = rand_r(seed) % (count / 2);
    int pivot2 = pivot1 + (rand_r(seed) % (count / 2));
    
    // Parent 1 (Début)
    memcpy(enfant->genes, p1->genes, pivot1);
    // Parent 2 (Milieu)
    memcpy(enfant->genes + pivot1, p2->genes + pivot1, pivot2 - pivot1);
    // Parent 1 (Fin)
    memcpy(enfant->genes + pivot2, p1->genes + pivot2, count - pivot2);
}