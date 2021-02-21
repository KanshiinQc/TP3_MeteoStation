// Include(s)Libraire de base
#include <FS.h> // Selon la doc doit être en premier sinon tout crash et brûle :D.
#include <Arduino.h>

// Include(s) pour requêtes API
#include <HTTPClient.h>
// Include(s) FS / JSON
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Include(s) Pour BME280
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>

// Include(s) pour LCD
#define PRESSION_NIVEAU_MER (1013.25)
#include <LiquidCrystal_I2C.h>

// Include(s) Connexion Wifi
#include <AccessPointCredentials.h>
#include <WiFi.h>
#include <WifiManager.h>

// Include(s) File Message
#include <PubSubClient.h>
bool doitSauvegarderConfig;

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
class ConfigurationStation
{
private:
  WiFiManager wifiManager;
  WiFiManagerParameter custom_mqtt_server;
  WiFiManagerParameter custom_mqtt_port;
  WiFiManagerParameter custom_mqtt_username;
  WiFiManagerParameter custom_mqtt_password;
  WiFiClient clientWifi;
  PubSubClient clientMQTT;
  char mqttServer[15];
  char mqttPort[5];
  char mqttUser[50];
  char mqttPassword[100];
  const char ssid[50] = MYSSID;
  const char password[50] = MYPSW;

public:
  //static bool &doitSauvegarderConfig;
  ConfigurationStation()
      : wifiManager(), custom_mqtt_server("mqttServer", "Serveur MQTT", mqttServer, 15), custom_mqtt_port("mqttPort", "Port MQTT", mqttPort, 4),
        custom_mqtt_username("mqttUser", "Username MQTT", mqttUser, 40), custom_mqtt_password("mqttPassword", "Password MQTT", mqttPassword, 100),
        clientWifi(), clientMQTT(clientWifi)
  {
    AjouterParametresWifiManagerCustom();
  }

  static void SauvegarderConfigCallback()
  {
    Serial.println("La config doit être sauvegardée...");
    doitSauvegarderConfig = true;
  }

  void ParametrerAvantLancement()
  {
    wifiManager.setSaveConfigCallback(SauvegarderConfigCallback);
    MonterSystemeDeFichier();
    TenterConnexionAuWifi();
    SauvegarderConfigurationReseauDansFichier();
    AttribuerMqttAPartirFichierConfig();
    ConfigurerMQTT();
  }

  void TenterConnexionAuWifi()
  {
    if (!wifiManager.autoConnect(ssid, password))
    {
      Serial.println("non connecte :");
    }
    else
    {
      Serial.print("connecte a:");
      Serial.println(ssid);
    }
  }

  void AjouterParametresWifiManagerCustom()
  {
    Serial.println("Ajout Des Parametres");
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_username);
    wifiManager.addParameter(&custom_mqtt_password);
  }

  void MonterSystemeDeFichier()
  {
    //Si besoin de formater
    //SPIFFS.format();

    //read configuration from FS json
    Serial.println("Tentative de montage du système de fichier...");

    if (SPIFFS.begin())
    {
      Serial.println("Systeme de Fichier monté avec succès!");
    }
    else
    {
      Serial.println("Echec lors du montage du SYSTEME DE FICHIER");
    }
  }

  void ConfigurerReseauSurDemande()
  {
    wifiManager.startConfigPortal(ssid, password);
    SauvegarderConfigurationReseauDansFichier();
    AttribuerMqttAPartirFichierConfig();
    ConfigurerMQTT();
  }

  void SauvegarderConfigurationReseauDansFichier()
  {
    if (doitSauvegarderConfig)
    {
      File configFile = SPIFFS.open("/config.json", FILE_WRITE);

      if (!configFile)
      {
        Serial.println("Erreur lors de l'ouverture du fichier config.json");
      }

      DynamicJsonDocument doc(512);
      deserializeJson(doc, configFile);

      doc["mqtt_server"] = custom_mqtt_server.getValue();
      doc["mqtt_port"] = custom_mqtt_port.getValue();
      doc["mqtt_user"] = custom_mqtt_username.getValue();
      ;
      doc["mqtt_password"] = custom_mqtt_password.getValue();

      Serial.println("Sauvegarde de la configuration");
      serializeJson(doc, configFile);
      configFile.close();
      doc.clear();
      doitSauvegarderConfig = false;
    }
  }

  void AttribuerMqttAPartirFichierConfig()
  {
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("tentative de lecture du fichier");

      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile)
      {
        Serial.println("fichier config.json ouvert avec succès!");

        DynamicJsonDocument doc(512);

        auto error = deserializeJson(doc, configFile);

        if (error)
        {
          Serial.print(F("deserializeJson() failed with code "));
          Serial.println(error.c_str());
          return;
        }

        else
        {
          Serial.println("VALEURS DANS LE FICHIERS CONFIG:");
          serializeJsonPretty(doc["mqtt_server"], Serial);
          serializeJsonPretty(doc["mqtt_port"], Serial);
          serializeJsonPretty(doc["mqtt_user"], Serial);
          serializeJsonPretty(doc["mqtt_password"], Serial);
          Serial.println();

          strcpy(mqttServer, doc["mqtt_server"]);
          strcpy(mqttPort, doc["mqtt_port"]);
          strcpy(mqttUser, doc["mqtt_user"]);
          strcpy(mqttPassword, doc["mqtt_password"]);
          Serial.println("VALEURS DANS LES VARIABLES MQTT DE L'OBJET");
          Serial.println(mqttServer);
          Serial.println(mqttPort);
          Serial.println(mqttUser);
          Serial.println(mqttPassword);

          Serial.println("Fichier Json Modifié");
        }

        configFile.close();
        doc.clear();
      }
    }
    else
    {
      Serial.println("Fichier config.json Inexistant");
    }
  }

  void ConfigurerMQTT()
  {
    int tentatives = 0;

    clientMQTT.setServer(mqttServer, atoi(mqttPort));

    while (!clientMQTT.connected() && tentatives < 3)
    {
      if (tentatives == 2)
      {
        ConfigurerReseauSurDemande();
        SauvegarderConfigurationReseauDansFichier();
        AttribuerMqttAPartirFichierConfig();
      }

      Serial.println("Connecting to MQTT...");

      if (clientMQTT.connect("ESP32Client", mqttUser, mqttPassword))
      {
        Serial.println("connected");
      }
      else
      {
        Serial.print("failed with state");
        Serial.print(clientMQTT.state());
        tentatives++;
        delay(1000);
      }
    }
  }

  PubSubClient &GetClientMQTT()
  {
    return this->clientMQTT;
  }
};
class StationMeteo
{
private:
  Bouton &boutonStation;
  ConfigurationStation &configurationStation;
  Adafruit_BME280 &bme280Station;
  LiquidCrystal_I2C &ecranLCDStation;
  long tempsDerniereAlternanceLCD;
  bool togglePrintLCD = false;
  const int delaiAlternanceEtPublication = 5000;
  long tempsDernierePublicationMQTT;

public:
  StationMeteo(Bouton &p_boutonStation, ConfigurationStation &p_configurationStation, Adafruit_BME280 &p_bme280, LiquidCrystal_I2C &p_ecranLCD)
      : boutonStation(p_boutonStation), configurationStation(p_configurationStation), bme280Station(p_bme280), ecranLCDStation(p_ecranLCD){};

