#ifndef SETTINGS_H
#define SETTINGS_H

#include "../main.h"
#include "gen.h"

/**
 * @file settings.h
 * @brief Menu interactif de paramétrage et de relance de l'algorithme génétique.
 */

void settings_menu(Town *communes, size_t count, GAResult* res, const GAContext* ctx);

#endif /* SETTINGS_H */
