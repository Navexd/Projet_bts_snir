#!/bin/bash

# Fonction pour installer les bibliothèques
install_libs() {
    echo "Mise à jour des packages..."
    sudo apt-get update
    
    echo "Installation de libserialport-dev..."
    sudo apt-get install -y libserialport-dev
    
    echo "Installation de libmysqlcppconn-dev..."
    sudo apt-get install -y libmysqlcppconn-dev
    
    echo "Installation terminée."
}

# Fonction pour désinstaller les bibliothèques
uninstall_libs() {
    echo "Désinstallation de libserialport-dev..."
    sudo apt-get remove --purge -y libserialport-dev
    
    echo "Désinstallation de libmysqlcppconn-dev..."
    sudo apt-get remove --purge -y libmysqlcppconn-dev
    
    echo "Nettoyage des packages non utilisés..."
    sudo apt-get autoremove -y
    
    echo "Désinstallation terminée."
}

# Vérification de l'argument passé au script
if [ "$1" == "installer" ]; then
    install_libs
elif [ "$1" == "désinstaller" ]; then
    uninstall_libs
else
    echo "Usage: $0 {installer|désinstaller}"
    exit 1
fi
