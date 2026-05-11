#include <stdlib.h>
#include "data.h"

Town* charger_communes(const char* csv_path, size_t* count) {
      FILE* file = fopen(csv_path, "r");
      Town* communes = NULL;
      size_t capacity = 0;
      *count = 0;
      char line[512];

      if (file == NULL) {
          perror("Unable to open CSV file");
          return NULL;
      }

      while (fgets(line, sizeof(line), file) != NULL) {
          Town commune;
          int parsed = sscanf(
              line,
              "%d,%49[^,],%d,%49[^,],%d,%49[^,],%d,%d,%f,%f",
              &commune.insee_code,
              commune.name,
              &commune.region,
              commune.region_name,
              &commune.departement,
              commune.department_name,
              &commune.postal_code,
              &commune.population,
              &commune.y,
              &commune.x
          );

          if (parsed != 10) continue;

          if (*count == capacity) {
              size_t new_cap = (capacity == 0) ? 1024 : capacity * 2;
              Town* resized = realloc(communes, new_cap * sizeof(*communes));
              if (resized == NULL) {
                  perror("Memory allocation failed");
                  free(communes);
                  fclose(file);
                  return NULL;
              }
              communes = resized;
              capacity = new_cap;
          }

          commune.visited = 0;
          communes[*count] = commune;
          (*count)++;
      }

      fclose(file);
      return communes;
  }



    void liberer_communes(Town* communes) {
        free(communes);
    }