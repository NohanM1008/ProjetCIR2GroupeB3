#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <optional> 
#include <random>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include "avion.hpp"
#include "thread.hpp"
#include "sfml.hpp"

#ifdef __linux__
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

// pour définir le répertoire de travail
void setRepertoire(const char* executablePath) {
    std::filesystem::path path(executablePath);
    std::filesystem::current_path(path.parent_path());
}

// Variables globales pour la gestion des threads et des avions
std::vector<std::thread> threads_infra;
std::vector<Avion*> flotte;
std::mutex mutexFlotte;

// Variables globales pour l'interaction utilisateur
Avion* avionSelectionne = nullptr;
Aeroport* aeroportVue = nullptr;

int main(int argc, char* argv[]) {
    try {
        // Initialisation de l'aléatoire et du répertoire de travail
        std::srand(static_cast<unsigned int>(time(NULL)));
        if (argc > 0) setRepertoire(argv[0]);

        std::cout << "--- SIMULATION ---\n";

        // Création de la fenêtre SFML
        sf::RenderWindow window(sf::VideoMode({LARGEUR, HAUTEUR}), "Simulation");
        window.setFramerateLimit(60);

        // Configuration des vues (caméras)
        sf::View vueDefaut = window.getDefaultView();
        sf::View vueFrance = window.getDefaultView();
        float niveauZoomActuel = 1.0f;

        sf::Texture textureCarte, textureAvion;
        sf::Font police;
        bool TextureMap = textureCarte.loadFromFile("img/carte.jpg");

        // Chargement et traitement de l'image avion (transparence)
        sf::Image imageAvion;
        bool TextureAvion = false;
        if (imageAvion.loadFromFile("img/avion.png")) {
            imageAvion.createMaskFromColor(sf::Color::White);
            if (textureAvion.loadFromImage(imageAvion)) TextureAvion = true;
        }
        if (!TextureAvion) throw std::runtime_error("img/avion.png pas trouve");

        bool Police = police.openFromFile("img/arial.ttf");
        if (!Police) throw std::runtime_error("img/arial.ttf pas trouve");

        sf::Sprite spriteFond(textureCarte);
        if (TextureMap) adapterFondFenetre(spriteFond, textureCarte);

        CCR ccr;
        std::vector<Aeroport*> listeAeroports;
        std::vector<Avion*> avionsPretsAuDepart;

        // Lecture du fichier avec les infos de départ
        std::ifstream fichier("debut.txt");
        if (!fichier.is_open()) fichier.open("Projet/debut.txt");
        if (!fichier.is_open()) throw std::runtime_error("Fichier debut.txt pas trouve");

        std::string ligne, section;
        while (std::getline(fichier, ligne)) {
            if (ligne.empty() || ligne[0] == '#') continue;
            if (ligne.back() == '\r') ligne.pop_back();
            if (ligne == "[AEROPORTS]") { section = "AEROPORTS"; continue; }
            if (ligne == "[AVIONS]") { section = "AVIONS"; continue; }

            std::stringstream ss(ligne);
            if (section == "AEROPORTS") {
                // Chargement des aéroports
                std::string nom; double x, y; float r;
                ss >> nom >> x >> y >> r;
                if (!nom.empty()) listeAeroports.push_back(new Aeroport(nom, Position(x, y, 0), r));
            }
            else if (section == "AVIONS") {
                // Chargement des avions
                std::string nom, dep, dest;
                float v, vs, carb, conso, dur;
                ss >> nom >> v >> vs >> carb >> conso >> dur >> dep >> dest;

                Aeroport* d = nullptr; Aeroport* a = nullptr;
                for (auto aero : listeAeroports) {
                    if (aero->nom == dep) d = aero;
                    if (aero->nom == dest) a = aero;
                }
                if (d && a) {
                    Position p = d->position;
                    p.setPosition(p.getX(), p.getY() - 5000, 10000); // Position initiale décalée
                    Avion* av = new Avion(nom, v, vs, carb, conso, dur, p);
                    av->setDestination(a);
                    avionsPretsAuDepart.push_back(av);
                }
            }
        }
        fichier.close();
        if (listeAeroports.empty()) throw std::runtime_error("Aucun aeroport charge");

        // lancement des threads
        threads_infra.emplace_back(routine_ccr, std::ref(ccr));
        for (auto aero : listeAeroports) {
            threads_infra.emplace_back(routine_twr, std::ref(*aero->twr));
            threads_infra.emplace_back(routine_app, std::ref(*aero->app));
        }

        // Génère les avions
        std::thread trafficGenerator([&, avionsPretsAuDepart]() {
            for (Avion* avion : avionsPretsAuDepart) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500 + std::rand() % 1000));

                // Recherche de l'aéroport le plus proche pour le départ
                Aeroport* depart = nullptr;
                double distMin = 1e12;
                for (auto aero : listeAeroports) {
                    double d = avion->getPosition().distance(aero->position);
                    if (d < distMin) { distMin = d; depart = aero; }
                }

                if (depart && avion->getDestination()) {
                    ccr.prendreEnCharge(avion);
                    // Lancement du thread dédié à l'avion
                    std::thread t(routine_avion, std::ref(*avion), std::ref(*depart), std::ref(*avion->getDestination()), std::ref(ccr), listeAeroports);
                    t.detach();
                    {
                        std::lock_guard<std::mutex> lock(mutexFlotte);
                        flotte.push_back(avion);
                    }
                }
            }
            });
        trafficGenerator.detach();

        // Boucle principale d'affichage
        while (window.isOpen()) {
            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();
                else if (const auto* k = event->getIf<sf::Event::KeyPressed>()) {
                    if (k->code == sf::Keyboard::Key::Escape) window.close();
                }
                else if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (m->button == sf::Mouse::Button::Left) {
                        // Gestion du clic souris (sélection avion ou zoom aéroport)
                        window.setView(vueFrance);
                        sf::Vector2f mousePos = window.mapPixelToCoords(m->position);
                        bool clic = false;

                        {
                            std::lock_guard<std::mutex> lock(mutexFlotte);
                            for (auto avion : flotte) {
                                if (!avion || avion->getEtat() == EtatAvion::TERMINE) continue;
                                sf::Vector2f posAvion = conversion(avion->getPosition());
                                float dx = mousePos.x - posAvion.x;
                                float dy = mousePos.y - posAvion.y;
                                if (std::sqrt(dx * dx + dy * dy) < (30.f * niveauZoomActuel)) {
                                    avionSelectionne = avion;
                                    clic = true;
                                    break;
                                }
                            }
                        }

                        if (!clic) {
                            avionSelectionne = nullptr;
                            if (aeroportVue) {
                                // Dézoom (retour vue france)
                                aeroportVue = nullptr;
                                vueFrance = window.getDefaultView();
                                niveauZoomActuel = 1.0f;
                                if (TextureMap) adapterFondFenetre(spriteFond, textureCarte);
                            }
                            else {
                                // Zoom sur un aéroport
                                for (auto aero : listeAeroports) {
                                    sf::Vector2f posAero = conversion(aero->position);
                                    float dx = mousePos.x - posAero.x;
                                    float dy = mousePos.y - posAero.y;
                                    if (std::sqrt(dx * dx + dy * dy) < 50.f) {
                                        aeroportVue = aero;
                                        vueFrance.setCenter(posAero);
                                        niveauZoomActuel = 0.002f;
                                        vueFrance.setSize({ (float)LARGEUR * niveauZoomActuel, (float)HAUTEUR * niveauZoomActuel });
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            window.clear(sf::Color::White);

            // Dessin du fond
            window.setView(vueDefaut);
            if ((TextureMap && !aeroportVue) || (aeroportVue && !TextureMap)) {
                if (!aeroportVue) window.draw(spriteFond);
            }

            // Dessin de la france (aéroports et avions)
            window.setView(vueFrance);
            if (!aeroportVue) dessinerAeroports(window, listeAeroports, police, Police);
            else dessinerDetailsAeroport(window, aeroportVue, police, Police, niveauZoomActuel);

            // Dessin des avions
            {
                std::lock_guard<std::mutex> lock(mutexFlotte);
                for (auto avion : flotte) {
                    if (avion && avion->getEtat() != EtatAvion::TERMINE) {
                        dessinerAvion(window, avion, textureAvion, TextureAvion, police, Police, niveauZoomActuel, avionSelectionne, aeroportVue);
                        if (avion == avionSelectionne && Police) {
                            dessinerInfo(window, avion, police, niveauZoomActuel);
                        }
                    }
                }
            }
            window.display();
        }

        // Nettoyage et fermeture
        {
            std::lock_guard<std::mutex> lock(mutexFlotte);
            for (auto avion : flotte) {
                if (avion) { avion->setEtat(EtatAvion::TERMINE); delete avion; }
            }
            flotte.clear();
        }
        for (auto aero : listeAeroports) { delete aero->twr; delete aero->app; delete aero; }
    }
    catch (const std::exception& e) {
        std::cerr << "Erreur " << e.what() << "\n";
        return -1;
    }
    return 0;
}