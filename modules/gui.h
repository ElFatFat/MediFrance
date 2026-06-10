#ifndef GUI_H
#define GUI_H

#include "../main.h"
#include "gen.h"
#include <MLV/MLV_all.h>

/**
 * @file gui.h
 * @brief Fonctions d'interface graphique et d'affichage cartographique via la LibMLV.
 */

void init_window(void);
void close_window(void);
void create_cloud(Town *communes, size_t count);
void draw_hospitals(Town *communes, size_t count, Individual ind, const OptimizedData* data);
void draw_stats(Individual ind);
void draw_fitness_point(int gen, int nb_gen, double best_fitness, double worst_fitness);
void draw_fitness_curve(const double* best_history, const double* worst_history, int nb_gen);
void settings_menu(Town *communes, size_t count, GAResult* res, const GAContext* ctx);

#endif
