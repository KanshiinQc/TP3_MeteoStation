// UN SEUL FICHIER CPP COMME DEMANDÉ DANS L'ÉNONCÉ

// Include(s) Librairies de base
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

// Include(s) / Define(s) pour LCD
#define PRESSION_NIVEAU_MER (1013.25)
#include <LiquidCrystal_I2C.h>

// Include(s) Connexion/Configuration Wifi
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
  void SetEstAppuye(bool p_boutonEstAppuye)
  {
    this->estAppuye = p_boutonEstAppuye;
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
  static bool doitSauvegarderConfig;

public:
  ConfigurationStation()
      : wifiManager(), custom_mqtt_server("mqttServer", "Serveur MQTT", mqttServer, 15), custom_mqtt_port("mqttPort", "Port MQTT", mqttPort, 5),
        custom_mqtt_username("mqttUser", "Utilisateur MQTT", mqttUser, 50), custom_mqtt_password("mqttPassword", "Mot de passe MQTT", mqttPassword, 100),
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
    this->wifiManager.setWiFiAutoReconnect(true);
    this->wifiManager.setSaveConfigCallback(SauvegarderConfigCallback);
    this->wifiManager.setConfigPortalTimeout(180);
    this->MonterSystemeDeFichier();
    this->TenterConnexionAutomatiqueWifi();
    this->SauvegarderConfigurationReseauDansFichier();
    this->AttribuerMqttAPartirFichierConfig();
    this->ConfigurerMQTT();
  }

  void AjouterParametresWifiManagerCustom()
  {
    Serial.println("Ajout Des Parametres");
    this->wifiManager.addParameter(&custom_mqtt_server);
    this->wifiManager.addParameter(&custom_mqtt_port);
    this->wifiManager.addParameter(&custom_mqtt_username);
    this->wifiManager.addParameter(&custom_mqtt_password);
  }

  void TenterConnexionAutomatiqueWifi()
  {
    if (!this->wifiManager.autoConnect(ssid, password))
    {
      Serial.println("non connecte :");
    }
    else
    {
      Serial.print("connecte a:");
      Serial.println(ssid);
    }
  }

  bool ReconnecterMQTTSiDeconnecter()
  {
    // si le wifi crash, wifiManager va reconnecter automatiquement l'esp lors du retour du wifi, cependant il ne reconnectera pas MQTT automatiquement
    // On va donc vérifier une fois par boucle de temps complète, si MQTT s'est déconnecté
    bool mqttConnecte = false;
    if (!this->clientMQTT.connected())
    {
      mqttConnecte = this->ConfigurerMQTT();
    }

    return mqttConnecte;
  }

  void ConfigurerReseauSurDemande()
  {
    this->wifiManager.startConfigPortal(ssid, password);
    // J'ai essayé des delai pour enlever le probleme de WiFiManager Not Ready sans succès...
    // delay(1000);
    this->SauvegarderConfigurationReseauDansFichier();
    this->AttribuerMqttAPartirFichierConfig();
    this->ConfigurerMQTT();
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
      Serial.println("Echec lors du montage du SYSTEME DE FICHIERS");
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

          strcpy(this->mqttServer, doc["mqtt_server"]);
          strcpy(this->mqttPort, doc["mqtt_port"]);
          strcpy(this->mqttUser, doc["mqtt_user"]);
          strcpy(this->mqttPassword, doc["mqtt_password"]);
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

  bool ConfigurerMQTT()
  {
    this->clientMQTT.setServer(mqttServer, atoi(mqttPort));

    Serial.println("Connexion en cours a MQTT");

    if (this->clientMQTT.connect("ESP32Client", mqttUser, mqttPassword))
    {
      Serial.println("connecté!");
    }
    else
    {
      Serial.print("echec avec code");
      Serial.print(clientMQTT.state());
    }
    return this->clientMQTT.connected();
  }

  PubSubClient &GetClientMQTT()
  {
    return this->clientMQTT;
  }
};
bool ConfigurationStation::doitSauvegarderConfig = false;

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
    memcpy(this->image1, p_image1, sizeof(image1));
    memcpy(this->image2, p_image2, sizeof(image2));
  }
};