  void ParametrerAvantLancement()
  {
    this->configurationStation.ParametrerAvantLancement();

    if (!bme280Station.begin(0x76))
    {
      Serial.println("Aucun baromètre trouvé, vérifier le câblage");
      while (1)
        ;
    }

    ecranLCDStation.init();
    ecranLCDStation.backlight();
  }

  void PublierInfosMeteoMQTT()
  {
    if ((millis() - tempsDernierePublicationMQTT) >= delaiAlternanceEtPublication)
    {
      String temperature = String(bme280Station.readTemperature());
      String humidite = String(bme280Station.readHumidity());
      String pression = String(bme280Station.readPressure() / 100.0F);
      String altitude = String(bme280Station.readAltitude(PRESSION_NIVEAU_MER));

      this->configurationStation.GetClientMQTT().publish("stationMeteo/temperature", temperature.c_str());
      this->configurationStation.GetClientMQTT().publish("stationMeteo/humidite", humidite.c_str());
      this->configurationStation.GetClientMQTT().publish("stationMeteo/pression", pression.c_str());
      this->configurationStation.GetClientMQTT().publish("stationMeteo/altitude", altitude.c_str());
      // Selon la doc, la methode loop doit être lancé au 15 secondes maximum par défaut.
      this->configurationStation.GetClientMQTT().loop();
      tempsDernierePublicationMQTT = millis();
    }
  }

  void AfficherInfosLCD()
  {   
    if ((millis() - tempsDerniereAlternanceLCD) < delaiAlternanceEtPublication / 2)
    {
      if (!togglePrintLCD)
      {
        ecranLCDStation.clear();
        ecranLCDStation.setCursor(0, 0);
        ecranLCDStation.print("Temp= ");
        ecranLCDStation.print(bme280Station.readTemperature());
        ecranLCDStation.print(" *C");

        ecranLCDStation.setCursor(0, 1);
        ecranLCDStation.print("Press= ");
        ecranLCDStation.print(bme280Station.readPressure() / 100.0F);
        ecranLCDStation.print("hPa");
        togglePrintLCD = true;
      }
    }
    else if((millis() - tempsDerniereAlternanceLCD) >= delaiAlternanceEtPublication / 2)
    {
      if (togglePrintLCD)
      {
        ecranLCDStation.clear();
        ecranLCDStation.setCursor(0, 0);
        ecranLCDStation.print("Alt= ");
        ecranLCDStation.print(bme280Station.readAltitude(PRESSION_NIVEAU_MER));
        ecranLCDStation.print(" m");

        ecranLCDStation.setCursor(0, 1);
        ecranLCDStation.print("Hum= ");
        ecranLCDStation.print(bme280Station.readHumidity());
        ecranLCDStation.print(" %");
        togglePrintLCD = false;
      }
    }
    if(millis() - tempsDerniereAlternanceLCD > delaiAlternanceEtPublication)
    {
      tempsDerniereAlternanceLCD = millis();
    }
  }

  void Executer()
  {
    this->boutonStation.LireBoutonEtSetEtat();
    if (this->boutonStation.GetEstAppuye())
    {
      this->configurationStation.ConfigurerReseauSurDemande();
      this->boutonStation.SetEstAppuye(false);
    }

    PublierInfosMeteoMQTT();
    AfficherInfosLCD();
  }
};

Bouton boutonStation(18);
ConfigurationStation configurationStation;
Adafruit_BME280 bme280Station;
LiquidCrystal_I2C ecranLCDStation(0x27, 16, 2);
StationMeteo stationMeteo(boutonStation, configurationStation, bme280Station, ecranLCDStation);

void setup()
{
  Serial.begin(115200);
  delay(10);
  stationMeteo.ParametrerAvantLancement();
}

void loop()
{
  stationMeteo.Executer();
}
