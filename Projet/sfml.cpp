#include "sfml.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

// Définition des constantes d'affichage
const unsigned int WINDOW_WIDTH = 1100;
const unsigned int WINDOW_HEIGHT = 1000;
float ECHELLE = 0.00068f;
float DECALAGE_GAUCHE = 600.0f;
float DECALAGE_DROITE = 280.0f;
const float PI = 3.14159265f;

// Convertit les coordonnées du monde (km) en coordonnées écran (pixels)
sf::Vector2f worldToScreen(Position pos) {
    return sf::Vector2f(
        DECALAGE_GAUCHE + static_cast<float>(pos.getX()) * ECHELLE,
        DECALAGE_DROITE - static_cast<float>(pos.getY()) * ECHELLE 
    );
}

// Adapte l'image de fond à la taille de la fenêtre
void adapterFondFenetre(sf::Sprite& sprite, const sf::Texture& texture) {
    sf::Vector2u size = texture.getSize();
    if (size.x > 0 && size.y > 0) {
        sprite.setTexture(texture);
        sprite.setScale({ 1.f, 1.f });
        // Calcul du ratio pour remplir la fenêtre
        sprite.setScale({ (float)WINDOW_WIDTH / size.x, (float)WINDOW_HEIGHT / size.y });
    }
}

// Affiche les aéroports sur la carte globale
void dessinerAeroports(sf::RenderWindow& window, const std::vector<Aeroport*>& aeroports, const sf::Font& police, bool Police) {
    for (auto aero : aeroports) {
        sf::Vector2f p = worldToScreen(aero->position);

        // Point central de l'aéroport
        sf::CircleShape point(5.f);
        point.setFillColor(sf::Color::Red);
        point.setOrigin({ 5.f, 5.f });
        point.setPosition(p);
        window.draw(point);

        // Zone de contrôle aérien (cercle transparent)
        float rayonVisuel = aero->rayonControle * ECHELLE;
        sf::CircleShape zone(rayonVisuel);
        zone.setFillColor(sf::Color(255, 0, 0, 30));
        zone.setOutlineColor(sf::Color::Red);
        zone.setOutlineThickness(1.f);
        zone.setOrigin({ rayonVisuel, rayonVisuel });
        zone.setPosition(p);
        window.draw(zone);

        // Affichage du nom si la police est chargée
        if (Police) {
            sf::Text text(police, aero->nom, 12);
            text.setPosition({ p.x + 10.f, p.y - 10.f });
            text.setFillColor(sf::Color::White);
            window.draw(text);
        }
    }
}

// Affiche les détails d'un aéroport (piste, parkings) lors du zoom
void dessinerDetailsAeroport(sf::RenderWindow& window, Aeroport* aero, const sf::Font& police, bool Police, float zoom) {
    // Dessin de la piste
    Position posPiste = aero->twr->getPositionPiste();
    Position posVisuellePiste = posPiste + Position(750, 0, 0);
    sf::Vector2f pPiste = worldToScreen(posVisuellePiste);

    sf::Vector2f taillePiste = { 1500.f * ECHELLE, 60.f * ECHELLE };
    sf::RectangleShape rectPiste(taillePiste);
    rectPiste.setOrigin({ taillePiste.x / 2.f, taillePiste.y / 2.f });
    rectPiste.setPosition(pPiste);
    rectPiste.setFillColor(sf::Color(80, 80, 80));
    rectPiste.setOutlineColor(sf::Color::Black);
    rectPiste.setOutlineThickness(1.f * zoom);
    window.draw(rectPiste);

    // Dessin des parkings (Vert = Libre, Rouge = Occupé)
    for (const auto& parking : aero->parkings) {
        sf::Vector2f pParking = worldToScreen(parking.getPosition());
        float tailleParking = 150.f * ECHELLE;
        
        sf::RectangleShape rectParking({ tailleParking, tailleParking });
        rectParking.setOrigin({ tailleParking / 2.f, tailleParking / 2.f });
        rectParking.setPosition(pParking);

        if (parking.estOccupe()) {
            rectParking.setFillColor(sf::Color(200, 0, 0, 150));
            rectParking.setOutlineColor(sf::Color::Red);
        } else {
            rectParking.setFillColor(sf::Color(0, 200, 0, 150));
            rectParking.setOutlineColor(sf::Color::Green);
        }
        rectParking.setOutlineThickness(1.f * zoom);
        window.draw(rectParking);

        // Nom du parking
        if (Police) {
            sf::Text txtPkg(police, parking.getNom(), 8);
            txtPkg.setScale({ zoom, zoom });
            txtPkg.setPosition({ pParking.x - tailleParking / 2, pParking.y - tailleParking });
            txtPkg.setFillColor(sf::Color::Black);
            window.draw(txtPkg);
        }
    }
}

