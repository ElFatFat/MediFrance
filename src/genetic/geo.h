#ifndef GEO_H
#define GEO_H

#include <stddef.h>
#include "../../main.h"

/**
 * @file geo.h
 * @brief Modélisation géospatiale : calculs de distances et grille de hachage spatial.
 *
 * Ce module regroupe les structures mathématiques et géographiques indispensables 
 * au traitement spatial. Il gère la projection simplifiée de coordonnées et le maillage
 * du territoire pour accélérer les requêtes de proximité (voisinage).
 */

/**
 * @def COVERAGE_RADIUS_KM
 * @brief Rayon de couverture réglementaire d'un établissement de santé (ex: 10.0 km).
 */
#define COVERAGE_RADIUS_KM 10.0f

/**
 * @def CELL_SIZE
 * @brief Dimension angulaire (en degrés) d'une cellule de la grille de hachage.
 * * Une valeur de 0.1° correspond à environ 11 km sur l'axe des latitudes, ce qui est 
 * idéal pour discrétiser des voisinages de 10 km.
 */
#define CELL_SIZE 0.1f

/**
 * @struct OptimizedData
 * @brief Structure regroupant les métriques géospatiales précalculées d'une commune.
 * * Contient des listes d'adjacence optimisées permettant à la fonction de fitness 
 * de s'exécuter en un temps minimal sans refaire de calculs trigonométriques.
 */
typedef struct {
    int* neighbors;            /**< Tableau d'indices (ID internes) des communes situées à moins de 10km. */
    int neighbor_count;        /**< Nombre exact de communes voisines indexées dans le tableau. */
    int is_eligible_for_chru;  /**< Indicateur d'éligibilité au statut CHRU (1 si la commune seule > 80 000 habitants, 0 sinon). */
    int max_covered_population;/**< Somme cumulée des populations de la commune et de tous ses voisins directs. */
} OptimizedData;

/**
 * @brief Calcule la distance physique en kilomètres entre deux coordonnées géographiques.
 * * Utilise une approximation plane par projection sinusoïdale (méthode de la loxodromie moyenne),
 * beaucoup plus rapide à calculer sur des milliers de points que la formule de la trigonométrie sphérique (Haversine).
 *
 * @param a Pointeur vers la commune d'origine.
 * @param b Pointeur vers la commune de destination.
 * @return  La distance réelle estimée en kilomètres entre les deux points.
 */
float town_distance_km(const Town* a, const Town* b);

/**
 * @brief Sectorise le territoire national et extrait le voisinage restreint de chaque commune.
 * * Cette fonction orchestre la grille de hachage spatial. Elle crée un maillage 2D dynamique, 
 * y distribue l'ensemble des communes de France, puis utilise un filtre de recherche localisé (fenêtre de cellules)
 * pour n'évaluer la distance qu'entre les communes géographiquement proches.
 *
 * @note Cette fonction applique l'optimisation de pointeurs de non-recouvrement via le mot-clé `restrict`.
 *
 * @param towns Tableau contenant la totalité des structures de communes chargées en mémoire.
 * @param count Nombre total de communes indexées.
 * @return      Un tableau alloué de structures OptimizedData (taille égale à `count`), ou NULL en cas d'erreur système.
 */
OptimizedData* precalc_near(Town* restrict towns, size_t count);

#endif /* GEO_H */