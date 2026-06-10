#ifndef LOADER_H
#define LOADER_H

#include <stddef.h>
#include "../main.h"

/**
 * @file loader.h
 * @brief Chargement des communes depuis le fichier CSV source.
 */

/**
 * @brief Charge les communes depuis un CSV (une ligne par commune, champs séparés par des virgules).
 *
 * Les lignes ne contenant pas les 10 champs attendus sont ignorées.
 *
 * @param path   Chemin du fichier CSV à lire.
 * @param count  En sortie : nombre de communes chargées.
 * @return       Tableau alloué de communes (à libérer par l'appelant), NULL en cas d'erreur.
 */
Town* load_towns_csv(const char* path, size_t* count);

#endif /* LOADER_H */
