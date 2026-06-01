#include "gui.h"
#include "gen.h"
#include <stdlib.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// Zone d'affichage de la courbe de fitness, située sous la carte (sans la recouvrir).
// La carte de France descend jusqu'à ~y=610 ; on place la courbe en dessous.
#define GRAPH_LEFT 20
#define GRAPH_RIGHT 830
#define GRAPH_TOP 625
#define GRAPH_BOTTOM 712
#define FITNESS_MIN 40000000.0
#define FITNESS_MAX 65000000.0

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

void draw_hospitals(Town *communes, size_t count, Individual ind, const OptimizedData* data) {
    MLV_Color hospital_color = MLV_rgba(0, 255, 0, 128); // Hôpital classique : vert
    MLV_Color chru_color = MLV_rgba(0, 0, 255, 128);     // Ville éligible CHRU : bleu
    for (size_t i = 0; i < count; i++) {
        if(ind.genes[i] == 1) {
            float x = communes[i].x;
            float y = communes[i].y;

            int screen_x = (int)((x + 5.5) * 41.3 + 90);
            int screen_y = (int)((51.5 - y) * 60);

            if (screen_x >= -6 && screen_x < WINDOW_WIDTH + 6 && screen_y >= -6 && screen_y < WINDOW_HEIGHT + 6) {
                MLV_Color color = data[i].is_eligible_for_chru ? chru_color : hospital_color;
                int radius = data[i].is_eligible_for_chru ? 6 : 3; // CHRU plus gros que les hôpitaux normaux
                MLV_draw_filled_circle(screen_x, screen_y, radius, color);
            }
        }
    }
}

// Affiche les statistiques du meilleur individu dans le panneau de droite
void draw_stats(Individual ind) {
    char buffer[128];
    int ty = 460;

    MLV_draw_text(870, ty, "--- RESULTAT ---", MLV_COLOR_WHITE); ty += 26;
    snprintf(buffer, sizeof(buffer), "Fitness  : %.0f", ind.fitness);
    MLV_draw_text(870, ty, buffer, MLV_COLOR_WHITE); ty += 22;
    snprintf(buffer, sizeof(buffer), "Hopitaux : %d", ind.hospitals_count);
    MLV_draw_text(870, ty, buffer, MLV_COLOR_WHITE); ty += 22;
    snprintf(buffer, sizeof(buffer), "CHRU     : %d", ind.chru_count);
    MLV_draw_text(870, ty, buffer, MLV_COLOR_WHITE); ty += 22;
    snprintf(buffer, sizeof(buffer), "Lits     : %ld", ind.beds_count);
    MLV_draw_text(870, ty, buffer, MLV_COLOR_WHITE);
}

// Convertit un indice de génération en coordonnée X dans la zone de courbe
static int graph_x(int index, int nb_gen) {
    if (nb_gen <= 1) return GRAPH_LEFT;
    return GRAPH_LEFT + (int)((long)index * (GRAPH_RIGHT - GRAPH_LEFT) / (nb_gen - 1));
}

// Convertit une valeur de fitness en coordonnée Y dans la zone de courbe
static int graph_y(double fitness) {
    if (fitness > FITNESS_MAX) fitness = FITNESS_MAX;
    if (fitness < FITNESS_MIN) fitness = FITNESS_MIN;
    double ratio = (fitness - FITNESS_MIN) / (FITNESS_MAX - FITNESS_MIN);
    return GRAPH_BOTTOM - (int)(ratio * (GRAPH_BOTTOM - GRAPH_TOP));
}

// Dessine le repère de la zone de courbe (ligne de base + légende)
static void draw_graph_frame(void) {
    MLV_Color axis_color = MLV_rgba(120, 120, 120, 255);
    MLV_draw_line(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, axis_color);
    MLV_draw_text(GRAPH_LEFT, GRAPH_TOP - 18,
                  "Evolution de la fitness (bleu = meilleure, jaune = pire)", MLV_COLOR_WHITE);
}

