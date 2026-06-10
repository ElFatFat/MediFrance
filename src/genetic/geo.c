/**
 * @file geo.c
 * @brief Implémentation géospatiale : distances, grille de hachage spatial et voisinages précalculés.
 */

#include "geo.h"
#include "../constants.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 0.1° ≈ 11 km ; à ~48°N, 10 km en longitude ≈ 0.13° → il faut ±2 cellules */
#define CELL_SEARCH_RADIUS 2

#define LAT_TO_RAD 0.0174533f
#define LATITUDE_FACTOR 111.32f
#define MAX_NEIGHBORS 200

/**
 * @struct Cell
 * @brief Cellule dynamique utilisée pour la sectorisation de la grille de hachage spatial.
 */
typedef struct {
    int* indexArray;          ///< Indices des communes résidentes dans la cellule
    int count;                ///< Nombre effectif de communes recensées
    int capacity;             ///< Capacité maximale allouée pour le tableau dynamique
} Cell;

float town_distance_km(const Town* a, const Town* b) {
    float lat_rad = ((a->y + b->y) * 0.5f) * LAT_TO_RAD;
    float dx = (a->x - b->x) * LATITUDE_FACTOR * cosf(lat_rad);
    float dy = (a->y - b->y) * LATITUDE_FACTOR;
    return sqrtf(dx * dx + dy * dy);
}

OptimizedData* precalc_near(Town* restrict towns, size_t count) {
    float minX = towns[0].x;
    float maxX = minX;
    float minY = towns[0].y;
    float maxY = minY;
    for (size_t i = 1; i < count; i++) {
        if (towns[i].x < minX) minX = towns[i].x;
        if (towns[i].x > maxX) maxX = towns[i].x;
        if (towns[i].y < minY) minY = towns[i].y;
        if (towns[i].y > maxY) maxY = towns[i].y;
    }

    int cols = (int)((maxX - minX) / CELL_SIZE) + 1;
    int rows = (int)((maxY - minY) / CELL_SIZE) + 1;

    Cell** grid = malloc((size_t)rows * sizeof(Cell*));
    if (grid == NULL) {
        perror("Erreur d'allocation de la grille");
        return NULL;
    }
    for (int i = 0; i < rows; i++) {
        grid[i] = calloc((size_t)cols, sizeof(Cell));
        if (grid[i] == NULL) {
            perror("Erreur d'allocation de la grille");
            for (int j = 0; j < i; j++) {
                free(grid[j]);
            }
            free(grid);
            return NULL;
        }
    }

    for (int i = 0; i < (int)count; i++) {
        int c = (int)((towns[i].x - minX) / CELL_SIZE);
        int r = (int)((towns[i].y - minY) / CELL_SIZE);

        if (c >= cols) c = cols - 1;
        if (r >= rows) r = rows - 1;
        if (c < 0) c = 0;
        if (r < 0) r = 0;

        Cell* cell = &grid[r][c];

        if (cell->count >= cell->capacity) {
            int new_cap = (cell->capacity == 0) ? 10 : cell->capacity * 2;
            int* new_indices = realloc(cell->indexArray, (size_t)new_cap * sizeof(int));

            if (new_indices == NULL) {
                perror("Erreur realloc dans la grille");
                for (int rr = 0; rr < rows; rr++) {
                    if (grid[rr]->indexArray) {
                        free(grid[rr]->indexArray);
                    }
                    free(grid[rr]);
                }
                free(grid);
                return NULL;
            }

            cell->indexArray = new_indices;
            cell->capacity = new_cap;
        }
        cell->indexArray[cell->count++] = i;
    }

    OptimizedData* data = malloc(count * sizeof(OptimizedData));
    if (data == NULL) {
        perror("Erreur d'allocation des données optimisées");
        for (int rr = 0; rr < rows; rr++) {
            if (grid[rr]->indexArray) {
                free(grid[rr]->indexArray);
            }
            free(grid[rr]);
        }
        free(grid);
        return NULL;
    }

    for (int i = 0; i < (int)count; i++) {
        data[i].neighbor_count = 0;
        data[i].neighbors = NULL;
        data[i].is_eligible_for_chru = (towns[i].population > CHRU_POP_THRESHOLD) ? 1 : 0;

        int r = (int)((towns[i].y - minY) / CELL_SIZE);
        int c = (int)((towns[i].x - minX) / CELL_SIZE);
        if (r < 0) r = 0;
        else if (r >= rows) r = rows - 1;
        if (c < 0) c = 0;
        else if (c >= cols) c = cols - 1;

        int tempNeighbors[MAX_NEIGHBORS];
        int foundCount = 0;

        for (int dr = -CELL_SEARCH_RADIUS; dr <= CELL_SEARCH_RADIUS; dr++) {
            for (int dc = -CELL_SEARCH_RADIUS; dc <= CELL_SEARCH_RADIUS; dc++) {
                int tr = r + dr;
                int tc = c + dc;
                if (tr < 0 || tr >= rows || tc < 0 || tc >= cols) {
                    continue;
                }
                Cell* cell = &grid[tr][tc];
                for (int k = 0; k < cell->count; k++) {
                    int j = cell->indexArray[k];
                    if (i == j) {
                        continue;
                    }

                    float dist = town_distance_km(&towns[i], &towns[j]);
                    if (dist <= COVERAGE_RADIUS_KM && foundCount < MAX_NEIGHBORS) {
                        tempNeighbors[foundCount++] = j;
                    }
                }
            }
        }

        if (foundCount > 0) {
            data[i].neighbor_count = foundCount;
            data[i].neighbors = malloc((size_t)foundCount * sizeof(int));
            if (data[i].neighbors == NULL) {
                perror("Erreur d'allocation voisins");
                for (int n = 0; n < i; n++) {
                    free(data[n].neighbors);
                }
                free(data);
                for (int rr = 0; rr < rows; rr++) {
                    if (grid[rr]->indexArray) {
                        free(grid[rr]->indexArray);
                    }
                    free(grid[rr]);
                }
                free(grid);
                return NULL;
            }
            memcpy(data[i].neighbors, tempNeighbors, (size_t)foundCount * sizeof(int));
        }

        data[i].max_covered_population = towns[i].population;
        for (int v = 0; v < data[i].neighbor_count; v++) {
            data[i].max_covered_population += towns[data[i].neighbors[v]].population;
        }
    }

    for (int i = 0; i < rows; i++) {
        if (grid[i]->indexArray) {
            free(grid[i]->indexArray);
        }
        free(grid[i]);
    }
    free(grid);

    return data;
}
