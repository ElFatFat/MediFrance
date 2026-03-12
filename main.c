#include <MLV/MLV_all.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    // 1. Create a window of 800x600 pixels
    MLV_create_window("libMLV Test", "Test", 800, 600);

    // 2. Draw some text in the center
    MLV_draw_text(
        300, 280, 
        "libMLV is working!", 
        MLV_COLOR_WHITE
    );

    // 3. Refresh the window to show the changes
    MLV_actualise_window();

    printf("Window opened successfully. Press any key in the window to exit.\n");

    // 4. Wait for a keyboard event before closing
    MLV_wait_keyboard_or_mouse(NULL, NULL, NULL, NULL, NULL);

    // 5. Clean up and close the window
    MLV_free_window();

    (void)argc;
    (void)argv;
    return 0;
}