// Trace en temps réel le segment de courbe de la génération `gen`, appelé depuis
// l'algo génétique pendant l'évolution. Conserve le point précédent pour relier les
// générations ; à gen == 0 on (re)dessine le repère et on réinitialise l'origine.
void draw_fitness_point(int gen, int nb_gen, double best_fitness, double worst_fitness) {
    static int prev_x = GRAPH_LEFT;
    static int prev_best_y = GRAPH_BOTTOM;
    static int prev_worst_y = GRAPH_BOTTOM;

    int cx = graph_x(gen, nb_gen);
    int by = graph_y(best_fitness);
    int wy = graph_y(worst_fitness);

    if (gen == 0) {
        draw_graph_frame(); // nouveau tracé
    } else {
        MLV_draw_line(prev_x, prev_worst_y, cx, wy, MLV_COLOR_YELLOW);
        MLV_draw_line(prev_x, prev_best_y,  cx, by, MLV_COLOR_BLUE);
    }

    prev_x = cx;
    prev_best_y = by;
    prev_worst_y = wy;

    MLV_actualise_window();
}

// Trace la courbe complète d'évolution de la fitness (meilleure en bleu, pire en
// jaune) dans une bande sous la carte. Ne dépend que de l'historique : peut donc
// être redessinée à volonté après un effacement de la fenêtre, sans recalcul.
void draw_fitness_curve(const double* best_history, const double* worst_history, int nb_gen) {
    if (nb_gen <= 0) return;

    draw_graph_frame();

    for (int i = 1; i < nb_gen; i++) {
        if (worst_history != NULL) {
            MLV_draw_line(graph_x(i - 1, nb_gen), graph_y(worst_history[i - 1]),
                          graph_x(i, nb_gen),     graph_y(worst_history[i]), MLV_COLOR_YELLOW);
        }
        if (best_history != NULL) {
            MLV_draw_line(graph_x(i - 1, nb_gen), graph_y(best_history[i - 1]),
                          graph_x(i, nb_gen),     graph_y(best_history[i]), MLV_COLOR_BLUE);
        }
    }
}

