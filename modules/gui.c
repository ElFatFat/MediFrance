#include "gui.h"
#include "gen.h"



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

void draw_hospitalsCSV(Hopital *hopitaux, size_t count) {
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

void draw_hospitals(Commune *communes, size_t count, Individu ind) {
    MLV_Color hospital_color = MLV_rgba(0, 255, 0, 128);
    for (size_t i = 0; i < count; i++) {
        if(ind.genes[i]==1) {
            float x = communes[i].x;
            float y = communes[i].y;

            // Même formule de projection que les communes
            int screen_x = (int)((x + 5.5) * 41.3 + 90);
            int screen_y = (int)((51.5 - y) * 60);

            // Cercle vert de rayon 2 pour les distinguer des communes (rouges)
            MLV_draw_filled_circle(screen_x, screen_y, 6, hospital_color);
    }
    MLV_actualise_window();

    }
}

/*void fitness_graph(double fitness,int nb_gen, int index){
    //MLV_draw_line(10 + index*5, 590 - (int)(fitness/100000), 10 + (index+1)*5, 590 - (int)(fitness/100000), MLV_COLOR_BLUE);
    MLV_draw_line(WINDOW_WIDTH/nb_gen*index, 0, WINDOW_WIDTH/nb_gen*index, fitness-40000000, MLV_COLOR_BLUE);
    MLV_actualise_window();
}*/

/*void fitness_graph(double fitness, int nb_gen, int index, MLV_Color color) {
    //if (fitness < 40000000) fitness = 40000000; 
    float ratio = (fitness - 40000000.0) / 10000000.0;
    int y = (int)(WINDOW_HEIGHT - (ratio * WINDOW_HEIGHT));

    MLV_draw_line(WINDOW_WIDTH/nb_gen+1*index, WINDOW_HEIGHT, WINDOW_WIDTH/nb_gen+1*index, y, color);
    MLV_actualise_window();
}*/

void fitness_graph(double fitness, int nb_gen, int index, MLV_Color color) {
    if (fitness > 65000000.0) fitness = 65000000.0; 
    if (fitness < 40000000.0) fitness = 40000000.0;

    float ratio = (fitness - 40000000.0) / 25000000.0;
    int y = (int)(WINDOW_HEIGHT - (ratio * WINDOW_HEIGHT));

    int x = (WINDOW_WIDTH / nb_gen) + index;

    MLV_draw_line(x, WINDOW_HEIGHT, x, y, color);
    
    MLV_actualise_window();
}

void close_window(void) {
    MLV_free_window();
    return;
}
