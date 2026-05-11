  #ifndef DATA_H
  #define DATA_H

  #include "../main.h"

  Town* charger_communes(const char* csv_path, size_t* count);
  void liberer_communes(Town* communes);

  #endif
