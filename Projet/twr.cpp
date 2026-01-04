#include "avion.hpp"
#include <stdexcept>
#include <algorithm>
#include <sstream>

TWR::TWR(std::vector<Parking>& parkings, Position posPiste, float tempsAtterrissageDecollage)
    : pisteLibre_(true),
    parkings_(parkings),
    posPiste_(posPiste),
    tempsAtterrissageDecollage_(tempsAtterrissageDecollage),
    urgenceEnCours_(false),
    tourActuel_(Tour::DECOLLAGE),
    demandeAtterrissage_(false)
{
    if (parkings_.empty()) throw std::runtime_error("TWR initialisee sans parkings");
}

Position TWR::getPositionPiste() const {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    return posPiste_;
}

bool TWR::estPisteLibre() const { return pisteLibre_; }
void TWR::libererPiste() { pisteLibre_ = true; }
void TWR::reserverPiste() { pisteLibre_ = false; }

void TWR::setDemandeAtterrissage(bool statut) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    demandeAtterrissage_ = statut;
}

bool TWR::autoriserAtterrissage(Avion* avion) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    if (!avion) throw std::invalid_argument("Avion NULL");

    // Vérification de la disponibilité d'un parking
    bool parkingDispo = false;
    for (const auto& p : parkings_) {
        if (!p.estOccupe()) {
            parkingDispo = true;
            break;
        }
    }

    // Priorité absolue aux urgences
    if (avion->estEnUrgence()) {
        pisteLibre_ = false;
        avion->setEtat(EtatAvion::ATTERRISSAGE);
        return true;
    }

    // Refus si piste occupée ou pas de parking
    if (!pisteLibre_) {
        return false;
    }
    if (!parkingDispo) {
        return false;
    }

    // Vérification s'il y a des avions en attente de décollage
    bool avionsEnAttenteDecollage = false;
    for (auto* a : filePourDecollage_) {
        if (a->getEtat() == EtatAvion::EN_ATTENTE_PISTE) {
            avionsEnAttenteDecollage = true;
            break;
        }
    }

    // Gestion de l'alternance Décollage/Atterrissage
    if (tourActuel_ == Tour::DECOLLAGE && avionsEnAttenteDecollage) {
        return false; // Priorité aux décollages
    }

    // Autorisation accordée
    pisteLibre_ = false;
    tourActuel_ = Tour::DECOLLAGE; // Le prochain tour sera pour un décollage
    avion->setEtat(EtatAvion::ATTERRISSAGE);
    return true;
}

Parking* TWR::choisirParkingLibre() {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    for (auto& parking : parkings_) {
        if (!parking.estOccupe()) {
            return &parking;
        }
    }
    return nullptr;
}

void TWR::attribuerParking(Avion* avion, Parking* parking) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    if (!avion || !parking) throw std::invalid_argument("Avion ou parking NULL");

    avion->setParking(parking);
    parking->occuper();

    // Fin de l'état d'urgence une fois au sol et garé
    if (avion->estEnUrgence()) {
        urgenceEnCours_ = false;
    }

    std::stringstream ss;
    ss << "Avion " << avion->getNom() << " au parking " << parking->getNom();
    Logs::getLogs().log("TWR", "Parking", ss.str());
}

void TWR::gererRoulageVersParking(Avion* avion, Parking* parking) {
    if (!avion || !parking) throw std::invalid_argument("Avion ou parking NULL");

    // Création du chemin de roulage
    std::vector<Position> cheminRoulage;
    cheminRoulage.push_back(Position(posPiste_.getX(), posPiste_.getY(), 0)); // Sortie de piste
    cheminRoulage.push_back(Position(posPiste_.getX(), posPiste_.getY() + 200, 0)); // Voie de circulation
    cheminRoulage.push_back(Position(parking->getPosition().getX(), posPiste_.getY() + 200, 0)); // Approche parking
    cheminRoulage.push_back(parking->getPosition()); // Entrée parking

    avion->setTrajectoire(cheminRoulage);
    avion->setEtat(EtatAvion::ROULE_VERS_PARKING);
}

void TWR::enregistrerPourDecollage(Avion* avion) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    if (!avion) throw std::invalid_argument("Avion NULL");

    if (std::find(filePourDecollage_.begin(), filePourDecollage_.end(), avion) == filePourDecollage_.end()) {
        filePourDecollage_.push_back(avion);
        avion->setEtat(EtatAvion::EN_ATTENTE_DECOLLAGE);
    }
}

