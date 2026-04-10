#include "gui.h"
#include "gen.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

void init_window(void) {
    MLV_create_window("MediFrance", "MediFrance", WINDOW_WIDTH, WINDOW_HEIGHT);
    MLV_clear_window(MLV_COLOR_BLACK);
    MLV_draw_text(30, 30, "MediFrance - Basic LibMLV window", MLV_COLOR_WHITE);
    MLV_actualise_window();
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

void create_cloud(Town *communes, size_t count) {
    printf("Affichage de la carte...\n");
    for (size_t i = 0; i < count; i++) {
        float x = communes[i].x; 
        float y = communes[i].y; 

        int screen_x = (int)((x + 5.5) * 41.3 + 90);
        int screen_y = (int)((51.5 - y) * 60);

        if (screen_x >= 0 && screen_x < WINDOW_WIDTH && screen_y >= 0 && screen_y < WINDOW_HEIGHT) {
            MLV_draw_point(screen_x, screen_y, MLV_COLOR_RED);
        }
    }
    printf("Carte affichée...\n");

}

void draw_hospitals(Town *communes, size_t count, Individual ind) {
    MLV_Color hospital_color = MLV_rgba(0, 255, 0, 128);
    for (size_t i = 0; i < count; i++) {
        if(ind.genes[i] == 1) {
            float x = communes[i].x;
            float y = communes[i].y;

            int screen_x = (int)((x + 5.5) * 41.3 + 90);
            int screen_y = (int)((51.5 - y) * 60);

            if (screen_x >= -6 && screen_x < WINDOW_WIDTH + 6 && screen_y >= -6 && screen_y < WINDOW_HEIGHT + 6) {
                MLV_draw_filled_circle(screen_x, screen_y, 6, hospital_color);
            }
        }
    }
}

void fitness_graph(double fitness, int nb_gen, int index, MLV_Color color) {
    if (fitness > 65000000.0) fitness = 65000000.0; 
    if (fitness < 40000000.0) fitness = 40000000.0;

    float ratio = (fitness - 40000000.0) / 25000000.0;
    int y = (int)(WINDOW_HEIGHT - (ratio * WINDOW_HEIGHT));

    int x = (WINDOW_WIDTH / nb_gen) + index;

    MLV_draw_line(x, WINDOW_HEIGHT, x, y, color);
    MLV_actualise_window();
}

void draw_base_interface() {
    MLV_clear_window(MLV_COLOR_BLACK);
    
    MLV_draw_line(850, 0, 850, 720, MLV_COLOR_WHITE);

    MLV_draw_text(870, 30, "--- SETTINGS ---", MLV_COLOR_WHITE);
    MLV_draw_text(870, 80, "Taille de truc : ", MLV_COLOR_WHITE);
    MLV_draw_text(870, 120, "Nombre de truc : ", MLV_COLOR_WHITE);
    MLV_draw_text(870, 160, "Nombre de generations : ", MLV_COLOR_WHITE);

    MLV_draw_text_box(
        960, 600, 200, 60, 
        "LAUNCH ALGO", 
        2, 
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_GREEN, 
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
}

void settings_menu(Town *communes, size_t count, Individual ind) {
    int x, y;

    draw_base_interface();

    create_cloud(communes, count);
    
    MLV_actualise_window();

    MLV_Keyboard_button touche;

    touche=MLV_KEYBOARD_NONE;

    while(touche != MLV_KEYBOARD_ESCAPE){
        MLV_Event event = MLV_wait_keyboard_or_mouse(&touche, NULL, NULL, &x, &y);

        if (event == MLV_MOUSE_BUTTON && x >= 960 && x <= 1160 && y >= 600 && y <= 660) {
            printf("Bouton LANCER clique !\n");
            
            draw_base_interface();
            create_cloud(communes, count);
            draw_hospitals(communes, count, ind);
            
            MLV_actualise_window();
        }
    }
}

void close_window(void) {
    MLV_free_window();
}