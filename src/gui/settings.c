/**
 * @file settings.c
 * @brief Implémentation du menu de paramétrage : saisie population/générations et relance de l'algorithme.
 */

#include "settings.h"
#include "render.h"
#include "../data/export.h"
#include "../genetic/ga_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
