#include <stdio.h>

struct Commune {
    int code_insee;
    char nom[50];
    int region;
    char region_name[50];
    int departement;
    char departement_name[50];
    int code_postal;
    int population;
    float x;
    float y;
};