#include "avion.hpp"
#include <stdexcept>

Avion::Avion(std::string n, float v, float vSol, float c, float conso, float dureeStat, Position pos)
    : nom_(n), vitesse_(v), vitesseSol_(vSol), carburant_(c), conso_(conso),
    dureeStationnement_(dureeStat), pos_(pos), etat_(EtatAvion::STATIONNE),
    parking_(nullptr), destination_(nullptr), typeUrgence_(TypeUrgence::AUCUNE) {
    
    if (v <= 0 || vSol <= 0) throw std::invalid_argument("Vitesse avion invalide (<= 0)");
    if (c < 0) throw std::invalid_argument("Carburant initial negatif");
    if (conso < 0) throw std::invalid_argument("Consommation negative");
}

std::string Avion::getNom() const { return nom_; }
float Avion::getVitesse() const { std::lock_guard<std::mutex> lock(mtx_); return vitesse_; }
float Avion::getVitesseSol() const { std::lock_guard<std::mutex> lock(mtx_); return vitesseSol_; }
float Avion::getCarburant() const { std::lock_guard<std::mutex> lock(mtx_); return carburant_; }
float Avion::getConsommation() const { std::lock_guard<std::mutex> lock(mtx_); return conso_; }
Position Avion::getPosition() const { std::lock_guard<std::mutex> lock(mtx_); return pos_; }
EtatAvion Avion::getEtat() const { std::lock_guard<std::mutex> lock(mtx_); return etat_; }
Parking* Avion::getParking() const { std::lock_guard<std::mutex> lock(mtx_); return parking_; }
Aeroport* Avion::getDestination() const { std::lock_guard<std::mutex> lock(mtx_); return destination_; }
float Avion::getDureeStationnement() const { std::lock_guard<std::mutex> lock(mtx_); return dureeStationnement_; }
bool Avion::estEnUrgence() const { std::lock_guard<std::mutex> lock(mtx_); return typeUrgence_ != TypeUrgence::AUCUNE; }
TypeUrgence Avion::getTypeUrgence() const { std::lock_guard<std::mutex> lock(mtx_); return typeUrgence_; }
const std::vector<Position> Avion::getTrajectoire() const { std::lock_guard<std::mutex> lock(mtx_); return trajectoire_; }

void Avion::setPosition(const Position& p) { std::lock_guard<std::mutex> lock(mtx_); pos_ = p; }
void Avion::setTrajectoire(const std::vector<Position>& traj) { std::lock_guard<std::mutex> lock(mtx_); trajectoire_ = traj; }
void Avion::setEtat(EtatAvion e) { std::lock_guard<std::mutex> lock(mtx_); etat_ = e; }
void Avion::setParking(Parking* p) { std::lock_guard<std::mutex> lock(mtx_); parking_ = p; }
void Avion::setDestination(Aeroport* dest) { std::lock_guard<std::mutex> lock(mtx_); destination_ = dest; }

void Avion::avancer(float dt) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (trajectoire_.empty()) return; // Pas de mouvement si pas de trajectoire

    // Gestion de la consommation de carburant
    float consommationRequise = conso_ * dt;
    if (carburant_ < consommationRequise) {
        carburant_ = 0;
        etat_ = EtatAvion::TERMINE; // L'avion s'écrase
        std::cout << "[AVION " << nom_ << "] CRASH : Plus de carburant\n";
        Logs::getLogs().log("AVION", "CRASH", "Avion " + nom_ + " crash.");
        return;
    }

    Position cible = trajectoire_.front();
    
    // Calcul du vecteur direction et de la distance vers le prochain point
    Position direction = cible - pos_;
    double dist = std::sqrt(direction.getX() * direction.getX() + 
                            direction.getY() * direction.getY() + 
                            direction.getAltitude() * direction.getAltitude());
    
    float distance_a_parcourir = vitesse_ * dt;

    // Déplacement de l'avion
    if (dist <= distance_a_parcourir) {
        pos_ = cible; // On atteint le point exact
        trajectoire_.erase(trajectoire_.begin()); // On passe au point suivant
    }
    else {
        pos_ = pos_ + (direction * (distance_a_parcourir / dist)); // On avance vers le point
    }

    carburant_ -= consommationRequise;

    // Détection urgence carburant
    if (carburant_ < 1000 && typeUrgence_ == TypeUrgence::AUCUNE) {
        typeUrgence_ = TypeUrgence::CARBURANT;
        std::cout << "[AVION " << nom_ << "] Urgence CARBURANT (< 1000L)\n";
    }
}

