#pragma once
#include "avion.hpp"
#include <vector>

// Met en pause le thread courant pour une durée (en millisecondes)
void simuler_pause(int ms);

// Routine CCR
void routine_ccr(CCR& ccr);

// Routine TWR
void routine_twr(TWR& twr);

// Routine APP
void routine_app(APP& app);

// Routine pour chaque avion
void routine_avion(Avion& avion, Aeroport& depart, Aeroport& arrivee, CCR& ccr, std::vector<Aeroport*> aeroports);