// Include(s)Libraire de base
#include <FS.h> // Selon la doc doit être en premier sinon tout crash et brûle :D.
#include <Arduino.h>

// Include(s) pour requêtes API
#include <HTTPClient.h>

// Include(s) FS / JSON
#include <ArduinoJson.h>
#include <SPIFFS.h>
// En le mettant dans la classe config, tout plantait a cause de la func... Je n'ai pas trouvé quoi faire.
bool doitSauvegarderConfig;

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
      : wifiManager(), custom_mqtt_server("mqttServer", "Serveur MQTT", mqttServer, 15), custom_mqtt_port("mqttPort", "Port MQTT", mqttPort, 5),
        custom_mqtt_username("mqttUser", "Username MQTT", mqttUser, 50), custom_mqtt_password("mqttPassword", "Password MQTT", mqttPassword, 100),
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
    wifiManager.setConfigPortalTimeout(20);
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

  void ConfigurerReseauSurDemande()
  {
    wifiManager.startConfigPortal(ssid, password);
    // J'ai essayé des delai pour enlever le probleme de WiFiManager Not Ready sans succès...
    // delay(1000);
    SauvegarderConfigurationReseauDansFichier();
    AttribuerMqttAPartirFichierConfig();
    ConfigurerMQTT();
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
    int nombretentatives = 1;

    clientMQTT.setServer(mqttServer, atoi(mqttPort));

    while (!clientMQTT.connected() && nombretentatives < 5)
    {
      if (nombretentatives == 4)
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
        nombretentatives++;
        delay(1000);
      }
    }
  }

  PubSubClient &GetClientMQTT()
  {
    return this->clientMQTT;
  }
};

class LibrairieImagesLCD
{
public:
  byte soleil1[6][8] = {
      {B00000,
       B00000,
       B00000,
       B00000,
       B00010,
       B00000,
       B00000,
       B00000},
      {B01110,
       B00000,
       B00000,
       B00000,
       B00010,
       B00000,
       B00000,
       B00000},
      {B00000,
       B00000,
       B00100,
       B00100,
       B00100,
       B00000,
       B01110,
       B11111},
      {B11111,
       B11111,
       B01110,
       B00000,
       B00100,
       B00100,
       B00100,
       B00000},
      {B00000,
       B00000,
       B00000,
       B00000,
       B01000,
       B00000,
       B00000,
       B00000},
      {B01110,
       B00000,
       B00000,
       B00000,
       B01000,
       B00000,
       B00000,
       B00000}};

  byte soleil2[6][8] = {
      {B00000,
       B00000,
       B00000,
       B00100,
       B00010,
       B00000,
       B00000,
       B00000},
      {B11110,
       B00000,
       B00000,
       B00000,
       B00010,
       B00100,
       B00000,
       B00000},
      {B00000,
       B00100,
       B00100,
       B00100,
       B00100,
       B00000,
       B01110,
       B11111},
      {B11111,
       B11111,
       B01110,
       B00000,
       B00100,
       B00100,
       B00100,
       B00100},
      {B00000,
       B00000,
       B00000,
       B00100,
       B01000,
       B00000,
       B00000,
       B00000},
      {B01111,
       B00000,
       B00000,
       B01000,
       B00100,
       B00000,
       B00000,
       B00000}};

  byte nuage1[6][8] = {
      {B00000,
       B00001,
       B00011,
       B00111,
       B01111,
       B11111,
       B01111,
       B00000},
      {B00000,
       B00000,
       B00000,
       B00000,
       B00000,
       B00001,
       B00000,
       B00000},
      {B00000,
       B11110,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B00000},
      {B00000,
       B00011,
       B00111,
       B01111,
       B11111,
       B11111,
       B11111,
       B00000},
      {B00000,
       B00000,
       B00000,
       B10000,
       B10000,
       B10000,
       B00000,
       B00000},
      {B00000,
       B11100,
       B11110,
       B11111,
       B11111,
       B11111,
       B11110,
       B00000}};

  byte nuage2[6][8] = {
      {B00000,
       B00000,
       B00001,
       B00011,
       B00111,
       B01111,
       B00111,
       B00000},
      {B00000,
       B00000,
       B00000,
       B00000,
       B00001,
       B00011,
       B00001,
       B00000},
      {B00000,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B00000},
      {B00000,
       B00111,
       B01111,
       B11111,
       B11111,
       B11111,
       B11111,
       B00000},
      {B00000,
       B00000,
       B10000,
       B11000,
       B11000,
       B11000,
       B10000,
       B00000},
      {B00000,
       B11000,
       B11100,
       B11110,
       B11110,
       B11110,
       B11100,
       B00000}};