void Avion::avancerSol(float dt) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (trajectoire_.empty()) return;

    // Consommation réduite au sol (5% de la conso normale)
    float consommationSol = conso_ * 0.05f;
    float consommationRequise = consommationSol * dt;

    if (carburant_ < consommationRequise) {
        carburant_ = 0;
        etat_ = EtatAvion::TERMINE;
        std::cout << "[AVION " << nom_ << "] Panne carburant au sol\n";
        return;
    }

    Position cible = trajectoire_.front();
    
    Position direction = cible - pos_;
    double dist = std::sqrt(direction.getX() * direction.getX() + 
                            direction.getY() * direction.getY() + 
                            direction.getAltitude() * direction.getAltitude());
    
    float distance_a_parcourir = vitesseSol_ * dt; // Utilisation de la vitesse au sol

    if (dist <= distance_a_parcourir) {
        pos_ = cible;
        trajectoire_.erase(trajectoire_.begin());

        // Logique de fin de trajectoire au sol
        if (trajectoire_.empty()) {
            if (etat_ == EtatAvion::ROULE_VERS_PISTE) {
                etat_ = EtatAvion::EN_ATTENTE_PISTE; // Prêt à décoller
                if (parking_) {
                    parking_->liberer(); // Libération du parking de départ
                    parking_ = nullptr;
                }
                std::cout << "[AVION " << nom_ << "] Arrive a la piste.\n";
            }
            else if (etat_ == EtatAvion::ROULE_VERS_PARKING) {
                etat_ = EtatAvion::STATIONNE; // Arrivée au parking final
                std::cout << "[AVION " << nom_ << "] Arrive au parking. Fin du vol.\n";
            }
        }
    }
    else {
        pos_ = pos_ + (direction * (distance_a_parcourir / dist));
    }

    carburant_ -= consommationRequise;
}

void Avion::declarerUrgence(TypeUrgence type) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (typeUrgence_ == TypeUrgence::AUCUNE) { // On ne déclare l'urgence que si pas déjà en urgence
        typeUrgence_ = type;
        std::string raison;
        switch (type) {
            case TypeUrgence::PANNE_MOTEUR: raison = "PANNE MOTEUR"; break;
            case TypeUrgence::MEDICAL: raison = "MEDICALE"; break;
            case TypeUrgence::CARBURANT: raison = "CARBURANT"; break;
            default: raison = "INCONNUE"; break;
        }
        std::cout << "[AVION " << nom_ << "] MAYDAY : Urgence " << raison << " !\n";
        Logs::getLogs().log("AVION", "URGENCE", "Urgence : " + raison);
    }
}

void Avion::effectuerMaintenance() {
    std::lock_guard<std::mutex> lock(mtx_);
    // Ravitaillement minimum garanti
    if (carburant_ < 10000.0f) carburant_ = 10000.0f;
    
    // Résolution des problèmes techniques ou médicaux
    if (typeUrgence_ != TypeUrgence::AUCUNE) {
        std::cout << "[AVION " << nom_ << "] Urgence resolue.\n";
        typeUrgence_ = TypeUrgence::AUCUNE;
    } else {
        std::cout << "[AVION " << nom_ << "] Ravitaillement complet.\n";
    }
}

// Opérateur de comparaison
bool Avion::operator==(const Avion& other) const {
    return this->nom_ == other.nom_;
}
