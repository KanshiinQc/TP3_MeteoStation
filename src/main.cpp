// Include(s)Libraire de base
#include <FS.h> // Selon la doc doit être en premier sinon tout crash et brûle :D.
#include <Arduino.h>

// Include(s) et Variables Pour Bouton
const int borneBouton = 18;

// Include(s) Pour BME280
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>

// Include(s) et Variables pour LCD
#define SEALEVELPRESSURE_HPA (1013.25)
#include <LiquidCrystal_I2C.h>
int lcdColumns = 16;
int lcdRows = 2;

// Include(s) et Variables Connexion Wifi
#include <AccessPointCredentials.h>
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WifiManager.h>
const char *ssid = MYSSID;
const char *password = MYPSW;

// Include(s) Et Variables File Message
#include <PubSubClient.h>
char mqttServer[15];
char mqttPort[5];
char mqttUser[50];
char mqttPassword[100];

// Include(s) et Variables Pour FS / JSON
#include <ArduinoJson.h>
#include <SPIFFS.h>
bool shouldSaveConfig = false;

class Bouton
{
private:
  int borne;
  bool estAppuye;

public:
  Bouton(const int &p_borne)
  {
    this->borne = p_borne;
    pinMode(this->borne, INPUT);
  }
  void LireBoutonEtSetEtat()
  {
    this->SetEstAppuye(digitalRead(this->borne));
  }
  bool GetEstAppuye()
  {
    return this->estAppuye;
  }
  void SetEstAppuye(bool boutonEstAppuye)
  {
    this->estAppuye = boutonEstAppuye;
  }
};
class ConfigurationReseauStation
{
  private:
    WiFiManager wifiManager;


};
class SystemeDeFichierStation
{};
class StationMeteo
{};
