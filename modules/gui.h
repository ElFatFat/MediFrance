#ifndef GUI_H
#define GUI_H

#include "../main.h"
#include "gen.h"
#include <MLV/MLV_all.h>

/**
 * @file gui.h
 * @brief Fonctions d'interface graphique et d'affichage cartographique via la LibMLV.
 */

/**
 * @brief Initialise et configure la fenêtre principale de l'application.
 */
void init_window(void);

/**
 * @brief Ferme proprement la fenêtre graphique et libère la mémoire MLV.
 */
void close_window(void);

/**
 * @brief Projette et affiche un point isolé sur la carte.
 * @param x Coordonnée longitude (X)
 * @param y Coordonnée latitude (Y)
 */
void render_point(float x, float y);

/**
 * @brief Génère le nuage de points représentant la carte des communes de France.
 * @param communes Tableau des structures communes à dessiner.
 * @param count    Nombre total de communes.
 */
void create_cloud(Town *communes, size_t count);

/*void render_point(float x, float y);*/
/*void draw_hospitalsCSV(Hopital *hopitaux, size_t count);*/

/**
 * @brief Dessine des cercles translucides représentant les hôpitaux actifs d'une solution.
 * @param communes Tableau des communes.
 * @param count    Nombre total de communes.
 * @param ind      L'individu contenant les gènes d'activation des hôpitaux.
 */
void draw_hospitals(Town *communes, size_t count, Individual ind);

/**
 * @brief Dessine un graphique représentant la progression temporelle de la fitness.
 * @param fitness Valeur de fitness calculée.
 * @param nb_gen  Nombre total attendu de générations (pour l'échelle X).
 * @param index   Index de la génération courante.
 * @param color   Couleur associée à la courbe.
 */
void fitness_graph(double fitness, int nb_gen, int index, MLV_Color color);

/**
 * @brief Gère l'affichage du menu des options et capture les interactions utilisateur.
 * @param communes Tableau des communes.
 * @param count    Nombre total de communes.
 * @param ind      Individu initial/actuel à manipuler graphiquement.
 */
void settings_menu(Town *communes, size_t count, Individual ind);

#endif