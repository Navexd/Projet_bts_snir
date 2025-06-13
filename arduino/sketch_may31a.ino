#include <DHT.h>

#define DHTTYPE DHT11 // Définir le type de capteur DHT
#define DHTPIN 7 // Définir la broche de données à laquelle le capteur est connecté
#define SoundSensorPin A1  // Définir la broche analogique à laquelle le capteur de son est connecté
#define VREF  5.0  // Tension sur la broche AREF, par défaut : tension de fonctionnement

// Initialiser le capteur DHT
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // Initialiser la communication série
  Serial.begin(9600);
  
  // Initialiser le capteur DHT
  dht.begin();
}

void loop() {
  // Attendre un moment entre les lectures
  delay(1000);
  
  // Lire l'humidité
  float h = dht.readHumidity();
  float voltageValue, dbValue;

  // Lire la valeur analogique du capteur de son et la convertir en tension
  voltageValue = analogRead(SoundSensorPin) / 1024.0 * VREF;
  // Convertir la tension en valeur de décibel
  dbValue = voltageValue * 50.0;

  // Vérifier si le niveau sonore dépasse ou est égal à 110 dBA
  if (dbValue >= 110.0) {
    // Afficher les valeurs lues sur le moniteur série
    Serial.print(dbValue, 1);
    Serial.print(" dBA ");
    Serial.print("Humidité: ");
    Serial.print(h);
    Serial.println(" %\t");
  }
}