  byte pluie1[6][8] = {
      {B00000,
       B00000,
       B00000,
       B00001,
       B00011,
       B00111,
       B01111,
       B01111},
      {B00111,
       B00000,
       B01000,
       B01000,
       B00010,
       B00010,
       B00000,
       B00000},
      {B00000,
       B01111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111},
      {B11111,
       B00000,
       B10001,
       B10001,
       B00100,
       B00100,
       B00000,
       B00000},
      {B00000,
       B10000,
       B11000,
       B11100,
       B11110,
       B11110,
       B11110,
       B11110},
      {B11100,
       B00000,
       B00010,
       B00010,
       B01000,
       B01000,
       B00000,
       B00000}};

  byte pluie2[6][8] = {
      {B00000,
       B00000,
       B00000,
       B00001,
       B00011,
       B00111,
       B01111,
       B01111},
      {B00111,
       B00000,
       B00010,
       B00010,
       B01000,
       B01000,
       B00000,
       B00000},
      {B00000,
       B01111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111},
      {B11111,
       B00000,
       B00100,
       B00100,
       B10001,
       B10001,
       B00000,
       B00000},
      {B00000,
       B10000,
       B11000,
       B11100,
       B11110,
       B11110,
       B11110,
       B11110},
      {B11100,
       B00000,
       B01000,
       B01000,
       B00010,
       B00010,
       B00000,
       B00000}};

  byte neige1[6][8] = {
      {B00000,
       B00000,
       B00000,
       B00001,
       B00011,
       B00111,
       B01111,
       B01111},
      {B00111,
       B00000,
       B00100,
       B01110,
       B00100,
       B00000,
       B00001,
       B00000},
      {B00000,
       B01111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111},
      {B11111,
       B00000,
       B00001,
       B00011,
       B00001,
       B10000,
       B11000,
       B10000},
      {B00000,
       B10000,
       B11000,
       B11100,
       B11110,
       B11110,
       B11110,
       B11110},
      {B11100,
       B00000,
       B00000,
       B10000,
       B00000,
       B00100,
       B01110,
       B00100}};

  byte neige2[6][8] = {
      {B00000,
       B00000,
       B00000,
       B00001,
       B00011,
       B00111,
       B01111,
       B01111},
      {B00111,
       B00000,
       B00000,
       B00001,
       B00000,
       B00100,
       B01110,
       B00100},
      {B00000,
       B01111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111,
       B11111},
      {B11111,
       B00000,
       B10000,
       B11000,
       B10000,
       B00001,
       B00011,
       B00001},
      {B00000,
       B10000,
       B11000,
       B11100,
       B11110,
       B11110,
       B11110,
       B11110},
      {B11100,
       B00000,
       B00100,
       B01110,
       B00100,
       B00000,
       B10000,
       B00000}};
};

class AnimationLCD
{
public:
  byte image1[6][8];
  byte image2[6][8];

  AnimationLCD(byte (&p_image1)[6][8], byte (&p_image2)[6][8])
  {
    memcpy(image1, p_image1, sizeof(image1));
    memcpy(image2, p_image2, sizeof(image2));
  }
};

class StationMeteo
{
private:
  Bouton &boutonPortailWifi;

  ConfigurationStation &configurationStation;

  Adafruit_BME280 &bme280Station;
  float bmeValeurTemperature;
  float bmeValeurHumidite;
  float bmeValeurPression;
  float bmeValeurAltitude;

  LiquidCrystal_I2C &ecranLCDStation;
  AnimationLCD animationsMeteoLCD[4];
  int tempsDepartAnimationLCD;
  int imageActuelleAnimation;

  long tempsDepartLoop;
  int etapeActuelleLoopProgramme;
  const int tempsParAction = 4000;

  const char *serveurNTP = "0.ca.pool.ntp.org";
  const long ajustementSecondesESTvsGMT = -18000;
  struct tm informationsTemps;
  String valeurRequeteAPI;
  String UrlRequeteAPI;
  int dernierTempsFecthAPI;
  String etatDeLaMeteo;

  //static TaskHandle_t tacheDedieeFetchAPI;
  //const static BaseType_t deuxiemeCoeur = 1;

public:
  StationMeteo(Bouton &p_boutonPortailWifi, ConfigurationStation &p_configurationStation, Adafruit_BME280 &p_bme280, LiquidCrystal_I2C &p_ecranLCD, AnimationLCD (&p_animationsLCD)[4])
      : boutonPortailWifi(p_boutonPortailWifi), configurationStation(p_configurationStation), bme280Station(p_bme280),
        ecranLCDStation(p_ecranLCD), animationsMeteoLCD(p_animationsLCD), informationsTemps(){};

