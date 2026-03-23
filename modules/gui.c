#include "gui.h"

#include <MLV/MLV_all.h>

void init_window(void) {
    MLV_create_window("MediFrance", "MediFrance", 800, 600);

    MLV_clear_window(MLV_COLOR_BLACK);
    MLV_draw_text(30, 30, "MediFrance - Basic LibMLV window", MLV_COLOR_WHITE);
    MLV_actualise_window();
    return;
}

void close_window(void) {
    MLV_free_window();
    return;
}