#pragma once
#include <SFML/Graphics.hpp>
#include "avion.hpp"

// Constantes
extern const unsigned int LARGEUR; // Largeur de la fenêtre
extern const unsigned int HAUTEUR; // Hauteur de la fenêtre
extern float ECHELLE; // Facteur d'échelle pour convertir les km en pixels
extern float DECALAGE_GAUCHE; // Décalage horizontal pour centrer la carte
extern float DECALAGE_DROITE; // Décalage vertical pour centrer la carte

// Fonctions utilitaires
sf::Vector2f conversion(Position pos); // Pour convertir en 2d
void adapterFondFenetre(sf::Sprite& sprite, const sf::Texture& texture); // Redimensionne l'image de fond

// Fonctions de dessin
void dessinerAeroports(sf::RenderWindow& window, const std::vector<Aeroport*>& aeroports, const sf::Font& police, bool Police); // Affiche les aéroports sur la carte globale
void dessinerDetailsAeroport(sf::RenderWindow& window, Aeroport* aero, const sf::Font& police, bool Police, float zoom); // Affiche les détails (piste, parkings) en vue zoomée
void dessinerAvion(sf::RenderWindow& window, Avion* avion, const sf::Texture& texture, bool hasTexture, const sf::Font& police, bool Police, float zoom, Avion* selection, Aeroport* vue); // Affiche un avion (sprite ou point)
void dessinerInfo(sf::RenderWindow& window, Avion* avion, const sf::Font& police, float zoom); // Affiche les infos de l'avion sélectionné