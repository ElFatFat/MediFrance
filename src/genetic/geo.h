#ifndef GEO_H
#define GEO_H

#include <stddef.h>
#include "../../main.h"

/**
 * @file geo.h
 * @brief Modélisation géospatiale : distances entre communes et voisinages précalculés.
 */

#define COVERAGE_RADIUS_KM 10.0f
#define CELL_SIZE 0.1f

/**
 * @struct OptimizedData
 * @brief Structure regroupant les données de voisinage précalculées à moins de 10km.
 */
typedef struct {
    int* neighbors;            ///< Tableau d'indices des communes situées à moins de 10km
    int neighbor_count;        ///< Nombre total de communes voisines trouvées
    int is_eligible_for_chru;  ///< Indicateur d'éligibilité CHRU : 1 si population > 80 000, sinon 0
    int max_covered_population;///< Population maximale théoriquement couverte par ce nœud
} OptimizedData;

float town_distance_km(const Town* a, const Town* b);

/**
 * @brief Effectue la sectorisation spatiale par grille de hachage et liste les communes à < 10km.
 */
OptimizedData* precalc_near(Town* restrict towns, size_t count);

#endif /* GEO_H */