class EcranLCDAnimeMeteo
{
private:
  LiquidCrystal_I2C &ecranLCD;
  AnimationLCD animationsLCD[4];
  long tempsDepartAnimationLCD;
  int imageActuelleAnimation;

public:
  EcranLCDAnimeMeteo(LiquidCrystal_I2C &p_ecranLCD, AnimationLCD (&p_animationsLCD)[4]) : ecranLCD(p_ecranLCD), animationsLCD(p_animationsLCD) {}

  void ParametrerAvantLancement()
  {
    this->ecranLCD.init();
    this->ecranLCD.backlight();
  }

  void AfficherAPMode()
  {
    this->ecranLCD.clear();
    this->ecranLCD.setCursor(0, 0);
    this->ecranLCD.print("Mode Point");
    this->ecranLCD.setCursor(0, 1);
    this->ecranLCD.print("Acces Active");
  }

  void AfficherMQTTDeconnecte()
  {
    this->ClearLCD();
    this->ecranLCD.setCursor(0, 0);
    this->ecranLCD.print("!!MQTT  ERREUR!!");
    this->ecranLCD.setCursor(0, 1);
    this->ecranLCD.print("TENTATIVE CONNEX");
  }

  void AfficherTemperaturePression(float p_temperature, float p_pression)
  {
    this->ecranLCD.clear();
    this->ecranLCD.setCursor(0, 0);
    this->ecranLCD.print("Temp= ");
    this->ecranLCD.print(p_temperature);
    this->ecranLCD.print(" *C");

    this->ecranLCD.setCursor(0, 1);
    this->ecranLCD.print("Press= ");
    this->ecranLCD.print(p_pression);
    this->ecranLCD.print("hPa");
  }

  void AfficherAltitudeHumidite(float p_altitude, float p_humidite)
  {
    this->ecranLCD.clear();
    this->ecranLCD.setCursor(0, 0);
    this->ecranLCD.print("Alt= ");
    this->ecranLCD.print(p_altitude);
    this->ecranLCD.print(" m");

    this->ecranLCD.setCursor(0, 1);
    this->ecranLCD.print("Hum= ");
    this->ecranLCD.print(p_humidite);
    this->ecranLCD.print(" %");
  }

  void EffacerAnimation()
  {
    this->ecranLCD.setCursor(0, 0);
    this->ecranLCD.print("");
    this->ecranLCD.setCursor(0, 1);
    this->ecranLCD.print("");
    this->ecranLCD.setCursor(1, 0);
    this->ecranLCD.print("");
    this->ecranLCD.setCursor(1, 1);
    this->ecranLCD.print("");
    this->ecranLCD.setCursor(2, 0);
    this->ecranLCD.print("");
    this->ecranLCD.setCursor(2, 1);
    this->ecranLCD.print("");
  }

  void AfficherAnimation(int index, int p_tempsAction)
  {
    if ((millis() - this->tempsDepartAnimationLCD) % (p_tempsAction / 3) <= p_tempsAction / 6)
    {
      if (this->imageActuelleAnimation == 0)
      {
        this->EffacerAnimation();

        for (int i = 0; i <= 5; i++)
        {
          this->ecranLCD.createChar(i, this->animationsLCD[index].image1[i]);
        }

        int indexWrite = 0;
        for (int i = 0; i <= 2; i++)
        {
          for (int j = 0; j <= 1; j++)
          {
            this->ecranLCD.setCursor(i, j);
            this->ecranLCD.write(indexWrite++);
          }
        }

        this->imageActuelleAnimation = 1;
      }
    }

    else if ((millis() - this->tempsDepartAnimationLCD) % (p_tempsAction / 3) > p_tempsAction / 6)
    {
      if (this->imageActuelleAnimation == 1)
      {
        this->EffacerAnimation();

        for (int i = 0; i <= 5; i++)
        {
          this->ecranLCD.createChar(i, this->animationsLCD[index].image2[i]);
        }

        int indexWrite = 0;
        for (int i = 0; i <= 2; i++)
        {
          for (int j = 0; j <= 1; j++)
          {
            this->ecranLCD.setCursor(i, j);
            this->ecranLCD.write(indexWrite++);
          }
        }

        this->imageActuelleAnimation = 0;
      }
    }
  }

