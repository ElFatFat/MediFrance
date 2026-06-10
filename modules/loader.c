
/**
 * @file loader.c
 * @brief Implémentation du chargement et du parsing du CSV des communes.
 */

#include "loader.h"
#include <stdio.h>
#include <stdlib.h>

Town* load_towns_csv(const char* path, size_t* count) {
    FILE* file = fopen(path, "r");
    Town* towns = NULL;
    size_t capacity = 0;
    size_t loaded = 0;
    char line[512];

    if (file == NULL) {
        perror("Unable to open CSV file");
        return NULL;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        Town town;
        int parsed = sscanf(
            line,
            "%d,%49[^,],%d,%49[^,],%d,%49[^,],%d,%d,%f,%f",
            &town.insee_code,
            town.name,
            &town.region,
            town.region_name,
            &town.departement,
            town.department_name,
            &town.postal_code,
            &town.population,
            &town.y,
            &town.x
        );

        if (parsed != 10) {
            continue;
        }

        if (loaded == capacity) {
            size_t new_capacity = (capacity == 0) ? 1024 : capacity * 2;
            Town* resized = realloc(towns, new_capacity * sizeof(*towns));

            if (resized == NULL) {
                perror("Memory allocation failed");
                free(towns);
                fclose(file);
                return NULL;
            }

            towns = resized;
            capacity = new_capacity;
        }
        towns[loaded] = town;
        loaded++;
    }

    fclose(file);
    *count = loaded;
    return towns;
}
