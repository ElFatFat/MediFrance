#ifndef GUI_H
#define GUI_H

#include "../main.h"
#include <MLV/MLV_all.h>

void init_window(void);
void close_window(void);
void render_point(float x, float y);
void create_cloud(Commune *communes, size_t count);

#endif