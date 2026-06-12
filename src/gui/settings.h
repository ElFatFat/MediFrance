#ifndef SETTINGS_H
#define SETTINGS_H

#include "../../main.h"
#include "../genetic/gen.h"

/**
 * @file settings.h
 * @brief Menu interactif de paramétrage, saisies clavier et contrôle de relance pour l'algorithme génétique.
 * @author Votre Nom
 * @date 2026
 *
 * Ce module fournit l'IHM (Interface Homme-Machine) permettant de modifier à la volée 
 * les hyper-paramètres de la simulation, de capturer les événements clavier/souris 
 * de la LibMLV et d'orchestrer la ré-exécution du moteur génétique.
 */

/**
 * @brief Lance la boucle interactive du menu des configurations et de l'affichage des résultats.
 * * Cette fonction bloque l'exécution dans une boucle événementielle (`while`) scrutant activement 
 * les clics de souris et les entrées de texte clavier. Elle affiche la carte de France résultante, 
 * le panneau de saisie des paramètres, et permet de relancer l'algorithme génétique avec de nouvelles 
 * valeurs de population ou de générations.
 *
 * @param communes Pointeur vers le tableau global de structures Town (communes chargées).
 * @param count    Nombre total de communes indexées dans le système.
 * @param res      Pointeur vers la structure GAResult contenant l'élite et l'historique des fitness (modifié en cas de relance).
 * @param ctx      Pointeur vers le contexte partagé de l'algorithme génétique (contenant les threads, matrices de voisins, etc.).
 */
void settings_menu(Town *communes, size_t count, GAResult* res, const GAContext* ctx);

#endif /* SETTINGS_H */