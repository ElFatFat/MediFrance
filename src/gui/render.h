#ifndef RENDER_H
#define RENDER_H

#include "../../main.h"
#include "../genetic/gen.h"
#include <MLV/MLV_all.h>

/**
 * @file render.h
 * @brief Fonctions de rendu via la LibMLV : fenêtre, carte des communes, hôpitaux, statistiques et courbe de fitness.
 */

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

void init_window(void);
void close_window(void);
void create_cloud(Town *communes, size_t count);
void draw_hospitals(Town *communes, size_t count, Individual ind, const OptimizedData* data);
void draw_stats(Individual ind);
void draw_fitness_point(int gen, int nb_gen, double best_fitness, double worst_fitness);
void draw_fitness_curve(const double* best_history, const double* worst_history, int nb_gen);

#endif /* RENDER_H */
