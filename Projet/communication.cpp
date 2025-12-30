#include "avion.hpp"
#include <filesystem>
#include <stdexcept>

// Surcharge de l'opérateur << pour afficher une pos de manière lisible
std::ostream& operator<<(std::ostream& os, const Position& pos) {
    os << "(" << (int)pos.getX() << ", " << (int)pos.getY() << ", Alt:" << (int)pos.getAltitude() << ")";
    return os;
}

// Surcharge de l'opérateur << pour afficher les infos essentielles d'un Avion
std::ostream& operator<<(std::ostream& os, const Avion& avion) {
    os << "[" << avion.getNom() << "  Carburant : " << (int)avion.getCarburant() << "]";
    return os;
}

Logger::Logger() : premierElement_(true) {
    // Calcul du chemin absolu vers le fichier de logs (dans le dossier img du projet)
    std::filesystem::path cheminFichierSource = __FILE__;
    std::filesystem::path dossierProjet = cheminFichierSource.parent_path();
    std::filesystem::path cheminLog = dossierProjet / "img" / "logs.json";

    // Ouverture du fichier et initialisation du tableau JSON
    fichier_.open(cheminLog);
    if (fichier_.is_open()) {
        fichier_ << "[\n";
    }
    else {
        throw std::runtime_error("Impossible de creer ou d'ouvrir le fichier de log : " + cheminLog.string());
    }
}

Logger::~Logger() {
    // Fermeture propre du tableau JSON et du fichier à la destruction
    if (fichier_.is_open()) {
        fichier_ << "\n]";
        fichier_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::log(const std::string& acteur, const std::string& action, const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_); 

    if (fichier_.is_open()) {
        // virgule entre les éléments (sauf pour le premier)
        if (!premierElement_) fichier_ << ",\n";

        // Écriture structurée de l'event
        fichier_ << "  {\n";
        fichier_ << "    \"Controleur\": \"" << acteur << "\",\n";
        fichier_ << "    \"Action\": \"" << action << "\",\n";
        fichier_ << "    \"Details\": \"" << details << "\"\n";
        fichier_ << "  }";
        premierElement_ = false;
    }
}