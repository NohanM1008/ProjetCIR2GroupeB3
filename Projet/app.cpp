#include "avion.hpp"
#include <stdexcept>
#include <algorithm>

APP::APP(TWR* tour) : twr_(tour) {
    if (!tour) throw std::invalid_argument("pointeur TWR NULL");
}

size_t APP::getNombreAvionsDansZone() const { return avionsDansZone_.size(); }
size_t APP::getNombreAvionsEnAttente() const { return fileAttenteAtterrissage_.size(); }

void APP::ajouterAvion(Avion* avion) {
    std::lock_guard<std::recursive_mutex> lock(mutexAPP_);
    if (!avion) throw std::invalid_argument("Avion NULL");
    if (std::find(avionsDansZone_.begin(), avionsDansZone_.end(), avion) == avionsDansZone_.end()) { // Si l'avion n'est pas dans la zone APP
        avionsDansZone_.push_back(avion); // On l'ajoute dans la zone
        std::cout << "[APP] " << avion->getNom() << " entre dans la zone d'approche.\n";
    }
    Logger::getInstance().log("APP", "Prise en charge", "Avion " + avion->getNom());
}

void APP::assignerTrajectoireApproche(Avion* avion) {
    std::lock_guard<std::recursive_mutex> lock(mutexAPP_);
    if (!avion) throw std::invalid_argument("Avion NULL");
    if (!twr_) throw std::runtime_error("TWR NULL");

    Position pos = twr_->getPositionPiste(); // Récupération de la position de la piste
    double x = pos.getX(); 
    double y = pos.getY();

    // Définition des points de passage pour l'approche finale
    std::vector<Position> traj = {
        {x, y + 20000.0, 4000.0}, 
        {x, y + 10000.0, 2000.0}, 
        {x, y + 3000.0, 1000.0}, 
        {x, y + 1000.0, 500.0}
    };
    avion->setTrajectoire(traj); // Assignation de la trajectoire
    avion->setEtat(EtatAvion::EN_APPROCHE); // Mise à jour de l'état
    std::cout << "[APP] Trajectoire d'approche transmise a " << avion->getNom() << ".\n";
}

void APP::mettreEnAttente(Avion* avion) {
    std::lock_guard<std::recursive_mutex> lock(mutexAPP_);
    if (!avion) throw std::invalid_argument("Avion NULL");

    bool deja = (avion->getEtat() == EtatAvion::EN_ATTENTE_ATTERRISSAGE);
    avion->setEtat(EtatAvion::EN_ATTENTE_ATTERRISSAGE); // Changement d'état vers attente
    
    if (!deja) { // Si l'avion n'était pas déjà en attente
        fileAttenteAtterrissage_.push(avion); // Ajout à la file d'attente
        std::cout << "[APP] " << avion->getNom() << " entre en circuit d'attente.\n";
        Logger::getInstance().log("APP", "Mise en attente", "Avion " + avion->getNom());
    }

    // Création d'une trajectoire circulaire pour l'attente
    std::vector<Position> cercle;
    if (twr_) {
        Position centre = twr_->getPositionPiste();
        float rayon = avion->getDestination()->rayonControle;
        for (int i = 0; i < 5; ++i) {
            for (int angle = 0; angle < 360; angle += 10) {
                // Cast explicite pour éviter le warning
                float rad = static_cast<float>(angle * (3.14159265359 / 180.0));
                cercle.push_back({
                    centre.getX() + rayon * std::cos(rad), 
                    centre.getY() + rayon * std::sin(rad), 
                    2000.0
                });
            }
        }
        avion->setTrajectoire(cercle);
    }
}

bool APP::demanderAutorisationAtterrissage(Avion* avion) {
    std::lock_guard<std::recursive_mutex> lock(mutexAPP_);
    if (!avion || !twr_) return false;

    if (twr_->autoriserAtterrissage(avion)) { // Demande d'autorisation à la tour
        Position p = twr_->getPositionPiste();
        avion->setTrajectoire({{p.getX() + 1500.0, p.getY(), 0.0}}); // Trajectoire finale vers la piste

        // Retrait de la liste des avions en zone d'approche car il passe à la tour
        auto it = std::find(avionsDansZone_.begin(), avionsDansZone_.end(), avion);
        if (it != avionsDansZone_.end()) {
            avionsDansZone_.erase(it);
        }

        Logger::getInstance().log("APP", "Autorisation atterrissage", "Autorisation pour " + avion->getNom());
        return true;
    }
    return false;
}

void APP::mettreAJour() {
    std::lock_guard<std::recursive_mutex> lock(mutexAPP_);
    
    if (!twr_) return;

    twr_->setDemandeAtterrissage(!fileAttenteAtterrissage_.empty()); // Informe la tour si des avions attendent

    // Gestion prioritaire des urgences en attente
    for (Avion* avion : avionsDansZone_) {
        if (avion->estEnUrgence() && avion->getEtat() == EtatAvion::EN_ATTENTE_ATTERRISSAGE) {
            if (demanderAutorisationAtterrissage(avion)) {
                std::cout << "[APP] Urgence - Priorite d'atterrisage a " << avion->getNom() << ".\n";
                return;
            }
        }
    }

    // Gestion de la file d'attente standard
    if (!fileAttenteAtterrissage_.empty()) {
        Avion* avion = fileAttenteAtterrissage_.front();
        if (avion->getEtat() != EtatAvion::EN_ATTENTE_ATTERRISSAGE) { // Nettoyage si l'avion n'est plus en attente
            fileAttenteAtterrissage_.pop();
            return;
        }
        
        // Si la tour est disponible et pas d'urgence en cours, on tente l'atterrissage
        if (twr_ && !twr_->estUrgenceEnCours()) {
            if (demanderAutorisationAtterrissage(avion)) {
                fileAttenteAtterrissage_.pop();
                std::cout << "[APP] " << avion->getNom() << " atterrissage en cours.\n";
            }
        }
    }

    // Surveillance des nouvelles urgences dans la zone
    for (Avion* avion : avionsDansZone_) {
        if (avion->estEnUrgence() && twr_ && !twr_->estUrgenceEnCours()) {
            if (avion->getEtat() != EtatAvion::ATTERRISSAGE && avion->getEtat() != EtatAvion::EN_APPROCHE) {
                gererUrgence(avion);
            }
        }
    }
}

void APP::gererUrgence(Avion* avion) {
    if (!avion) {
        std::cerr << "Avion NULL\n";
        return;
    }
    if (!twr_) {
        std::cerr << "TWR NULL\n";
        return;
    }

    twr_->setUrgenceEnCours(true); // Déclenche le mode urgence de la tour
    std::cout << "[APP] Urgence pour " << avion->getNom() << ". Priorite absolue.\n";

    Position pos = twr_->getPositionPiste();
    avion->setTrajectoire({{pos.getX(), pos.getY(), 1000.0}, pos}); // Trajectoire directe vers la piste
    avion->setEtat(EtatAvion::EN_APPROCHE);
    std::cout << "[APP] Trajectoire directe d'urgence transmise.\n";
}
