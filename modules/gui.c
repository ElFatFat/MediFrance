#include "gui.h"



#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

void init_window(void) {
    MLV_create_window("MediFrance", "MediFrance", WINDOW_WIDTH, WINDOW_HEIGHT);

    MLV_clear_window(MLV_COLOR_BLACK);
    MLV_draw_text(30, 30, "MediFrance - Basic LibMLV window", MLV_COLOR_WHITE);
    MLV_actualise_window();
    return;
}

void render_point(float x, float y) {
    MLV_draw_filled_circle((x*100)-400, y*50, 2, MLV_COLOR_RED);
    return;
}

void create_cloud(Commune *communes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        float x = communes[i].x;
        float y = communes[i].y;
        render_point(x, y);
    }
    MLV_actualise_window();
    return;
}

void close_window(void) {
    MLV_free_window();
    return;
}