  void AfficherAnimationSelonMeteo(String p_etatMeteo, int p_tempsAction)
  {
    if (millis() - this->tempsDepartAnimationLCD < 10)
    {
      this->ecranLCD.setCursor(4, 0);
      this->ecranLCD.print("PRESENTEMENT");
    }

    if (p_etatMeteo == "Snow" || p_etatMeteo == "Sleet" || p_etatMeteo == "Hail")
    {
      this->AfficherAnimation(3, p_tempsAction);
      this->ecranLCD.setCursor(4, 1);
      this->ecranLCD.print("IL NEIGE");
    }
    else if (p_etatMeteo == "Thunderstorm" || p_etatMeteo == "Heavy Rain" || p_etatMeteo == "Light Rain" || p_etatMeteo == "Showers")
    {
      this->AfficherAnimation(2, p_tempsAction);
      this->ecranLCD.setCursor(4, 1);
      this->ecranLCD.print("IL PLEUT");
    }
    else if (p_etatMeteo == "Heavy Cloud" || p_etatMeteo == "Light Rain")
    {
      this->AfficherAnimation(1, p_tempsAction);
      this->ecranLCD.setCursor(4, 1);
      this->ecranLCD.print("NUAGEUX");
    }
    else if (p_etatMeteo == "Clear")
    {
      this->AfficherAnimation(0, p_tempsAction);
      this->ecranLCD.setCursor(4, 1);
      this->ecranLCD.print("BEAU SOLEIL!");
    }
  }

  void SetTempsDepartAnimation(long p_temps)
  {
    this->tempsDepartAnimationLCD = p_temps;
  }

  void ClearLCD()
  {
    this->ecranLCD.clear();
  }
};

class RapporteurEtatMeteo
{
private:
  const char *serveurNTP = "0.ca.pool.ntp.org";
  const long ajustementSecondesESTvsGMT = -18000;
  struct tm informationsTemps;
  String valeurRequeteAPI;
  String UrlRequeteAPI;
  String etatDeLaMeteo;
  long dernierTempsFecthEtatMeteo;

  //static TaskHandle_t tacheDedieeFetchAPI;
  //const static BaseType_t deuxiemeCoeur = 1;

public:
  RapporteurEtatMeteo() {}

  void FaireRequeteAPIChaqueHeure()
  {
    if ((millis() - this->dernierTempsFecthEtatMeteo) > 3600000)
    {
      this->MettreEtatMeteoAJour();
      this->dernierTempsFecthEtatMeteo = millis();
    }
  }

  // CRÉDIT A PIERRE-FRANÇOIS POUR CE CODE ET L'IDÉE DU 2EME COEUR`
  /*
  void LierTaskHTTPAuDeuxiemeCoeur()
  {
    xTaskCreatePinnedToCore([](void *rm) -> void { ((RapporteurEtatMeteo *)rm)->FaireRequeteHttpGet(); }, "tacheDedieeFetchAPI", 30000, this, 1, &tacheDedieeFetchAPI, deuxiemeCoeur);
  }
  */

