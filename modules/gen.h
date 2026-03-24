#ifndef GEN_H
#define GEN_H

#include "../main.h"

typedef struct {
    int* voisins;    // Tableau d'indices des communes à < 10km
    int nb_voisins;
    int est_eligible_chru; // 1 si pop > 80 000, sinon 0
    int max_pop_couverte;
} DataOptimisee;

typedef struct {
    unsigned char* genes; // Tableau de taille NB_COMMUNES (0 ou 1)
    double fitness;
    int nb_hopitaux;
    int nb_chru;
    long hab_desert;
} Individu;

typedef struct {
    int* indices;
    int count;
    int capacity;
} Cell;

typedef struct {
    unsigned char* couvert; // Tableau de 36000 octets
} ThreadWorkspace;

DataOptimisee* precalc_near(Commune* communes, size_t count);
void fitness(Individu* ind, Commune* communes, DataOptimisee* data, size_t count, unsigned char* couvert_local);
void quick_sort_population(Individu* pop, int left, int right);
void copier_individu(Individu* dest, const Individu* src, size_t count);
void muter(Individu* ind, size_t count, unsigned int* seed);
void muter_intelligente(Individu* ind, const DataOptimisee* data, size_t count, unsigned int* seed);
void crossover(Individu* enfant, const Individu* p1, const Individu* p2, size_t count, unsigned int* seed);
#endif