#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>

typedef struct Town {
    int insee_code;
    char name[50];
    int region;
    char region_name[50];
    int departement;
    char department_name[50];
    int postal_code;
    int population;
    float x;
    float y;
    int visited;
} Town;

typedef struct Hopital {
    char name[256];
    float x; // Longitude
    float y; // Latitude
} Hopital;

#endif