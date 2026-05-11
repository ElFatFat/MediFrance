  #ifndef DATA_H
  #define DATA_H

  #include "../main.h"

  Town* charger_communes(const char* csv_path, size_t* count);
  Hopital* charger_hopitaux(const char* csv_path, size_t* count);
  void liberer_communes(Town* communes);
  void liberer_hopitaux(Hopital* hopitaux);

  #endif
