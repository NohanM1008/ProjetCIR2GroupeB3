#include "avion.hpp"

Position::Position(double x, double y, double z) : x_(x), y_(y), altitude_(z) {}

double Position::getX() const { return x_; }
double Position::getY() const { return y_; }
double Position::getAltitude() const { return altitude_; }
void Position::setPosition(double x, double y, double alt) { x_ = x; y_ = y; altitude_ = alt; }

// Calcul de la distance en 3D entre deux positions
double Position::distance(const Position& other) const {
    // Utilisation de l'opérateur - pour simplifier
    Position diff = *this - other;
    return std::sqrt(diff.x_ * diff.x_ + diff.y_ * diff.y_ + diff.altitude_ * diff.altitude_);
}


// Addition vectorielle de deux positions
Position Position::operator+(const Position& other) const {
    return Position(x_ + other.x_, y_ + other.y_, altitude_ + other.altitude_);
}

// Soustraction vectorielle
Position Position::operator-(const Position& other) const {
    return Position(x_ - other.x_, y_ - other.y_, altitude_ - other.altitude_);
}

// Multiplication par un scalaire
Position Position::operator*(double scalar) const {
    return Position(x_ * scalar, y_ * scalar, altitude_ * scalar);
}

// Comparaison d'égalité avec tolérance pour les nombres flottants
bool Position::operator==(const Position& other) const {
    const double epsilon = 0.001;
    return std::abs(x_ - other.x_) < epsilon &&
        std::abs(y_ - other.y_) < epsilon &&
        std::abs(altitude_ - other.altitude_) < epsilon;
}