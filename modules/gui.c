#include "gui.h"
#include "gen.h"
#include "export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define SETTINGS_PANEL_X 850
#define POP_LABEL_X 870
#define POP_LABEL_Y 80
#define GEN_LABEL_X 870
#define GEN_LABEL_Y 220
#define INPUT_X 980
#define POP_INPUT_Y 80
#define GEN_INPUT_Y 220
#define INPUT_W 200
#define INPUT_H 50
#define LAUNCH_X 960
#define LAUNCH_Y 600
#define LAUNCH_W 200
#define LAUNCH_H 60
#define INPUT_VALUE_LEN 32
#define DEFAULT_POP_TEXT "200"
#define DEFAULT_GEN_TEXT "1000"

static int g_map_screen_cached = 0;

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void init_window(void) {
    MLV_create_window("MediFrance", "MediFrance", WINDOW_WIDTH, WINDOW_HEIGHT);
    MLV_clear_window(MLV_COLOR_BLACK);
    MLV_draw_text(30, 30, "MediFrance - Basic LibMLV window", MLV_COLOR_WHITE);
    MLV_actualise_window();
}

void create_cloud(Town *communes, size_t count) {
    printf("Affichage de la carte...\n");
    MLV_Color city_color = MLV_rgba(80, 24, 151, 255);
    for (size_t i = 0; i < count; i++) {
        float x = communes[i].x; 
        float y = communes[i].y; 

        int screen_x = (int)((x + 5.5) * 41.3 + 90);
        int screen_y = (int)((51.5 - y) * 60);

        if (screen_x >= 0 && screen_x < WINDOW_WIDTH && screen_y >= 0 && screen_y < WINDOW_HEIGHT) {
            MLV_draw_point(screen_x, screen_y, city_color);
        }
    }
    printf("Carte affichée...\n");

}

void draw_hospitals(Town *communes, size_t count, Individual ind, const OptimizedData* data) {
    //MLV_Color hospital_color = MLV_rgba(0, 255, 0, 128); // Hôpital classique : vert
    //MLV_Color chru_color = MLV_rgba(0, 0, 255, 128);     // Ville éligible CHRU : bleu
    MLV_Color hospital_color = MLV_rgba(244, 63, 94, 128); // Hôpital classique : vert
    MLV_Color chru_color = MLV_rgba(253, 224, 71, 255); 
    for (size_t i = 0; i < count; i++) {
        if(ind.genes[i] == 1) {
            float x = communes[i].x;
            float y = communes[i].y;

            int screen_x = (int)((x + 5.5) * 41.3 + 90);
            int screen_y = (int)((51.5 - y) * 60);

            if (screen_x >= -6 && screen_x < WINDOW_WIDTH + 6 && screen_y >= -6 && screen_y < WINDOW_HEIGHT + 6) {
                MLV_Color color = data[i].is_eligible_for_chru ? chru_color : hospital_color;
                int radius = data[i].is_eligible_for_chru ? 6 : 4; // CHRU plus gros que les hôpitaux normaux
                MLV_draw_filled_circle(screen_x, screen_y, radius, color);
            }
        }
    }
}