  void ParametrerAvantLancement()
  {
    configTime(ajustementSecondesESTvsGMT, 0, serveurNTP);
    //LierTaskHTTPAuDeuxiemeCoeur();
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

      this->UrlRequeteAPI = "https://www.metaweather.com/api/location/3534/" + annee + "/" + jour + "/" + mois + "/";

      // POUR TESTER D'AUTRES TEMPERATURES/ANIMATIONS --> LONDON / SANTA CRUZ
      //UrlRequeteAPI = "https://www.metaweather.com/api/location/44418/" + annee + "/" + jour + "/" + mois + "/";
      //UrlRequeteAPI = "https://www.metaweather.com/api/location/2488853/" + annee + "/" + jour + "/" + mois + "/";
    }
  }

  void FaireRequeteHttpGet()
  {
    Serial.print("La tâche roule sur le coeur numéro:");
    Serial.println(xPortGetCoreID());
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
      const char *etatMeteoChar = doc[0]["weather_state_name"];
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

  void Executer()
  {
    FaireRequeteAPIChaqueHeure();
  }

  String &GetEtatMeteo()
  {
    return this->etatDeLaMeteo;
  }
};
//TaskHandle_t RapporteurEtatMeteo::tacheDedieeFetchAPI;

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

  EcranLCDAnimeMeteo &ecranLCDAnime;

  RapporteurEtatMeteo rapporteurEtatMeteo;

  long tempsDepartLoop;
  int etapeActuelleLoopProgramme;
  const int tempsParAction = 4000;

