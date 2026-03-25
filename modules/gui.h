#ifndef GUI_H
#define GUI_H

#include "../main.h"
#include "gen.h"
#include <MLV/MLV_all.h>

void init_window(void);
void close_window(void);
void render_point(float x, float y);
void create_cloud(Commune *communes, size_t count);
void draw_hospitalsCSV(Hopital *hopitaux, size_t count);
void draw_hospitals(Commune *communes, size_t count, Individu ind);
void fitness_graph(double fitness, int nb_gen, int index, MLV_Color color);


#endif