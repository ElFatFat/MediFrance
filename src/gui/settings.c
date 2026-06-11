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

typedef enum {
    FOCUS_NONE = 0,
    FOCUS_POP,
    FOCUS_GEN
} InputFocus;

static int g_map_screen_cached = 0;

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static int click_hits_pop_field(int x, int y) {
    return point_in_rect(x, y, POP_LABEL_X, POP_LABEL_Y, 100, 60)
        || point_in_rect(x, y, INPUT_X, POP_INPUT_Y, INPUT_W, INPUT_H);
}

static int click_hits_gen_field(int x, int y) {
    return point_in_rect(x, y, GEN_LABEL_X, GEN_LABEL_Y, 100, 60)
        || point_in_rect(x, y, INPUT_X, GEN_INPUT_Y, INPUT_W, INPUT_H);
}

static void draw_text_field(int x, int y, int w, int h, const char* value, int focused) {
    MLV_Color border = focused ? MLV_rgba(100, 220, 120, 255) : MLV_COLOR_WHITE;
    MLV_draw_filled_rectangle(x, y, w, h, MLV_COLOR_BLACK);
    MLV_draw_rectangle(x, y, w, h, border);
    if (value[0] != '\0') {
        MLV_draw_text(x + 8, y + 16, value, MLV_COLOR_WHITE);
    }
}

