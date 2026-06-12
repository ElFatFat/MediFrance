#ifndef RENDER_H
#define RENDER_H

#include "../../main.h"
#include "../genetic/gen.h"
#include <MLV/MLV_all.h>

/**
 * @file render.h
 * @brief Fonctions de rendu via la LibMLV : gestion de la fenêtre, carte, statistiques et courbes de fitness.
 *
 * Ce module regroupe l'ensemble des fonctionnalités d'affichage graphique de l'application. 
 * Il s'occupe de dessiner le nuage de points des communes, de matérialiser les décisions 
 * d'ouverture d'hôpitaux et de CHRU, et d'afficher l'évolution temporelle des performances.
 */

/**
 * @def WINDOW_WIDTH
 * @brief Largeur fixe en pixels de la fenêtre graphique de l'application (1280 px).
 */
#define WINDOW_WIDTH 1280

/**
 * @def WINDOW_HEIGHT
 * @brief Hauteur fixe en pixels de la fenêtre graphique de l'application (720 px).
 */
#define WINDOW_HEIGHT 720

/**
 * @brief Initialise le sous-système graphique LibMLV et ouvre la fenêtre principale.
 * * Crée une fenêtre nommée "MediFrance", applique un fond noir par défaut, et affiche 
 * un message de bienvenue d'initialisation.
 */
void init_window(void);

/**
 * @brief Libère proprement les ressources allouées par la LibMLV et ferme la fenêtre.
 */
void close_window(void);

/**
 * @brief Génère le nuage de points cartographique représentant les communes de France.
 * * Parcourt le tableau complet des communes et projette leurs coordonnées géographiques 
 * pour dessiner un pixel blanc par commune, faisant ainsi apparaître la forme du territoire.
 *
 * @param communes Tableau de toutes les structures Town chargées en mémoire.
 * @param count    Nombre total de communes présentes dans le tableau.
 */
void create_cloud(Town *communes, size_t count);

/**
 * @brief Dessine les hôpitaux et les CHRU sur la carte pour un individu donné.
 * * Parcourt les gènes de la solution fournie. Si un gène est actif, l'établissement 
 * est dessiné (en vert pour un hôpital standard, en rouge pour un CHRU) avec un cercle 
 * translucide représentant son rayon de couverture de 10 km.
 *
 * @param communes Tableau de l'intégralité des communes.
 * @param count    Nombre total de communes.
 * @param ind      L'individu (solution) dont on souhaite matérialiser le réseau d'établissements.
 * @param data     Données de voisinage précalculées nécessaires à l'identification visuelle.
 */
void draw_hospitals(Town *communes, size_t count, Individual ind, const OptimizedData* data);

/**
 * @brief Affiche le panneau textuel des statistiques métier sur la droite de l'écran.
 * * Dessine un cadre d'interface dédié et y inscrit en texte blanc lisible les valeurs 
 * actuelles de l'individu d'élite : score de fitness, nombre d'hôpitaux et de CHRU ouverts, 
 * population totale en désert médical et volume global de lits déployés.
 *
 * @param ind L'individu d'élite dont on extrait et affiche les métriques courantes.
 */
void draw_stats(Individual ind);

/**
 * @brief Dessine de manière incrémentale un segment de la courbe d'évolution de la fitness.
 * * Utilisée à chaque génération à l'intérieur de la boucle de l'algorithme génétique. 
 * Elle relie par un segment de droite les valeurs de la génération précédente à celles 
 * de la génération actuelle (bleu pour la meilleure fitness, jaune pour la pire).
 *
 * @param gen           Index de la génération courante.
 * @param nb_gen        Nombre maximum de générations prévu pour la simulation (sert au calcul de l'échelle X).
 * @param best_fitness  Score de fitness du meilleur individu à l'itération actuelle.
 * @param worst_fitness Score de fitness du moins bon individu à l'itération actuelle.
 */
void draw_fitness_point(int gen, int nb_gen, double best_fitness, double worst_fitness);

/**
 * @brief Reconstruit et trace l'intégralité des courbes de fitness à partir d'historiques.
 * * Redessine entièrement le graphique à partir de deux tableaux de données temporelles. 
 * Indispensable pour rafraîchir l'écran à volonté sans perte d'information après un effacement 
 * complet de la fenêtre ou lors d'interactions dans les menus.
 *
 * @param best_history  Tableau dynamique contenant l'historique des meilleures fitness.
 * @param worst_history Tableau dynamique contenant l'historique des pires fitness.
 * @param nb_gen        Nombre d'éléments enregistrés dans les historiques (générations accomplies).
 */
void draw_fitness_curve(const double* best_history, const double* worst_history, int nb_gen);

#endif /* RENDER_H */