void draw_base_interface() {

    MLV_clear_window(MLV_COLOR_BLACK);
    
    MLV_draw_line(850, 0, 850, 720, MLV_COLOR_WHITE);

    MLV_draw_text(870, 30, "--- SETTINGS ---", MLV_COLOR_WHITE);
    //MLV_draw_text(870, 80, "Taille de population : ", MLV_COLOR_WHITE);
    MLV_draw_text_box(
        870, 80, 100, 60, 
        "population", 
        2, 
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_GREEN, 
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
    //MLV_create_input_box(870,120,200,40,MLV_COLOR_BLACK,MLV_COLOR_BLACK,MLV_COLOR_WHITE,"");

    //MLV_draw_text(870, 220, "Nombre de générations : ", MLV_COLOR_WHITE);
    MLV_draw_text_box(
        870, 220, 100, 60, 
        "générations", 
        2, 
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_GREEN, 
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
    //MLV_create_input_box(870,260,200,40,MLV_COLOR_BLACK,MLV_COLOR_BLACK,MLV_COLOR_WHITE,"");

    //MLV_draw_text(870, 360, "Nombre d'elites : ", MLV_COLOR_WHITE);
    MLV_draw_text_box(
        870, 360, 100, 60, 
        "elites", 
        2, 
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_GREEN, 
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
    //MLV_create_input_box(870,400,200,40,MLV_COLOR_BLACK,MLV_COLOR_BLACK,MLV_COLOR_WHITE,"");

    MLV_draw_all_input_boxes();

    MLV_draw_text_box(
        960, 600, 200, 60, 
        "LAUNCH ALGO", 
        2, 
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_GREEN, 
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
}

void settings_menu(Town *communes, size_t count, GAResult* res, const GAContext* ctx) {
    int x, y;

    char *text = NULL;          // initialisé à NULL

    int populationParam  = 0;
    int generationsParam = 0;
    int elitesParam      = 0;

    draw_base_interface();
    create_cloud(communes, count);
    if (res->best.genes != NULL) {
        draw_hospitals(communes, count, res->best, ctx->precalc_data); // Affiche le résultat initial
        draw_stats(res->best);
    }
    draw_fitness_curve(res->best_fitness_history, res->worst_fitness_history, res->generations);
    MLV_actualise_window();

    MLV_Keyboard_button touche = MLV_KEYBOARD_NONE;

    while (touche != MLV_KEYBOARD_ESCAPE) {
        MLV_Event event = MLV_wait_keyboard_or_mouse(&touche, NULL, NULL, &x, &y);

        MLV_draw_all_input_boxes();
        MLV_actualise_window();

        if (event == MLV_MOUSE_BUTTON && x >= 870 && x <= 970 && y >= 80 && y <= 140) {
            MLV_wait_input_box(980, 80, 200, 60,
                MLV_COLOR_BLACK, MLV_COLOR_BLACK, MLV_COLOR_WHITE, " ", &text);
            populationParam = atoi(text);   // conversion chaîne -> int
            printf("Population saisie : %d\n", populationParam);
            free(text);                     // MLV a alloué la mémoire
            text = NULL;
        }

        if (event == MLV_MOUSE_BUTTON && x >= 870 && x <= 970 && y >= 220 && y <= 280) {
            MLV_wait_input_box(980, 220, 200, 60,
                MLV_COLOR_BLACK, MLV_COLOR_BLACK, MLV_COLOR_WHITE, " ", &text);
            generationsParam = atoi(text);
            printf("Generations saisie : %d\n", generationsParam);
            free(text);
            text = NULL;
        }

        if (event == MLV_MOUSE_BUTTON && x >= 870 && x <= 970 && y >= 360 && y <= 420) {
            MLV_wait_input_box(980, 360, 200, 60,
                MLV_COLOR_BLACK, MLV_COLOR_BLACK, MLV_COLOR_WHITE, " ", &text);
            elitesParam = atoi(text);
            printf("Elites saisie : %d\n", elitesParam);
            free(text);
            text = NULL;
        }

        if (event == MLV_MOUSE_BUTTON && x >= 960 && x <= 1160 && y >= 600 && y <= 660) {
            printf("Bouton LANCER clique !\n");
            printf("Population  : %d\n", populationParam);
            printf("Generations : %d\n", generationsParam);
            printf("Elites      : %d\n", elitesParam);

            if (populationParam > 1 && generationsParam > 0) {
                // On relance l'algo génétique avec les paramètres saisis.
                // Les workspaces et données précalculées sont réutilisés via le contexte.
                free(res->best.genes);                  // Libère le résultat précédent
                free(res->best_fitness_history);
                free(res->worst_fitness_history);

                // Fond propre pour le tracé temps réel : carte seule (sans hôpitaux)
                MLV_clear_window(MLV_COLOR_BLACK);
                create_cloud(communes, count);
                MLV_actualise_window();

                *res = run_genetic_algorithm(ctx, populationParam, generationsParam, elitesParam);
                if (res->best.genes == NULL) {
                    fprintf(stderr, "Echec de la relance de l'algorithme genetique\n");
                }
            } else {
                printf("Parametres invalides : population > 1 et generations > 0 requis.\n");
            }

            // Réaffichage de la carte avec le résultat courant + la courbe de fitness
            draw_base_interface();
            create_cloud(communes, count);
            if (res->best.genes != NULL) {
                draw_hospitals(communes, count, res->best, ctx->precalc_data);
                draw_stats(res->best);
            }
            draw_fitness_curve(res->best_fitness_history, res->worst_fitness_history, res->generations);
            MLV_draw_all_input_boxes();
            MLV_actualise_window();
        }
    }
}

void close_window(void) {
    MLV_free_window();
}