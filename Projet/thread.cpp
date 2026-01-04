#pragma warning(disable: 4828)
#include "thread.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <cmath>

#define PROBA_URGENCE 1500 // Probabilité d'urgence (1 chance sur 1500 par cycle)

// Fonction pour mettre en pause le thread courant
void simuler_pause(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Routine du Centre de Contrôle Régional (CCR)
void routine_ccr(CCR& ccr) {
    while (true) {
        ccr.gererEspaceAerien(); // Gestion des collisions et transferts
        simuler_pause(50);
    }
}

// Routine de la Tour de Contrôle (TWR)
void routine_twr(TWR& twr) {
    while (true) {
        simuler_pause(500);
        // Gestion des décollages si la piste est libre et pas d'urgence
        Avion* avionPret = twr.choisirAvionPourDecollage();

        if (avionPret != nullptr) {
            if (!twr.estUrgenceEnCours()) {
                twr.autoriserDecollage(avionPret);
            }
        }
    }
}

// Routine du Contrôle d'Approche (APP)
void routine_app(APP& app) {
    while (true) {
        app.mettreAJour(); // Gestion des atterrissages et files d'attente
        simuler_pause(500);
    }
}

// Routine principale simulant le comportement d'un avion (un thread par avion)
void routine_avion(Avion& avion, Aeroport& depart, Aeroport& arrivee, CCR& ccr, std::vector<Aeroport*> aeroports) {

    // Initialisation des générateurs aléatoires pour les urgences et destinations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distUrgence(0, PROBA_URGENCE);
    std::uniform_int_distribution<> distType(0, 1);
    std::uniform_int_distribution<> distDest(0, (int)aeroports.size() - 1);

    Aeroport* aeroDepart = &depart;
    Aeroport* aeroArrivee = &arrivee;

    APP* appArrivee = aeroArrivee->app;
    TWR* twrArrivee = aeroArrivee->twr;

    float dt = 1.f; // Pas de temps pour la simulation physique

    // Gestion de l'état pour éviter les libérations multiples de la piste
    EtatAvion dernierEtat = EtatAvion::TERMINE;
    bool LiberePiste = false;

    // Boucle de "vie" de l'avion
    while (avion.getEtat() != EtatAvion::TERMINE) {

        EtatAvion etat = avion.getEtat();

        // Réinitialisation si l'état change
        if (etat != dernierEtat) {
            dernierEtat = etat;
            LiberePiste = false;
        }

        // Détection du décollage (encore au sol)
        bool decollageAuSol = (etat == EtatAvion::DECOLLAGE && avion.getPosition().getAltitude() < 10.0f);

        // Gestion des mouvements

        if (etat == EtatAvion::ROULE_VERS_PARKING || etat == EtatAvion::ROULE_VERS_PISTE) {
            avion.avancerSol(dt);

            // Libération de la piste une fois dégagée après atterrissage
            if (etat == EtatAvion::ROULE_VERS_PARKING) {
                if (!LiberePiste) {
                    float yPiste = static_cast<float>(twrArrivee->getPositionPiste().getY());
                    // Si l'avion s'est suffisamment éloigné de l'axe de la piste
                    if (std::abs(avion.getPosition().getY() - yPiste) > 50.0f) {
                        twrArrivee->libererPiste();
                        LiberePiste = true; // Marqué comme fait
                    }
                }
            }

            // si on arrive au parking sans avoir libéré, on libère maintenant
            if (etat == EtatAvion::ROULE_VERS_PARKING && avion.getEtat() == EtatAvion::STATIONNE) {
                if (!LiberePiste) {
                    twrArrivee->libererPiste();
                    LiberePiste = true;
                }
            }
        }
        else if (decollageAuSol) {
            avion.avancerSol(dt * 15.0f); // Accélération sur la piste
        }
        else if (etat != EtatAvion::STATIONNE && etat != EtatAvion::EN_ATTENTE_DECOLLAGE && etat != EtatAvion::EN_ATTENTE_PISTE) {
            avion.avancer(dt); // Vol normal
        }

        // Gestion des états

        if (etat == EtatAvion::EN_APPROCHE) {
            // Arrivée en fin de trajectoire d'approche, demande atterrissage
            if (avion.getTrajectoire().empty()) {
                bool autorise = appArrivee->demanderAutorisationAtterrissage(&avion);
                if (!autorise) appArrivee->mettreEnAttente(&avion);
            }
        }

        else if (etat == EtatAvion::ATTERRISSAGE) {
            // Fin de l'atterrissage, demande de parking
            if (avion.getTrajectoire().empty()) {
                Parking* p = twrArrivee->choisirParkingLibre();

                if (p) {
                    twrArrivee->attribuerParking(&avion, p);
                    twrArrivee->gererRoulageVersParking(&avion, p);
                }
                else {
                    // Cas très rare, atterrissage mais sans parking disponible, l'avion bloque alors la piste on le fait disparaître
                    twrArrivee->libererPiste();
                    simuler_pause(3000);
                    avion.setEtat(EtatAvion::TERMINE);
                }
            }
        }

        else if (etat == EtatAvion::STATIONNE) {
            // Phase au sol

            simuler_pause(3000);
            // Gestion des urgences déclarées en vol
            if (avion.estEnUrgence()) {
                if (avion.getTypeUrgence() == TypeUrgence::PANNE_MOTEUR) {
                    Logs::getLogs().log("MAINTENANCE", "Reparation", "Moteur en cours de reparation sur " + avion.getNom());
                    simuler_pause(5000);
                }
                else if (avion.getTypeUrgence() == TypeUrgence::MEDICAL) {
                    Logs::getLogs().log("MAINTENANCE", "Evacuation", "Passager malade debarque de " + avion.getNom());
                    simuler_pause(2000);
                }
            }
            avion.effectuerMaintenance();

            // Recherche d'une nouvelle destination valide
            Aeroport* nouvelleDestination = nullptr;
            bool planDeVolValide = false;

            while (!planDeVolValide) {
                nouvelleDestination = aeroArrivee;
                do {
                    int idx = distDest(gen);
                    nouvelleDestination = aeroports[idx];
                } while (nouvelleDestination == aeroArrivee); // Eviter vol sur place

                // Validation auprès du CCR (créneaux horaires)
                if (ccr.validerPlanDeVol(aeroArrivee, nouvelleDestination)) {
                    planDeVolValide = true;
                }
                else {
                    std::cout << "[CCR] Planning : Vol " << aeroArrivee->nom << " -> " << nouvelleDestination->nom << " refuse (creneau indisponible). Recherche d'un autre itineraire\n";
                    simuler_pause(1000);
                }
            }

            // Mise à jour des paramètres pour le nouveau vol
            aeroDepart = aeroArrivee;
            aeroArrivee = nouvelleDestination;

            appArrivee = aeroArrivee->app;
            TWR* twrActuelle = aeroDepart->twr;

            avion.setDestination(aeroArrivee);
            std::cout << "[AVION] " << avion.getNom() << " : Nouvel itineraire valide vers " << aeroArrivee->nom << ".\n";

            twrActuelle->enregistrerPourDecollage(&avion);

            // Attente passive jusqu'au décollage
            while (avion.getEtat() == EtatAvion::EN_ATTENTE_DECOLLAGE) {
                simuler_pause(200);
            }
        }
        else if (etat == EtatAvion::DECOLLAGE) {
            TWR* twrActuelle = aeroDepart->twr;

            // Transfert de la tour à CCR une fois en l'air
            if (avion.getPosition().getAltitude() > 1000) {
                twrActuelle->retirerAvionDeDecollage(&avion);

                std::cout << "[AVION] " << avion.getNom() << " quitte la zone et passe en croisiere.\n";

                ccr.prendreEnCharge(&avion);

                // Mise à jour des contrôleurs pour l'arrivée
                twrArrivee = aeroArrivee->twr;
                appArrivee = aeroArrivee->app;
            }
        }

        // Simulation d'incidents aléatoires en vol
        if (etat == EtatAvion::EN_ROUTE || etat == EtatAvion::EN_APPROCHE) {
            if (!avion.estEnUrgence() && (distUrgence(gen) == 0)) {
                if (distType(gen) == 0) {
                    avion.declarerUrgence(TypeUrgence::MEDICAL);
                }
                else {
                    avion.declarerUrgence(TypeUrgence::PANNE_MOTEUR);
                }
            }
        }

        simuler_pause(75);
    }
}