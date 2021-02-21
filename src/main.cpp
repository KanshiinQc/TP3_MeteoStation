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

    while (!clientMQTT.connected() && tentatives < 4)
    {
      if (tentatives == 3)
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

class DonneesDessinLCD
{
  private:
    
}

class StationMeteo
{
private:
  Bouton &boutonStation;
  ConfigurationStation &configurationStation;
  Adafruit_BME280 &bme280Station;
  LiquidCrystal_I2C &ecranLCDStation;
  long tempsDerniereAlternanceLCD;
  int etapeAlternanceLCD;
  const int delaiDeBaseEntreActions = 2500;
  long tempsDernierePublicationMQTT;
  struct tm informationsTemps;
  String valeurRequeteAPI;
  String etatDeLaMeteo;
  String UrlRequeteAPI;
  int tempsDernierFetchApiMeteo;
  const char *serveurNTP = "0.ca.pool.ntp.org";
  const long ajustementSecondesESTvsGMT = -18000;

public:
  StationMeteo(Bouton &p_boutonStation, ConfigurationStation &p_configurationStation, Adafruit_BME280 &p_bme280, LiquidCrystal_I2C &p_ecranLCD)
      : boutonStation(p_boutonStation), configurationStation(p_configurationStation), bme280Station(p_bme280), ecranLCDStation(p_ecranLCD), informationsTemps(){};

  void ParametrerAvantLancement()
  {
    this->configurationStation.ParametrerAvantLancement();

    if (!bme280Station.begin(0x76))
    {
      Serial.println("Aucun baromètre trouvé, vérifier le câblage");
      while (1)
        ;
    }

    configTime(ajustementSecondesESTvsGMT, 0, serveurNTP);

    ecranLCDStation.init();
    ecranLCDStation.backlight();
  }

  void SetUrlAPI()
  {
    if (!getLocalTime(&informationsTemps))
    {
      Serial.println("Failed to obtain time");
      return;
    }
    else
    {
      int anInt = informationsTemps.tm_year + 1900;
      int jourInt = informationsTemps.tm_mday;
      int moisInt = informationsTemps.tm_mon + 1;

      String annee = String(anInt);
      String mois = String(jourInt);
      String jour = String(moisInt);

      UrlRequeteAPI = "https://www.metaweather.com/api/location/3534/" + annee + "/" + jour + "/" + mois + "/";
    }
  }

  void FaireRequeteHttpGet()
  {
    HTTPClient clientHTTP;

    clientHTTP.begin(UrlRequeteAPI);

    int codeReponseHTTP = clientHTTP.GET();

    String resultatRequete = "{}";

    if (codeReponseHTTP > 0)
    {
      Serial.print("Code de reponse HTTP: ");
      Serial.println(codeReponseHTTP);
      valeurRequeteAPI = clientHTTP.getString();
    }
    else
    {
      Serial.print("Code d'erreur: ");
      Serial.println(codeReponseHTTP);
    }
    clientHTTP.end();
  }

  void SetEtatDeLaMeteo()
  {
    Serial.println(UrlRequeteAPI);

    DynamicJsonDocument doc(30000);

    auto error = deserializeJson(doc, valeurRequeteAPI);

    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
    else
    {
      const char *etatMeteoChar = doc[0]["weather_state_abbr"];
      etatDeLaMeteo = String(etatMeteoChar);
    }

    doc.clear();
  }

  void AfficherSurMatrice8x8()
  {

    Serial.println("Etat de la météo selon les données les plus près de chez vous que nous avons:");
    if (etatDeLaMeteo == "sn" || etatDeLaMeteo == "sl" || etatDeLaMeteo == "h")
    {
      Serial.println("Il neige/grêle");
    }
    else if (etatDeLaMeteo == "t" || etatDeLaMeteo == "hr" || etatDeLaMeteo == "lr" || etatDeLaMeteo == "s")
    {
      Serial.println("Il pleut et il y a possiblement des orages");
    }
    else if (etatDeLaMeteo == "hc" || etatDeLaMeteo == "lc")
    {
      Serial.println("C'est un peu ou très nuageux");
    }
    else if (etatDeLaMeteo == "c")
    {
      Serial.println("C'est très ensoleillé");
    }
  }

  void MettreEtatMeteoAJour()
  {
    if ((millis() - tempsDernierFetchApiMeteo) >= delaiDeBaseEntreActions * 2)
    {
      SetUrlAPI();
      FaireRequeteHttpGet();
      SetEtatDeLaMeteo();
      AfficherSurMatrice8x8();
      tempsDernierFetchApiMeteo = millis();
    }
  }

  void PublierInfosMeteoMQTT()
  {
    if ((millis() - tempsDernierePublicationMQTT) >= delaiDeBaseEntreActions * 2)
    {

      if (!getLocalTime(&informationsTemps))
      {
        Serial.println("Failed to obtain time");
        return;
      }

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

  void AfficherAPModeLCD()
  {
    ecranLCDStation.clear();
    ecranLCDStation.setCursor(0, 0);
    ecranLCDStation.print("Mode Point");
    ecranLCDStation.setCursor(0, 1);
    ecranLCDStation.print("Acces Active");
  }
  void AfficherInfosLCD()
  {
    if ((millis() - tempsDerniereAlternanceLCD) < delaiDeBaseEntreActions)
    {
      if (etapeAlternanceLCD == 0)
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
        etapeAlternanceLCD++;
      }
    }
    else if ((millis() - tempsDerniereAlternanceLCD) >= delaiDeBaseEntreActions && (millis() - tempsDerniereAlternanceLCD) < delaiDeBaseEntreActions * 2)
    {
      if (etapeAlternanceLCD == 1)
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
        etapeAlternanceLCD++;
      }
    }
    else if (millis() - tempsDerniereAlternanceLCD > delaiDeBaseEntreActions * 2)
    {
      if (etapeAlternanceLCD == 2)
      {
        byte sun1[] = {
            B00000,
            B00000,
            B00000,
            B00000,
            B00010,
            B00000,
            B00000,
            B00000};

        byte sun2[] = {
            B01110,
            B00000,
            B00000,
            B00000,
            B00010,
            B00000,
            B00000,
            B00000};

        byte sun3[] = {
            B00000,
            B00000,
            B00100,
            B00100,
            B00100,
            B00000,
            B01110,
            B11111};

        byte sun4[] = {
            B11111,
            B11111,
            B01110,
            B00000,
            B00100,
            B00100,
            B00100,
            B00000};

        byte sun5[] = {
            B00000,
            B00000,
            B00000,
            B00000,
            B01000,
            B00000,
            B00000,
            B00000};

        byte sun6[] = {
            B01110,
            B00000,
            B00000,
            B00000,
            B01000,
            B00000,
            B00000,
            B00000};

        ecranLCDStation.clear();

        ecranLCDStation.createChar(1, sun1);
        ecranLCDStation.createChar(2, sun2);
        ecranLCDStation.createChar(3, sun3);
        ecranLCDStation.createChar(4, sun4);
        ecranLCDStation.createChar(5, sun5);
        ecranLCDStation.createChar(6, sun6);

        ecranLCDStation.setCursor(0, 0);
        ecranLCDStation.write(1);
        ecranLCDStation.setCursor(0, 1);
        ecranLCDStation.write(2);
        ecranLCDStation.setCursor(1, 0);
        ecranLCDStation.write(3);
        ecranLCDStation.setCursor(1, 1);
        ecranLCDStation.write(4);
        ecranLCDStation.setCursor(2, 0);
        ecranLCDStation.write(5);
        ecranLCDStation.setCursor(2, 1);
        ecranLCDStation.write(6);

        etapeAlternanceLCD++;
      }
    }

    if (millis() - tempsDerniereAlternanceLCD > delaiDeBaseEntreActions * 3)
    {
      etapeAlternanceLCD = 0;
      tempsDerniereAlternanceLCD = millis();
    }
  }

  void Executer()
  {
    this->boutonStation.LireBoutonEtSetEtat();
    if (this->boutonStation.GetEstAppuye())
    {
      AfficherAPModeLCD();
      this->configurationStation.ConfigurerReseauSurDemande();
      this->boutonStation.SetEstAppuye(false);
      ecranLCDStation.clear();
    }

    MettreEtatMeteoAJour();
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
