#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <queue>
#include <cmath>
#include <fstream>

enum class EtatAvion {
    STATIONNE,// L'avion est stationné dans un parking
    ROULE_VERS_PISTE, // L'avion est parti du parking et roule vers la piste
    EN_ATTENTE_DECOLLAGE, // L'avion est au parking et attend d'être choisi pour décoller
    EN_ATTENTE_PISTE, // L'avion est arrivé à la piste, il attend qu'elle se libère pour décoller
    DECOLLAGE, // L'avion décolle
    EN_ROUTE, // L'avion est sur la route vers sa destination
    EN_APPROCHE, // L'avion entre dans la zone d'approche (APP) de sa destination
    EN_ATTENTE_ATTERRISSAGE, // L'avion est dans le circuit d'attente d'APP et il attend de pouvoir atterrir (que la piste se libère)
    ATTERRISSAGE, // L'avion atterrit
    ROULE_VERS_PARKING, // L'avion a atterri et roule vers son parking
    TERMINE // Disparition de l'avion
};

enum class TypeUrgence {
    AUCUNE,
    PANNE_MOTEUR, 
    MEDICAL,
    CARBURANT
};

enum class Tour { // Pour gérer qui atterrit et qui décolle, tour par tour
    DECOLLAGE,
    ATTERRISSAGE
};


class Position {
private:
    double x_, y_, altitude_;

public:
    Position(double x = 0, double y = 0, double z = 0);

    double getX() const;
    double getY() const;
    double getAltitude() const;
    void setPosition(double x, double y, double alt);

    double distance(const Position& other) const;

    Position operator+(const Position& other) const;
    Position operator-(const Position& other) const;
    Position operator*(double scalar) const;
    bool operator==(const Position& other) const;
};

class Parking {
private:
    std::string nom_;
    Position position_;
    bool occupe_;

public:
    Parking(std::string nom, Position pos);
    bool estOccupe() const; // Renvoie si le parking est occupe
    void occuper(); // Met a jour le parking en occupe
    void liberer(); // Met a jour le parking pour le liberer
    Position getPosition() const; // Renvoie la position du parking
    std::string getNom() const; // Renvoie le nom du parking
    double getDistancePiste(Position posPiste) const; // Renvoie la distance du parking a la piste (pour la priorite au decollage)

    bool operator==(const Parking& other) const;
};

struct Aeroport;

class Avion {
private:
    std::string nom_;
    float vitesse_;
    float vitesseSol_;
    float carburant_;
    float conso_;
    float dureeStationnement_;
    Position pos_;
    EtatAvion etat_;
    Parking* parking_;
    Aeroport* destination_;
    TypeUrgence typeUrgence_;
    std::vector<Position> trajectoire_;
    mutable std::mutex mtx_;

public:
    Avion(std::string n, float v, float vSol, float c, float conso, float dureeStat, Position pos);

    std::string getNom() const; // Renvoie le nom de l'avion
    float getVitesse() const; // Renvoie la vitesse de croisière
    float getVitesseSol() const; // Renvoie la vitesse au sol
    float getCarburant() const; // Renvoie la quantité de carburant
    float getConsommation() const; // Renvoie la consommation
    Position getPosition() const; // Renvoie la position actuelle
    EtatAvion getEtat() const; // Renvoie l'état actuel
    Parking* getParking() const; // Renvoie le parking assigné
    Aeroport* getDestination() const; // Renvoie l'aéroport de destination
    float getDureeStationnement() const; // Renvoie la durée de stationnement prévue
    bool estEnUrgence() const; // Renvoie si l'avion est en urgence
    TypeUrgence getTypeUrgence() const; // Renvoie le type d'urgence
    const std::vector<Position> getTrajectoire() const; // Renvoie la trajectoire à suivre

    void setPosition(const Position& p); // Définit la position
    void setTrajectoire(const std::vector<Position>& traj); // Définit la trajectoire
    void setEtat(EtatAvion e); // Définit l'état
    void setParking(Parking* p); // Assigne un parking
    void setDestination(Aeroport* dest); // Définit la destination