  /*
  void LierTaskHTTPAuDeuxiemeCoeur()
  {
    xTaskCreatePinnedToCore(this->FaireRequeteHttpGet(), "tacheDedieeFetchAPI", 30000, this, 1, &tacheDedieeFetchAPI, deuxiemeCoeur);
  }
  */
  void SetTempsDepartLoopAMaintenant()
  {
    this->tempsDepartLoop = millis();
  }

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
    MettreEtatMeteoAJour();
    SetTempsDepartLoopAMaintenant();
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

  void MettreEtatMeteoAJour()
  {
    SetUrlAPI();
    FaireRequeteHttpGet();
    SetEtatDeLaMeteo();
  }

  void LireDonneesBarometre()
  {
    bmeValeurTemperature = bme280Station.readTemperature();
    bmeValeurHumidite = bme280Station.readHumidity();
    bmeValeurPression = bme280Station.readPressure() / 100;
    bmeValeurAltitude = bme280Station.readAltitude(PRESSION_NIVEAU_MER);
  }

  void PublierInfosBarometreMQTT()
  {
    if (!getLocalTime(&informationsTemps))
    {
      Serial.println("Failed to obtain time");
      return;
    }

    String temperature = String(bmeValeurTemperature);
    String humidite = String(bmeValeurHumidite);
    String pression = String(bmeValeurPression);
    String altitude = String(bmeValeurAltitude);

    this->configurationStation.GetClientMQTT().publish("stationMeteo/temperature", temperature.c_str());
    this->configurationStation.GetClientMQTT().publish("stationMeteo/humidite", humidite.c_str());
    this->configurationStation.GetClientMQTT().publish("stationMeteo/pression", pression.c_str());
    this->configurationStation.GetClientMQTT().publish("stationMeteo/altitude", altitude.c_str());

    // Selon la doc, la methode loop doit être lancé au 15 secondes maximum par défaut.
    this->configurationStation.GetClientMQTT().loop();
  }

  void AfficherAPModeLCD()
  {
    ecranLCDStation.clear();
    ecranLCDStation.setCursor(0, 0);
    ecranLCDStation.print("Mode Point");
    ecranLCDStation.setCursor(0, 1);
    ecranLCDStation.print("Acces Active");
  }

  void AfficherTemperaturePressionLCD()
  {
    ecranLCDStation.clear();
    ecranLCDStation.setCursor(0, 0);
    ecranLCDStation.print("Temp= ");
    ecranLCDStation.print(bmeValeurTemperature);
    ecranLCDStation.print(" *C");

    ecranLCDStation.setCursor(0, 1);
    ecranLCDStation.print("Press= ");
    ecranLCDStation.print(bmeValeurPression);
    ecranLCDStation.print("hPa");
  }

  void AfficherAltitudeHumiditeLCD()
  {
    ecranLCDStation.clear();
    ecranLCDStation.setCursor(0, 0);
    ecranLCDStation.print("Alt= ");
    ecranLCDStation.print(bmeValeurAltitude);
    ecranLCDStation.print(" m");

    ecranLCDStation.setCursor(0, 1);
    ecranLCDStation.print("Hum= ");
    ecranLCDStation.print(bmeValeurHumidite);
    ecranLCDStation.print(" %");
  }

  void AfficherAnimationLCD(int index)
  {
    if ((millis() - tempsDepartAnimationLCD) % (tempsParAction / 3) <= tempsParAction / 6)
    {
      if (imageActuelleAnimation == 0)
      {
        ecranLCDStation.clear();

        ecranLCDStation.createChar(1, this->animationsMeteoLCD[index].image1[0]);
        ecranLCDStation.createChar(2, this->animationsMeteoLCD[index].image1[1]);
        ecranLCDStation.createChar(3, this->animationsMeteoLCD[index].image1[2]);
        ecranLCDStation.createChar(4, this->animationsMeteoLCD[index].image1[3]);
        ecranLCDStation.createChar(5, this->animationsMeteoLCD[index].image1[4]);
        ecranLCDStation.createChar(6, this->animationsMeteoLCD[index].image1[5]);

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
        imageActuelleAnimation = 1;
      }
    }

    else if ((millis() - tempsDepartAnimationLCD) % (tempsParAction / 3) > tempsParAction / 6)
    {
      if (imageActuelleAnimation == 1)
      {

        ecranLCDStation.clear();

        ecranLCDStation.createChar(1, this->animationsMeteoLCD[index].image2[0]);
        ecranLCDStation.createChar(2, this->animationsMeteoLCD[index].image2[1]);
        ecranLCDStation.createChar(3, this->animationsMeteoLCD[index].image2[2]);
        ecranLCDStation.createChar(4, this->animationsMeteoLCD[index].image2[3]);
        ecranLCDStation.createChar(5, this->animationsMeteoLCD[index].image2[4]);
        ecranLCDStation.createChar(6, this->animationsMeteoLCD[index].image2[5]);

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

        imageActuelleAnimation = 0;
      }
    }
  }