// Affiche un avion avec rotation et couleur selon statut
void dessinerAvion(sf::RenderWindow& window, Avion* avion, const sf::Texture& texture, bool hasTexture, const sf::Font& police, bool Police, float zoom, Avion* selection, Aeroport* vue) {
    sf::Vector2f screenPos = worldToScreen(avion->getPosition());

    if (hasTexture) {
        sf::Sprite spriteAvion(texture);
        sf::Vector2u tailleImg = texture.getSize();
        spriteAvion.setOrigin({ (float)tailleImg.x / 2.f, (float)tailleImg.y / 2.f });
        spriteAvion.setPosition(screenPos);

        // Ajustement de la taille selon le niveau de zoom
        float scaleFactor = 0.05f * zoom;
        if (vue != nullptr) scaleFactor *= 0.5f;
        spriteAvion.setScale({ scaleFactor, scaleFactor });

        // Calcul de l'orientation de l'avion selon sa trajectoire
        float angleDeg = 0.0f;
        auto traj = avion->getTrajectoire();
        if (!traj.empty()) {
            Position cible = traj.front();
            sf::Vector2f posCibleEcran = worldToScreen(cible);
            float dx = posCibleEcran.x - screenPos.x;
            float dy = posCibleEcran.y - screenPos.y;
            if (std::abs(dx) > 0.1f || std::abs(dy) > 0.1f) {
                angleDeg = std::atan2(dy, dx) * 180.f / PI + 90.f; // +90 car sprite orienté vers le haut
            }
        }
        spriteAvion.setRotation(sf::degrees(angleDeg));

        // Couleur selon statut
        if (avion->estEnUrgence()) spriteAvion.setColor(sf::Color::Red);
        else if (avion == selection) spriteAvion.setColor(sf::Color::Green);
        else spriteAvion.setColor(sf::Color::White);

        // Affichage si dans la vue (optimisation)
        if (vue == nullptr || avion->getPosition().distance(vue->position) < 10000.0f) {
            window.draw(spriteAvion);
            // Affichage du nom en vue zoomée
            if (Police && vue != nullptr) {
                sf::Text nom(police, avion->getNom(), 8);
                nom.setScale({ zoom, zoom });
                nom.setPosition({ screenPos.x + 10.f * zoom, screenPos.y - 10.f * zoom });
                nom.setFillColor(sf::Color::Black);
                window.draw(nom);
            }
        }
    } else {
        // Affichage d'un point si l'image d'avion est manquante
        float rayonBase = (vue == nullptr) ? 6.f : 4.f;
        sf::CircleShape rond(rayonBase);
        rond.setOrigin({ rayonBase, rayonBase });
        rond.setPosition(screenPos);
        rond.setScale({ zoom, zoom });
        if (avion->estEnUrgence()) rond.setFillColor(sf::Color::Red);
        else if (avion == selection) rond.setFillColor(sf::Color::Green);
        else rond.setFillColor(sf::Color::Cyan);
        window.draw(rond);
    }
}

// Affiche la fenêtre d'informations pour l'avion sélectionné
void dessinerInfoBulle(sf::RenderWindow& window, Avion* avion, const sf::Font& police, float zoom) {
    sf::Vector2f screenPos = worldToScreen(avion->getPosition());
    sf::Vector2f tailleBox = { 240.f, 140.f };
    
    // Fond semi-transparent
    sf::RectangleShape infoBox(tailleBox);
    infoBox.setFillColor(sf::Color(0, 0, 0, 200));
    infoBox.setOutlineColor(sf::Color::White);
    infoBox.setOutlineThickness(1.f);
    infoBox.setScale({ zoom, zoom });
    infoBox.setPosition(screenPos + sf::Vector2f(20.f * zoom, -60.f * zoom));
    window.draw(infoBox);

    // Construction du texte d'information
    std::stringstream ss;
    ss << "VOL: " << avion->getNom() << "\n"
       << "Dest: " << (avion->getDestination() ? avion->getDestination()->nom : "N/A") << "\n"
       << "Alt: " << (int)avion->getPosition().getAltitude() << " m\n"
       << "Fuel: " << (int)avion->getCarburant() << " L\n";

    // Vitesse affichée selon l'état (Sol vs Vol)
    float vit = 0.f;
    EtatAvion e = avion->getEtat();
    if (e == EtatAvion::ROULE_VERS_PISTE || e == EtatAvion::ROULE_VERS_PARKING) vit = avion->getVitesseSol();
    else if (e != EtatAvion::STATIONNE && e != EtatAvion::EN_ATTENTE_DECOLLAGE && e != EtatAvion::EN_ATTENTE_PISTE) vit = avion->getVitesse() / 2.f;
    
    ss << "Vit: " << (int)vit << " km/h\n";
    
    if (avion->estEnUrgence()) ss << "URGENCE ACTIVE\n";
    
    // Traduction de l'état en texte lisible
    std::string etatStr = "Inconnu";
    switch (e) {
        case EtatAvion::STATIONNE: etatStr = "Stationne"; break;
        case EtatAvion::EN_ATTENTE_DECOLLAGE: etatStr = "Attente Decollage"; break;
        case EtatAvion::ROULE_VERS_PISTE: etatStr = "Roule vers Piste"; break;
        case EtatAvion::EN_ATTENTE_PISTE: etatStr = "Seuil Piste"; break;
        case EtatAvion::DECOLLAGE: etatStr = "Decollage"; break;
        case EtatAvion::EN_ROUTE: etatStr = "En Croisiere"; break;
        case EtatAvion::EN_APPROCHE: etatStr = "Approche"; break;
        case EtatAvion::EN_ATTENTE_ATTERRISSAGE: etatStr = "Circuit Attente"; break;
        case EtatAvion::ATTERRISSAGE: etatStr = "Atterrissage"; break;
        case EtatAvion::ROULE_VERS_PARKING: etatStr = "Roule vers Parking"; break;
        case EtatAvion::TERMINE: etatStr = "Termine"; break;
        default: break;
    }
    ss << "Etat: " << etatStr;

    sf::Text text(police, ss.str(), 14);
    text.setScale({ zoom, zoom });
    text.setPosition(infoBox.getPosition() + sf::Vector2f(10.f * zoom, 10.f * zoom));
    text.setFillColor(avion->estEnUrgence() ? sf::Color::Red : sf::Color::White);
    window.draw(text);
}