    void avancer(float dt); // Fait avancer l'avion en vol
    void avancerSol(float dt); // Fait avancer l'avion au sol
    void declarerUrgence(TypeUrgence type); // Déclare une urgence
    void effectuerMaintenance(); // Effectue la maintenance au sol

    bool operator==(const Avion& other) const;
};

class TWR {
private:
    bool pisteLibre_;
    std::vector<Parking>& parkings_;
    Position posPiste_;
    float tempsAtterrissageDecollage_;
    std::vector<Avion*> filePourDecollage_;
    mutable std::mutex mutexTWR_;
    bool urgenceEnCours_;
    Tour tourActuel_;
    bool demandeAtterrissage_;

public:
    TWR(std::vector<Parking>& parkings, Position posPiste, float tempsAtterrisageDecollage);

    Position getPositionPiste() const; // Renvoie la position de la piste
    bool estPisteLibre() const; // Renvoie si la piste est libre
    void libererPiste(); // Libère la piste
    void reserverPiste(); // Réserve la piste

    void setDemandeAtterrissage(bool statut); // Signale une demande d'atterrissage
    bool autoriserAtterrissage(Avion* avion); // Autorise l'atterrissage si possible

    Parking* choisirParkingLibre(); // Trouve un parking libre
    void attribuerParking(Avion* avion, Parking* parking); // Assigne un parking à un avion
    void gererRoulageVersParking(Avion* avion, Parking* parking); // Calcule le trajet vers le parking

    void enregistrerPourDecollage(Avion* avion); // Ajoute un avion à la file de décollage
    Avion* choisirAvionPourDecollage(); // Sélectionne le prochain avion à décoller
    bool autoriserDecollage(Avion* avion); // Autorise le décollage
    void retirerAvionDeDecollage(Avion* avion); // Retire l'avion de la file après décollage

    void setUrgenceEnCours(bool statut); // Définit l'état d'urgence de la tour
    bool estUrgenceEnCours() const; // Renvoie si une urgence est en cours
};

class APP {
private:
    std::vector<Avion*> avionsDansZone_;
    std::queue<Avion*> fileAttenteAtterrissage_;
    TWR* twr_;
    mutable std::recursive_mutex mutexAPP_;

public:
    APP(TWR* tour);
    void ajouterAvion(Avion* avion); // Prend en charge un nouvel avion dans la zone
    void assignerTrajectoireApproche(Avion* avion); // Définit la trajectoire d'approche
    void mettreEnAttente(Avion* avion); // Place l'avion en circuit d'attente
    bool demanderAutorisationAtterrissage(Avion* avion); // Demande à la TWR l'autorisation d'atterrir
    void mettreAJour(); // Met à jour l'état des avions en approche
    size_t getNombreAvionsDansZone() const; // Renvoie le nombre d'avions gérés
    size_t getNombreAvionsEnAttente() const; // Renvoie le nombre d'avions en attente
    void gererUrgence(Avion* avion); // Gère un avion en urgence dans la zone
};

class CCR {
private:
    std::vector<Avion*> avionsEnCroisiere_;
    std::mutex mutexCCR_;

public:
    CCR();
    void prendreEnCharge(Avion* avion); // Prend en charge un avion en croisière
    void transfererVersApproche(Avion* avion, APP* appCible); // Transfère l'avion au contrôleur d'approche
    void gererEspaceAerien(); // Gère les collisions et les transferts
    bool validerPlanDeVol(Aeroport* depart, Aeroport* arrivee); // Vérifie si le plan de vol est valide
};

struct Aeroport {
    std::string nom;
    Position position;
    float rayonControle;
    std::vector<Parking> parkings;
    TWR* twr;
    APP* app;

    Aeroport(std::string n, Position pos, float rayon); // Constructeur de l'aéroport
};

class Logger {
private:
    std::ofstream fichier_;
    std::mutex mutex_;
    bool premierElement_;
    Logger();
    ~Logger();

public:
    static Logger& getInstance(); // Renvoie l'instance unique du logger
    void log(const std::string& acteur, const std::string& action, const std::string& details); // Enregistre une action dans le fichier log

    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;
};

std::ostream& operator<<(std::ostream& os, const Position& pos);
std::ostream& operator<<(std::ostream& os, const Avion& avion);