// Affiche les statistiques du meilleur individu dans le panneau de droite.
// Le bloc occupe l'espace libéré par la suppression de la 3e boîte (~y 320 -> 560).
void draw_stats(Individual ind) {
    char buffer[128];
    const int tx = 870;
    const int line = 40; // interligne (bloc plus aéré)
    int ty = 320;

    MLV_draw_text(tx, ty, "--- RESULTAT ---", MLV_COLOR_WHITE); ty += line + 8;
    snprintf(buffer, sizeof(buffer), "Fitness  : %.0f", ind.fitness);
    MLV_draw_text(tx, ty, buffer, MLV_COLOR_WHITE); ty += line;
    snprintf(buffer, sizeof(buffer), "Hopitaux : %d", ind.hospitals_count);
    MLV_draw_text(tx, ty, buffer, MLV_COLOR_WHITE); ty += line;
    snprintf(buffer, sizeof(buffer), "CHRU     : %d", ind.chru_count);
    MLV_draw_text(tx, ty, buffer, MLV_COLOR_WHITE); ty += line;
    snprintf(buffer, sizeof(buffer), "Lits     : %ld", ind.beds_count);
    MLV_draw_text(tx, ty, buffer, MLV_COLOR_WHITE); ty += line;
    snprintf(buffer, sizeof(buffer), "Desert   : %ld", ind.desert_population);
    MLV_draw_text(tx, ty, buffer, MLV_COLOR_WHITE);
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

static void draw_settings_panel(const GAResult* res, const char* pop_val, const char* gen_val) {
    MLV_Color hospital_color = MLV_rgba(80, 48, 151, 255);
    MLV_Color valid_color = MLV_rgba(100, 220, 120, 255);
    char line[INPUT_VALUE_LEN + 16];

    if (g_map_screen_cached) {
        MLV_load_screen();
    }

    MLV_draw_filled_rectangle(
        SETTINGS_PANEL_X + 1, 0,
        WINDOW_WIDTH - SETTINGS_PANEL_X - 1, WINDOW_HEIGHT,
        MLV_COLOR_BLACK
    );

    MLV_draw_line(SETTINGS_PANEL_X, 0, SETTINGS_PANEL_X, WINDOW_HEIGHT, MLV_COLOR_WHITE);
    MLV_draw_text(870, 30, "--- SETTINGS ---", MLV_COLOR_WHITE);
    MLV_draw_text(870, 155, "Entree = valider la valeur", MLV_COLOR_WHITE);
    MLV_draw_text_box(
        POP_LABEL_X, POP_LABEL_Y, 100, 60,
        "population",
        2,
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, hospital_color,
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
    snprintf(line, sizeof(line), "Actif : %s", pop_val);
    MLV_draw_text(INPUT_X, POP_INPUT_Y + INPUT_H + 6, line, valid_color);
    MLV_draw_text_box(
        GEN_LABEL_X, GEN_LABEL_Y, 100, 60,
        "generations",
        2,
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, hospital_color,
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
    snprintf(line, sizeof(line), "Actif : %s", gen_val);
    MLV_draw_text(INPUT_X, GEN_INPUT_Y + INPUT_H + 6, line, valid_color);
    MLV_draw_text_box(
        LAUNCH_X, LAUNCH_Y, LAUNCH_W, LAUNCH_H,
        "LAUNCH ALGO",
        2,
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, hospital_color,
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );

    if (res->best.genes != NULL) {
        draw_stats(res->best);
    }
    MLV_draw_all_input_boxes();
    MLV_actualise_window();
}

static void cache_map_background(
    Town* communes,
    size_t count,
    const GAResult* res,
    const GAContext* ctx
) {
    MLV_clear_window(MLV_COLOR_BLACK);
    create_cloud(communes, count);
    if (res->best.genes != NULL) {
        draw_hospitals(communes, count, res->best, ctx->precalc_data);
    }
    draw_fitness_curve(res->best_fitness_history, res->worst_fitness_history, res->generations);
    MLV_save_screen();
    g_map_screen_cached = 1;
}

static int click_hits_input_box(int x, int y, MLV_Input_box* pop_box, MLV_Input_box* gen_box) {
    (void)pop_box;
    (void)gen_box;
    return point_in_rect(x, y, INPUT_X, POP_INPUT_Y, INPUT_W, INPUT_H)
        || point_in_rect(x, y, INPUT_X, GEN_INPUT_Y, INPUT_W, INPUT_H);
}

static void launch_algorithm(
    Town* communes,
    size_t count,
    GAResult* res,
    const GAContext* ctx,
    int population_param,
    int generations_param,
    int elites_param
) {
    printf("Bouton LANCER clique !\n");
    printf("Population  : %d\n", population_param);
    printf("Generations : %d\n", generations_param);
    printf("Elites      : %d\n", elites_param);

    if (population_param <= 1 || generations_param <= 0) {
        printf("Parametres invalides : population > 1 et generations > 0 requis.\n");
        return;
    }

    free(res->best.genes);
    free(res->best_fitness_history);
    free(res->worst_fitness_history);

    MLV_clear_window(MLV_COLOR_BLACK);
    create_cloud(communes, count);
    MLV_actualise_window();

    *res = run_genetic_algorithm(ctx, population_param, generations_param, elites_param);
    if (res->best.genes == NULL) {
        fprintf(stderr, "Echec de la relance de l'algorithme genetique\n");
        return;
    }

    audit_coverage(communes, count, ctx->precalc_data, &res->best);
    if (export_resultats_csv("data/resultats_hopitaux.csv", communes, count,
                             ctx->precalc_data, &res->best) != 0) {
        fprintf(stderr, "Avertissement : export CSV échoué\n");
    }
}

static void store_input_value(char* dest, size_t dest_size, const char* text) {
    if (text == NULL || text[0] == '\0') {
        return;
    }
    strncpy(dest, text, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void settings_menu(Town *communes, size_t count, GAResult* res, const GAContext* ctx) {
    char pop_buf[INPUT_VALUE_LEN] = DEFAULT_POP_TEXT;
    char gen_buf[INPUT_VALUE_LEN] = DEFAULT_GEN_TEXT;
    int elites_param = 0;

    MLV_Input_box* pop_box = MLV_create_input_box(
        INPUT_X, POP_INPUT_Y, INPUT_W, INPUT_H,
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_BLACK,
        "Population"
    );
    MLV_Input_box* gen_box = MLV_create_input_box(
        INPUT_X, GEN_INPUT_Y, INPUT_W, INPUT_H,
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, MLV_COLOR_BLACK,
        "Generations"
    );
    if (pop_box == NULL || gen_box == NULL) {
        fprintf(stderr, "Echec de creation des boites de saisie\n");
        MLV_free_input_box(pop_box);
        MLV_free_input_box(gen_box);
        return;
    }

    cache_map_background(communes, count, res, ctx);
    draw_settings_panel(res, pop_buf, gen_buf);

    MLV_Keyboard_button key = MLV_KEYBOARD_NONE;
    MLV_Mouse_button mouse_button = MLV_BUTTON_LEFT;
    MLV_Button_state button_state = MLV_RELEASED;
    char* text = NULL;
    MLV_Input_box* active_box = NULL;
    MLV_Input_box* last_activated_box = NULL;
    int x = 0;
    int y = 0;

    while (key != MLV_KEYBOARD_ESCAPE) {
        MLV_Event event = MLV_wait_event(
            &key, NULL, NULL,
            &text, &active_box,
            &x, &y, &mouse_button, &button_state
        );

        if (event == MLV_KEY && key == MLV_KEYBOARD_ESCAPE) {
            break;
        }

        if (event == MLV_INPUT_BOX) {
            MLV_Input_box* validated_box = active_box != NULL ? active_box : last_activated_box;
            if (text != NULL) {
                if (validated_box == pop_box) {
                    store_input_value(pop_buf, sizeof(pop_buf), text);
                    printf("Population saisie : %s\n", pop_buf);
                } else if (validated_box == gen_box) {
                    store_input_value(gen_buf, sizeof(gen_buf), text);
                    printf("Generations saisie : %s\n", gen_buf);
                }
                free(text);
                text = NULL;
            }
            MLV_desactivate_input_box();
            last_activated_box = NULL;
            draw_settings_panel(res, pop_buf, gen_buf);
        } else if (event == MLV_MOUSE_BUTTON
            && mouse_button == MLV_BUTTON_LEFT
            && button_state == MLV_PRESSED) {
            if (point_in_rect(x, y, LAUNCH_X, LAUNCH_Y, LAUNCH_W, LAUNCH_H)) {
                MLV_desactivate_input_box();
                launch_algorithm(
                    communes, count, res, ctx,
                    atoi(pop_buf), atoi(gen_buf), elites_param
                );
                cache_map_background(communes, count, res, ctx);
                draw_settings_panel(res, pop_buf, gen_buf);
            } else if (point_in_rect(x, y, POP_LABEL_X, POP_LABEL_Y, 100, 60)
                || point_in_rect(x, y, INPUT_X, POP_INPUT_Y, INPUT_W, INPUT_H)) {
                MLV_activate_input_box(pop_box);
                last_activated_box = pop_box;
            } else if (point_in_rect(x, y, GEN_LABEL_X, GEN_LABEL_Y, 100, 60)
                || point_in_rect(x, y, INPUT_X, GEN_INPUT_Y, INPUT_W, INPUT_H)) {
                MLV_activate_input_box(gen_box);
                last_activated_box = gen_box;
            } else if (!click_hits_input_box(x, y, pop_box, gen_box)) {
                MLV_desactivate_input_box();
                draw_settings_panel(res, pop_buf, gen_buf);
            }
        }
    }

    MLV_desactivate_input_box();
    MLV_free_input_box(pop_box);
    MLV_free_input_box(gen_box);
}

void close_window(void) {
    MLV_free_window();
}