static void draw_settings_panel(
    const GAResult* res,
    const char* pop_val,
    const char* gen_val,
    const char* pop_edit,
    const char* gen_edit,
    InputFocus focus
) {
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
    draw_text_field(INPUT_X, POP_INPUT_Y, INPUT_W, INPUT_H, pop_edit, focus == FOCUS_POP);
    snprintf(line, sizeof(line), "Actif : %s", pop_val);
    MLV_draw_text(INPUT_X, POP_INPUT_Y + INPUT_H + 6, line, valid_color);
    MLV_draw_text_box(
        GEN_LABEL_X, GEN_LABEL_Y, 100, 60,
        "generations",
        2,
        MLV_COLOR_WHITE, MLV_COLOR_WHITE, hospital_color,
        MLV_TEXT_CENTER, MLV_HORIZONTAL_CENTER, MLV_VERTICAL_CENTER
    );
    draw_text_field(INPUT_X, GEN_INPUT_Y, INPUT_W, INPUT_H, gen_edit, focus == FOCUS_GEN);
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

static void copy_string(char* dest, size_t dest_size, const char* src) {
    snprintf(dest, dest_size, "%s", src);
}

static void commit_focused_field(
    InputFocus focus,
    char* pop_buf,
    char* gen_buf,
    const char* pop_edit,
    const char* gen_edit
) {
    if (focus == FOCUS_POP && pop_edit[0] != '\0') {
        copy_string(pop_buf, INPUT_VALUE_LEN, pop_edit);
        printf("Population saisie : %s\n", pop_buf);
    } else if (focus == FOCUS_GEN && gen_edit[0] != '\0') {
        copy_string(gen_buf, INPUT_VALUE_LEN, gen_edit);
        printf("Generations saisie : %s\n", gen_buf);
    }
}

static int key_to_digit(MLV_Keyboard_button key, int* digit) {
    if (key >= MLV_KEYBOARD_0 && key <= MLV_KEYBOARD_9) {
        *digit = (int)(key - MLV_KEYBOARD_0);
        return 1;
    }
    if (key >= MLV_KEYBOARD_KP0 && key <= MLV_KEYBOARD_KP9) {
        *digit = (int)(key - MLV_KEYBOARD_KP0);
        return 1;
    }
    return 0;
}

static void append_digit(char* buf, size_t buf_size, int digit) {
    size_t len = strlen(buf);
    if (len + 1 >= buf_size) {
        return;
    }
    buf[len] = (char)('0' + digit);
    buf[len + 1] = '\0';
}

static void backspace_char(char* buf) {
    size_t len = strlen(buf);
    if (len == 0) {
        return;
    }
    buf[len - 1] = '\0';
}

static void handle_text_key(
    InputFocus focus,
    char* pop_edit,
    char* gen_edit,
    MLV_Keyboard_button key
) {
    char* edit_buf = (focus == FOCUS_POP) ? pop_edit : gen_edit;
    int digit = 0;

    if (key == MLV_KEYBOARD_BACKSPACE || key == MLV_KEYBOARD_DELETE) {
        backspace_char(edit_buf);
        return;
    }

    if (key_to_digit(key, &digit)) {
        append_digit(edit_buf, INPUT_VALUE_LEN, digit);
    }
}

void settings_menu(Town *communes, size_t count, GAResult* res, const GAContext* ctx) {
    char pop_buf[INPUT_VALUE_LEN] = DEFAULT_POP_TEXT;
    char gen_buf[INPUT_VALUE_LEN] = DEFAULT_GEN_TEXT;
    char pop_edit[INPUT_VALUE_LEN] = DEFAULT_POP_TEXT;
    char gen_edit[INPUT_VALUE_LEN] = DEFAULT_GEN_TEXT;
    InputFocus focus = FOCUS_NONE;
    int elites_param = 15;

    cache_map_background(communes, count, res, ctx);
    draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);

    MLV_Keyboard_button key = MLV_KEYBOARD_NONE;
    MLV_Mouse_button mouse_button = MLV_BUTTON_LEFT;
    MLV_Button_state button_state = MLV_RELEASED;
    int x = 0;
    int y = 0;

    while (key != MLV_KEYBOARD_ESCAPE) {
        MLV_Event event = MLV_wait_event(
            &key, NULL, NULL,
            NULL, NULL,
            &x, &y, &mouse_button, &button_state
        );

        if (event == MLV_KEY && key == MLV_KEYBOARD_ESCAPE) {
            break;
        }

        if (event == MLV_KEY
            && button_state == MLV_PRESSED
            && focus != FOCUS_NONE) {
            if (key == MLV_KEYBOARD_RETURN || key == MLV_KEYBOARD_KP_ENTER) {
                commit_focused_field(focus, pop_buf, gen_buf, pop_edit, gen_edit);
                focus = FOCUS_NONE;
                draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);
            } else {
                handle_text_key(focus, pop_edit, gen_edit, key);
                draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);
            }
        } else if (event == MLV_MOUSE_BUTTON
            && mouse_button == MLV_BUTTON_LEFT
            && button_state == MLV_PRESSED) {
            if (point_in_rect(x, y, LAUNCH_X, LAUNCH_Y, LAUNCH_W, LAUNCH_H)) {
                if (focus != FOCUS_NONE) {
                    commit_focused_field(focus, pop_buf, gen_buf, pop_edit, gen_edit);
                    focus = FOCUS_NONE;
                }
                launch_algorithm(
                    communes, count, res, ctx,
                    atoi(pop_buf), atoi(gen_buf), elites_param
                );
                cache_map_background(communes, count, res, ctx);
                copy_string(pop_edit, INPUT_VALUE_LEN, pop_buf);
                copy_string(gen_edit, INPUT_VALUE_LEN, gen_buf);
                draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);
            } else if (click_hits_pop_field(x, y)) {
                if (focus == FOCUS_GEN) {
                    commit_focused_field(FOCUS_GEN, pop_buf, gen_buf, pop_edit, gen_edit);
                }
                focus = FOCUS_POP;
                copy_string(pop_edit, INPUT_VALUE_LEN, pop_buf);
                draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);
            } else if (click_hits_gen_field(x, y)) {
                if (focus == FOCUS_POP) {
                    commit_focused_field(FOCUS_POP, pop_buf, gen_buf, pop_edit, gen_edit);
                }
                focus = FOCUS_GEN;
                copy_string(gen_edit, INPUT_VALUE_LEN, gen_buf);
                draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);
            } else {
                if (focus != FOCUS_NONE) {
                    commit_focused_field(focus, pop_buf, gen_buf, pop_edit, gen_edit);
                    focus = FOCUS_NONE;
                    draw_settings_panel(res, pop_buf, gen_buf, pop_edit, gen_edit, focus);
                }
            }
        }
    }
}
