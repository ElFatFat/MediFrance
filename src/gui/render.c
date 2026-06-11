/**
 * @file render.c
 * @brief Implémentation du rendu : carte des communes, hôpitaux, statistiques et courbe de fitness.
 */

#include "render.h"
#include <stdio.h>

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

void close_window(void) {
    MLV_free_window();
}

// Projection des coordonnées géographiques (longitude x, latitude y) vers l'écran.
static void town_to_screen(float x, float y, int* screen_x, int* screen_y) {
    *screen_x = (int)((x + 5.5) * 41.3 + 90);
    *screen_y = (int)((51.5 - y) * 60);
}

void create_cloud(Town *communes, size_t count) {
    printf("Affichage de la carte...\n");
    MLV_Color city_color = MLV_rgba(80, 24, 151, 255);
    for (size_t i = 0; i < count; i++) {
        int screen_x;
        int screen_y;
        town_to_screen(communes[i].x, communes[i].y, &screen_x, &screen_y);

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
            int screen_x;
            int screen_y;
            town_to_screen(communes[i].x, communes[i].y, &screen_x, &screen_y);

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