public:
  StationMeteo(Bouton &p_boutonPortailWifi, ConfigurationStation &p_configurationStation, Adafruit_BME280 &p_bme280, EcranLCDAnimeMeteo &p_ecranLCDAnime)
      : boutonPortailWifi(p_boutonPortailWifi), configurationStation(p_configurationStation), bme280Station(p_bme280),
        ecranLCDAnime(p_ecranLCDAnime), rapporteurEtatMeteo(){};

  void SetTempsDepartLoopAMaintenant()
  {
    this->tempsDepartLoop = millis();
  }

  void ParametrerAvantLancement()
  {
    if (!this->bme280Station.begin(0x76))
    {
      Serial.println("Aucun baromètre trouvé, vérifier le câblage");
      while (1)
        ;
    }
    else
    {
      this->LireDonneesBarometre();
    }

    this->configurationStation.ParametrerAvantLancement();
    this->ecranLCDAnime.ParametrerAvantLancement();
    this->rapporteurEtatMeteo.ParametrerAvantLancement();
    if (!this->configurationStation.GetClientMQTT().connected())
    {
      this->ecranLCDAnime.AfficherMQTTDeconnecte();
      delay(2000);
    }
    this->rapporteurEtatMeteo.MettreEtatMeteoAJour();

    this->SetTempsDepartLoopAMaintenant();
  }

  void LireDonneesBarometre()
  {
    this->bmeValeurTemperature = this->bme280Station.readTemperature();
    this->bmeValeurHumidite = this->bme280Station.readHumidity();
    this->bmeValeurPression = this->bme280Station.readPressure() / 100;
    this->bmeValeurAltitude = this->bme280Station.readAltitude(PRESSION_NIVEAU_MER);
  }

  void PublierInfosBarometreMQTT()
  {
    String temperature = String(this->bmeValeurTemperature);
    String humidite = String(this->bmeValeurHumidite);
    String pression = String(this->bmeValeurPression);
    String altitude = String(this->bmeValeurAltitude);

    this->configurationStation.GetClientMQTT().publish("stationMeteo/temperature", temperature.c_str());
    this->configurationStation.GetClientMQTT().publish("stationMeteo/humidite", humidite.c_str());
    this->configurationStation.GetClientMQTT().publish("stationMeteo/pression", pression.c_str());
    this->configurationStation.GetClientMQTT().publish("stationMeteo/altitude", altitude.c_str());

    // Selon la doc, la methode loop doit être lancé au 15 secondes maximum par défaut.
    this->configurationStation.GetClientMQTT().loop();
  }

  void GererBoutonPortailWifi()
  {
    this->boutonPortailWifi.LireBoutonEtSetEtat();
    if (this->boutonPortailWifi.GetEstAppuye())
    {
      this->ecranLCDAnime.AfficherAPMode();
      this->configurationStation.ConfigurerReseauSurDemande();
      this->boutonPortailWifi.SetEstAppuye(false);
      this->ecranLCDAnime.ClearLCD();
    }
  }

  void LancerEtapeAfficherTemperaturePressionLCD()
  {
    if ((millis() - this->tempsDepartLoop) <= this->tempsParAction)
    {
      if (this->etapeActuelleLoopProgramme == 0)
      {
        Serial.println(millis() - tempsDepartLoop);
        this->ecranLCDAnime.AfficherTemperaturePression(this->bmeValeurTemperature, this->bmeValeurPression);
        this->etapeActuelleLoopProgramme++;
      }
    }
  }

  void LancerEtapeAfficherAltitudeHumiditeLCD()
  {
    if ((millis() - this->tempsDepartLoop) > this->tempsParAction)
    {
      if (this->etapeActuelleLoopProgramme == 1)
      {
        Serial.println(millis() - this->tempsDepartLoop);
        this->ecranLCDAnime.AfficherAltitudeHumidite(this->bmeValeurAltitude, this->bmeValeurHumidite);
        this->etapeActuelleLoopProgramme++;
      }
    }
  }

  void LancerEtapeAfficherAnimationMeteoLCD()
  {
    if ((millis() - this->tempsDepartLoop) > this->tempsParAction * 2)
    {
      if (this->etapeActuelleLoopProgramme == 2)
      {
        Serial.println(millis() - tempsDepartLoop);
        this->ecranLCDAnime.ClearLCD();
        this->ecranLCDAnime.SetTempsDepartAnimation(millis());
        this->etapeActuelleLoopProgramme++;
      }
      if (this->etapeActuelleLoopProgramme < 4)
      {
        this->ecranLCDAnime.AfficherAnimationSelonMeteo(this->rapporteurEtatMeteo.GetEtatMeteo(), this->tempsParAction);
      }
    }
  }

  void LancerEtapeLireBarometreEtPublierMQTT()
  {
    if ((millis() - this->tempsDepartLoop) > this->tempsParAction * 3)
    {
      if (this->etapeActuelleLoopProgramme == 3)
      {
        this->LireDonneesBarometre();

        if (!this->configurationStation.GetClientMQTT().connected())
        {
          this->ecranLCDAnime.AfficherMQTTDeconnecte();
          // Ceci gère les déconnexion au Wifi très courtes. Si c'est plus long, l'AP est lancé.
          this->configurationStation.ReconnecterMQTTSiDeconnecter();
          this->etapeActuelleLoopProgramme++;
        }
        else
        {
          Serial.println(millis() - tempsDepartLoop);
          this->PublierInfosBarometreMQTT();
          this->tempsDepartLoop = millis();
          this->etapeActuelleLoopProgramme = 0;
        }
      }
    }
  }

  void LancerEtapeRemiseAZeroLoop()
  {
    if ((millis() - this->tempsDepartLoop) > this->tempsParAction * 4)
    {
      Serial.println(millis() - tempsDepartLoop);
      this->tempsDepartLoop = millis();
      this->etapeActuelleLoopProgramme = 0;
    }
  }

  void Executer()
  {
    this->rapporteurEtatMeteo.Executer();

    this->GererBoutonPortailWifi();

    this->LancerEtapeAfficherTemperaturePressionLCD();
    this->LancerEtapeAfficherAltitudeHumiditeLCD();
    this->LancerEtapeAfficherAnimationMeteoLCD();
    this->LancerEtapeLireBarometreEtPublierMQTT();
    this->LancerEtapeRemiseAZeroLoop();
  }
};

// Instanciation des objets
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
EcranLCDAnimeMeteo ecranAnimeLCDMeteo(ecranLCDStation, animationsLCD);

StationMeteo stationMeteo(boutonPortailWifi, configurationStation, bme280Station, ecranAnimeLCDMeteo);

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
