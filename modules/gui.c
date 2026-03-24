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

/*void render_point(float x, float y) {
    //MLV_draw_filled_circle((x*100)-400, y*50, 2, MLV_COLOR_RED);
    MLV_draw_filled_circle(x, y, 1, MLV_COLOR_RED);
    return;
}

void render_hospital(float x, float y) {
    MLV_draw_filled_circle(x, y, 6, MLV_COLOR_GREEN);
    return;
}*/

void create_cloud(Commune *communes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        float x = communes[i].x; 
        float y = communes[i].y; 

        // Calcul direct pour du 800x600
        int screen_x = (int)((x + 5.5) * 41.3 + 90);
        int screen_y = (int)((51.5 - y) * 60);

        MLV_draw_filled_circle(screen_x, screen_y, 0.5, MLV_COLOR_RED);
    }
    MLV_actualise_window();
    return;
}

void draw_hospitals(Hopital *hopitaux, size_t count) {
    MLV_Color hospital_color = MLV_rgba(0, 255, 0, 128);
    for (size_t i = 0; i < count; i++) {
        float x = hopitaux[i].x;
        float y = hopitaux[i].y;

        // Même formule de projection que les communes
        int screen_x = (int)((x + 5.5) * 41.3 + 90);
        int screen_y = (int)((51.5 - y) * 60);

        // Cercle vert de rayon 2 pour les distinguer des communes (rouges)
        MLV_draw_filled_circle(screen_x, screen_y, 6, hospital_color);
    }
    MLV_actualise_window();
}



void close_window(void) {
    MLV_free_window();
    return;
}