Avion* TWR::choisirAvionPourDecollage() {
    std::lock_guard<std::mutex> lock(mutexTWR_);

    if (filePourDecollage_.empty()) return nullptr;

    // Priorité 1 : Avion déjà au seuil de piste
    for (Avion* avion : filePourDecollage_) {
        if (avion->getEtat() == EtatAvion::EN_ATTENTE_PISTE) return avion;
    }

    // Si un avion roule déjà vers la piste, on attend qu'il arrive
    for (Avion* avion : filePourDecollage_) {
        if (avion->getEtat() == EtatAvion::ROULE_VERS_PISTE) return nullptr;
    }

    // Priorité 2 : Choisir l'avion le plus éloigné de la piste
    Avion* prioritaire = nullptr;
    double maxDistance = -1.0;

    for (Avion* avion : filePourDecollage_) {
        if (avion->getEtat() == EtatAvion::EN_ATTENTE_DECOLLAGE) {
            Parking* parking = avion->getParking();
            if (parking) {
                double distance = parking->getDistancePiste(posPiste_);
                if (distance > maxDistance) {
                    maxDistance = distance;
                    prioritaire = avion;
                }
            }
        }
    }

    // Lancement du roulage pour l'avion choisi
    if (prioritaire) {
        std::vector<Position> cheminRoulage;
        Parking* pkg = prioritaire->getParking();
        if (!pkg) throw std::logic_error("Parking NULL");

        // Chemin inverse du parking vers la piste
        cheminRoulage.push_back(Position(pkg->getPosition().getX(), posPiste_.getY() + 200, 0));
        cheminRoulage.push_back(Position(posPiste_.getX(), posPiste_.getY() + 200, 0));
        cheminRoulage.push_back(Position(posPiste_.getX(), posPiste_.getY(), 0));

        prioritaire->setTrajectoire(cheminRoulage);
        prioritaire->setEtat(EtatAvion::ROULE_VERS_PISTE);

        return nullptr; // Retourne null car l'avion n'est pas encore prêt à décoller (il roule)
    }
    return nullptr;
}

bool TWR::autoriserDecollage(Avion* avion) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    if (!avion) throw std::invalid_argument("Avion NULL");

    if (urgenceEnCours_) return false; // Blocage total si urgence en cours

    if (avion->getEtat() == EtatAvion::EN_ATTENTE_PISTE) {
        if (!pisteLibre_) return false;

        // Vérification disponibilité parking (pour éviter blocage si atterrissage forcé)
        bool parkingDispo = false;
        for (const auto& p : parkings_) {
            if (!p.estOccupe()) {
                parkingDispo = true;
                break;
            }
        }

        // Gestion de l'alternance Atterrissage/Décollage
        if (tourActuel_ == Tour::ATTERRISSAGE && demandeAtterrissage_) {
            if (parkingDispo) {
                return false; // Priorité aux atterrissages
            }
        }

        // Autorisation accordée
        pisteLibre_ = false;
        tourActuel_ = Tour::ATTERRISSAGE; // Le prochain tour sera pour un atterrissage

        // Trajectoire de montée initiale
        std::vector<Position> trajMontee;
        trajMontee.push_back(Position(posPiste_.getX() + 1500, posPiste_.getY(), 0));
        trajMontee.push_back(Position(posPiste_.getX() + 21500, posPiste_.getY(), 3000));

        avion->setTrajectoire(trajMontee);
        avion->setEtat(EtatAvion::DECOLLAGE);

        std::stringstream ss;
        ss << "Decollage immediat pour " << avion->getNom();
        Logs::getLogs().log("TWR", "Decollage", ss.str());

        return true;
    }
    return false;
}

void TWR::retirerAvionDeDecollage(Avion* avion) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    if (!avion) return;

    auto it = std::find(filePourDecollage_.begin(), filePourDecollage_.end(), avion);
    if (it != filePourDecollage_.end()) {
        filePourDecollage_.erase(it);
        pisteLibre_ = true; // Libération de la piste une fois l'avion en l'air
    }
}

void TWR::setUrgenceEnCours(bool statut) {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    urgenceEnCours_ = statut;
}

bool TWR::estUrgenceEnCours() const {
    std::lock_guard<std::mutex> lock(mutexTWR_);
    return urgenceEnCours_;
}