#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>

/**
 * @file main.h
 * @brief Modélisation pivot des structures de données géographiques globales du projet.
 * @author Votre Nom
 * @date 2026
 *
 * Ce fichier d'en-tête maître fournit les définitions de types de base partagées 
 * par l'ensemble des modules du moteur génétique, des modules géospatiaux et d'affichage.
 */

/**
 * @struct Town
 * @brief Représente l'enregistrement complet d'une entité administrative communale française.
 * * Cette structure stocke les attributs textuels et numériques extraits du fichier CSV de référence 
 * ainsi que les coordonnées cartographiques bidimensionnelles projetées pour les calculs de distances.
 */
typedef struct Town {
    int insee_code;
    char name[50];
    int region;
    char region_name[50];
    int departement;
    char department_name[50];
    int postal_code;
    int population;
    float x;
    float y;
} Town;

#endif