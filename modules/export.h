#ifndef EXPORT_H
#define EXPORT_H

#include "../main.h"
#include "gen.h"

/**
 * @file export.h
 * @brief Gestion de l'exportation des données de simulation sous format CSV.
 */

/**
 * @brief Exporte une ligne par commune dans un CSV plat consommé par le script Python.
 *
 * Colonnes produites (une ligne par commune) :
 * nom_dep       — nom du département
 * ville         — nom de la commune
 * population    — population de la commune
 * has_hospital  — 1 si un hôpital est placé ici, 0 sinon
 * is_desert     — 1 si la commune n'est couverte par aucun hôpital, 0 sinon
 * nb_beds       — nombre de lits de l'hôpital (0 si pas d'hôpital dans cette commune)
 *
 * @param path          Chemin du fichier CSV à créer
 * @param towns         Tableau de toutes les communes
 * @param count         Nombre de communes
 * @param precalc_data  Données précalculées (neighbors, populations couvertes)
 * @param best          Meilleur individu issu de l'algorithme génétique
 * @return              0 en cas de succès, -1 en cas d'erreur
 */
int export_resultats_csv(const char* path,
                         const Town* towns,
                         size_t count,
                         const OptimizedData* precalc_data,
                         const Individual* best);

#endif /* EXPORT_H */