  void AfficherAnimationLCDSelonMeteo()
  {
    if (etatDeLaMeteo == "sn" || etatDeLaMeteo == "sl" || etatDeLaMeteo == "h")
    {
      AfficherAnimationLCD(1);
    }
    else if (etatDeLaMeteo == "t" || etatDeLaMeteo == "hr" || etatDeLaMeteo == "lr" || etatDeLaMeteo == "s")
    {
      AfficherAnimationLCD(1);
    }
    else if (etatDeLaMeteo == "hc" || etatDeLaMeteo == "lc")
    {
      AfficherAnimationLCD(1);
    }
    else if (etatDeLaMeteo == "c")
    {
      AfficherAnimationLCD(1);
    }
  }

  void GererBoutonPortailWifi()
  {
    this->boutonPortailWifi.LireBoutonEtSetEtat();
    if (this->boutonPortailWifi.GetEstAppuye())
    {
      AfficherAPModeLCD();
      this->configurationStation.ConfigurerReseauSurDemande();
      this->boutonPortailWifi.SetEstAppuye(false);
      ecranLCDStation.clear();
    }
  }

  void LancerAction1()
  {
    if ((millis() - tempsDepartLoop) <= tempsParAction)
    {
      if (etapeActuelleLoopProgramme == 0)
      {
        Serial.println(millis() - tempsDepartLoop);
        LireDonneesBarometre();
        AfficherTemperaturePressionLCD();
        PublierInfosBarometreMQTT();
        etapeActuelleLoopProgramme++;
      }
    }
  }

  void LancerAction2()
  {
    if ((millis() - tempsDepartLoop) > tempsParAction)
    {
      if (etapeActuelleLoopProgramme == 1)
      {
        Serial.println(millis() - tempsDepartLoop);
        AfficherAltitudeHumiditeLCD();
        etapeActuelleLoopProgramme++;
      }
    }
  }

  void LancerAction3()
  {
    if ((millis() - tempsDepartLoop) > tempsParAction * 2)
    {
      if (etapeActuelleLoopProgramme == 2)
      {
        Serial.println(millis() - tempsDepartLoop);
        tempsDepartAnimationLCD = millis();
        etapeActuelleLoopProgramme++;
      }
      AfficherAnimationLCDSelonMeteo();
    }
  }

  void LancerAction4()
  {
    if ((millis() - tempsDepartLoop) > tempsParAction * 3)
    {
      Serial.println(millis() - tempsDepartLoop);
      tempsDepartLoop = millis();
      etapeActuelleLoopProgramme = 0;
    }
  }

  void Executer()
  {
    if ((millis() - dernierTempsFecthAPI) > 3600000)
    {
      MettreEtatMeteoAJour();
      dernierTempsFecthAPI = millis();
    }

    GererBoutonPortailWifi();
    LancerAction1();
    LancerAction2();
    LancerAction3();
    LancerAction4();
  }
};

class EcranLCDAnime
{
private:
  LiquidCrystal_I2C &ecranLCD;
  AnimationLCD animationsLCD[4];
  
public:
  EcranLCDAnime(LiquidCrystal_I2C &p_ecranLCD, AnimationLCD (&p_animationsLCD)[4]) : ecranLCD(p_ecranLCD), animationsLCD(p_animationsLCD){}

};

Bouton boutonPortailWifi(18);
ConfigurationStation configurationStation;
Adafruit_BME280 bme280Station;

LibrairieImagesLCD librairiesImagesLCD;
AnimationLCD animationSoleil(librairiesImagesLCD.soleil1, librairiesImagesLCD.soleil2);
AnimationLCD animationNuages(librairiesImagesLCD.nuage1, librairiesImagesLCD.nuage2);
AnimationLCD animationPluie(librairiesImagesLCD.pluie1, librairiesImagesLCD.pluie2);
AnimationLCD animationNeige(librairiesImagesLCD.neige1, librairiesImagesLCD.neige2);
AnimationLCD animationsLCD[4]{animationSoleil, animationNuages, animationPluie, animationNeige};
LiquidCrystal_I2C ecranLCDStation(0x27, 16, 2);

StationMeteo stationMeteo(boutonPortailWifi, configurationStation, bme280Station, ecranLCDStation, animationsLCD);

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
