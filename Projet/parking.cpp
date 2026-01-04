#include "avion.hpp"

Parking::Parking(std::string nom, Position pos) : nom_(nom), position_(pos), occupe_(false) {}

bool Parking::estOccupe() const { return occupe_; }
void Parking::occuper() { occupe_ = true; }
void Parking::liberer() { occupe_ = false; }
Position Parking::getPosition() const { return position_; }
std::string Parking::getNom() const { return nom_; }

// Calcule la distance entre le parking et la piste (utile pour la priorité au décollage)
double Parking::getDistancePiste(Position posPiste) const {
    return position_.distance(posPiste);
}

// Comparaison basée sur le nom 
bool Parking::operator==(const Parking& other) const {
    return this->nom_ == other.nom_;
}