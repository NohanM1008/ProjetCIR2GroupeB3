#include "avion.hpp"
#include <map>
#include <stdexcept>
#include <chrono> 

// Structure pour identifier un trajet unique (du départ à l'arrivée)
struct TrajetKey {
    std::string depart;
    std::string arrivee;
    bool operator<(const TrajetKey& other) const {
        if (depart != other.depart) return depart < other.depart;
        return arrivee < other.arrivee;
    }
};

// Gestion globale du planning des vols pour éviter la saturation (que trop d'avions aillent vers un même aéroport)
static std::map<TrajetKey, std::chrono::steady_clock::time_point> planningVols;
static std::mutex mutexPlanning;
const auto DELAI_MIN_ENTRE_VOLS = std::chrono::seconds(15); 

CCR::CCR() {}

bool CCR::validerPlanDeVol(Aeroport* depart, Aeroport* arrivee) {
    std::lock_guard<std::mutex> lock(mutexPlanning);
    if (!depart || !arrivee) throw std::invalid_argument("Aeroport NULL");
    
    // Refus si l'aéroport d'arrivée est déjà saturé (file d'attente non vide)
    if (arrivee->app->getNombreAvionsEnAttente() > 0) return false;

    // Vérification de la disponibilité d'au moins un parking à l'arrivée
    bool parkingDispo = false;
    for (const auto& p : arrivee->parkings) {
        if (!p.estOccupe()) { parkingDispo = true; break; }
    }
    if (!parkingDispo) return false;

    // Vérification du délai minimum entre deux vols sur le même trajet
    TrajetKey key = { depart->nom, arrivee->nom };
    auto now = std::chrono::steady_clock::now();
    if (planningVols.find(key) != planningVols.end()) {
        if (now - planningVols[key] < DELAI_MIN_ENTRE_VOLS) return false; 
    }
    
    // Validation et enregistrement du vol
    planningVols[key] = now;
    return true;
}

void CCR::prendreEnCharge(Avion* avion) {
    std::lock_guard<std::mutex> lock(mutexCCR_);
    if (!avion) throw std::invalid_argument("Avion NULL");

    avionsEnCroisiere_.push_back(avion); // Ajout à la liste des avions gérés par le CCR
    avion->setEtat(EtatAvion::EN_ROUTE);

    // Calcul de la trajectoire de croisière
    if (avion->getDestination()) {
        std::vector<Position> route;
        Position dep = avion->getPosition();
        Position dest = avion->getDestination()->position;
        Position vec = dest - dep;
        
        // Point intermédiaire pour simuler une montée/descente progressive
        Position pt1 = dep + (vec * 0.33);
        pt1.setPosition(pt1.getX(), pt1.getY(), 10000.0); // Altitude de croisière
        
        Position pt2 = dest;
        pt2.setPosition(pt2.getX(), pt2.getY(), 10000.0);

        route.push_back(pt1);
        route.push_back(pt2);
        avion->setTrajectoire(route);
    }
    std::cout << "[CCR] Prise en charge " << avion->getNom() << ".\n";
    Logs::getLogs().log("CCR", "Prise en charge", "Avion " + avion->getNom());
}

void CCR::transfererVersApproche(Avion* avion, APP* appCible) {
    if (!avion || !appCible) throw std::invalid_argument("Avion ou APP NULL");
    std::cout << "[CCR] Transfert " << avion->getNom() << " vers APP.\n";
    
    appCible->ajouterAvion(avion); // Transfert de responsabilité à l'APP
    avion->setEtat(EtatAvion::EN_APPROCHE);
    
    // Gestion immédiate si urgence, sinon procédure standard
    if (avion->estEnUrgence()) appCible->gererUrgence(avion);
    else appCible->assignerTrajectoireApproche(avion);
    
    Logs::getLogs().log("CCR", "Transfert vers APP", "Avion " + avion->getNom());
}

void CCR::gererEspaceAerien() {
    std::lock_guard<std::mutex> lock(mutexCCR_);

    // Détection et résolution des conflits (collisions)
    for (size_t i = 0; i < avionsEnCroisiere_.size(); ++i) {
        for (size_t j = i + 1; j < avionsEnCroisiere_.size(); ++j) {
            Avion* a1 = avionsEnCroisiere_[i];
            Avion* a2 = avionsEnCroisiere_[j];
            
            // Si différence d'altitude suffisante, pas de conflit
            if (std::abs(a1->getPosition().getAltitude() - a2->getPosition().getAltitude()) >= 1000) continue;

            // Si trop proches horizontalement, résolution par changement d'altitude
            if (a1->getPosition().distance(a2->getPosition()) < 20000.0) {
                std::cout << "[CCR] Alerte collision : " << a1->getNom() << " / " << a2->getNom() << ".\n";
                Position p1 = a1->getPosition();
                a1->setPosition({p1.getX(), p1.getY(), p1.getAltitude() + 500});
                Position p2 = a2->getPosition();
                a2->setPosition({p2.getX(), p2.getY(), p2.getAltitude() - 500});
            }
        }
    }

    // Gestion des transferts vers l'approche (APP)
    for (auto it = avionsEnCroisiere_.begin(); it != avionsEnCroisiere_.end(); ) {
        Avion* avion = *it;
        Aeroport* dest = avion->getDestination();

        if (!dest || avion->getEtat() != EtatAvion::EN_ROUTE) {
            ++it;
            continue;
        }

        double dist = avion->getPosition().distance(dest->position);
        // Transfert si proche de la destination ou en urgence
        if (avion->estEnUrgence() || dist <= dest->rayonControle) {
            transfererVersApproche(avion, dest->app);
            it = avionsEnCroisiere_.erase(it); // Retrait de la liste CCR
        } else {
            ++it;
        }
    }
}

Aeroport::Aeroport(std::string n, Position pos, float r) : nom(n), position(pos), rayonControle(r) {
    Position posPiste(pos.getX(), pos.getY(), 0);
    // Création des parkings par défaut pour chaque aéroport
    parkings.push_back(Parking(n + "-P1", pos + Position(100, 400, 0)));
    parkings.push_back(Parking(n + "-P2", pos + Position(300, 400, 0)));
    parkings.push_back(Parking(n + "-P3", pos + Position(500, 400, 0)));
    parkings.push_back(Parking(n + "-P4", pos + Position(700, 400, 0)));
    parkings.push_back(Parking(n + "-P5", pos + Position(900, 400, 0)));
    
    // Initialisation des contrôleurs (TWR et APP)
    twr = new TWR(parkings, posPiste, 5000.f);
    app = new APP(twr);
}
