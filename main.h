#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>

typedef struct Commune {
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
    int visited;
} Commune;

typedef struct Hopital {
    char nom[256];
    float x; // Longitude
    float y; // Latitude
} Hopital;

#endif