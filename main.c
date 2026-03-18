#include "main.h"

#include <MLV/MLV_all.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char* csv_path = "communes-france-metrople-2025.csv";
    FILE* file = fopen(csv_path, "r");
    struct Commune* communes = NULL;
    size_t capacity = 0;
    size_t count = 0;
    char line[512];

    if (file == NULL) {
        perror("Unable to open CSV file");
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        struct Commune commune;
        int parsed = sscanf(
            line,
            "%d,%49[^,],%d,%49[^,],%d,%49[^,],%d,%d,%f,%f",
            &commune.code_insee,
            commune.nom,
            &commune.region,
            commune.region_name,
            &commune.departement,
            commune.departement_name,
            &commune.code_postal,
            &commune.population,
            &commune.x,
            &commune.y
        );

        if (parsed != 10) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 1024 : capacity * 2;
            struct Commune* resized = realloc(communes, new_capacity * sizeof(*communes));

            if (resized == NULL) {
                perror("Memory allocation failed");
                free(communes);
                fclose(file);
                return 1;
            }

            communes = resized;
            capacity = new_capacity;
        }

        communes[count] = commune;
        count++;
    }

    fclose(file);

    printf("Loaded %zu communes from %s\n", count, csv_path);

    if (count > 0) {
        printf(
            "First commune: %d, %s, %d, %s, %d, %s, %d, %d, %.3f, %.3f\n",
            communes[0].code_insee,
            communes[0].nom,
            communes[0].region,
            communes[0].region_name,
            communes[0].departement,
            communes[0].departement_name,
            communes[0].code_postal,
            communes[0].population,
            communes[0].x,
            communes[0].y
        );
    }

    MLV_create_window("MediFrance", "MediFrance", 800, 600);

    MLV_clear_window(MLV_COLOR_BLACK);
    MLV_draw_text(30, 30, "MediFrance - Basic LibMLV window", MLV_COLOR_WHITE);
    MLV_actualise_window();
    MLV_wait_seconds(3);
    MLV_free_window();

    free(communes);
    return 0;
}