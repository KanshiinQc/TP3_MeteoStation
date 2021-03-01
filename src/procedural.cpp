// // Include(s)Libraire de base
// #include <FS.h> // Selon la doc doit être en premier sinon tout crash et brûle :D.
// #include <Arduino.h>

// // Include(s) et Variables Pour Bouton
// const int borneBouton = 18;

// // Include(s) Pour BME280
// #include <Adafruit_BME280.h>
// #include <Wire.h>
// #include <Adafruit_Sensor.h>

// // Include(s) et Variables pour LCD
// #define SEALEVELPRESSURE_HPA (1013.25)
// #include <LiquidCrystal_I2C.h>
// int lcdColumns = 16;
// int lcdRows = 2;

// // Include(s) et Variables Connexion Wifi
// #include <AccessPointCredentials.h>
// #include <WiFi.h>
// #include <Update.h>
// #include <WebServer.h>
// #include <DNSServer.h>
// #include <WifiManager.h>
// const char *ssid = MYSSID;
// const char *password = MYPSW;

// // Include(s) Et Variables File Message
// #include <PubSubClient.h>
// char mqttServer[15];
// char mqttPort[5];
// char mqttUser[50];
// char mqttPassword[100];

// // Include(s) et Variables Pour FS / JSON
// #include <ArduinoJson.h>
// #include <SPIFFS.h>
// bool shouldSaveConfig = false;

// void saveConfigCallback()
// {
//   Serial.println("Should save config");
//   shouldSaveConfig = true;
// }

// void LireFichierConfig()
// {
//   if (SPIFFS.exists("/config.json"))
//   {
//     //file exists, reading and loading
//     Serial.println("reading config file");

//     File configFile = SPIFFS.open("/config.json", "r");

//     if (configFile)
//     {
//       Serial.println("opened config file");

//       DynamicJsonDocument doc(1024);

//       auto error = deserializeJson(doc, configFile);

//       if (error)
//       {
//         Serial.print(F("deserializeJson() failed with code "));
//         Serial.println(error.c_str());
//         return;
//       }

//       else
//       {
//         serializeJsonPretty(doc["mqtt_server"], Serial);
//         serializeJsonPretty(doc["mqtt_port"], Serial);
//         serializeJsonPretty(doc["mqtt_user"], Serial);
//         serializeJsonPretty(doc["mqtt_password"], Serial);
//         Serial.println();

//         strcpy(mqttServer, doc["mqtt_server"]);
//         strcpy(mqttPort, doc["mqtt_port"]);
//         strcpy(mqttUser, doc["mqtt_user"]);
//         strcpy(mqttPassword, doc["mqtt_password"]);

//         Serial.println("Fichier Json Modifié");
//       }

//       configFile.close();
//     }
//   }
//   else
//   {
//     Serial.println("Fichier config.json Inexistant");
//   }
// }

// // OBJETS
// Adafruit_BME280 bme;
// LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
// WiFiClient espClient;
// PubSubClient client(espClient);
// WiFiManager wifiManager;

// WiFiManagerParameter custom_mqtt_server("mqttServer", "Serveur MQTT", mqttServer, 15);
// WiFiManagerParameter custom_mqtt_port("mqttPort", "Port MQTT", mqttPort, 4);
// WiFiManagerParameter custom_mqtt_username("mqttUser", "Username MQTT", mqttUser, 40);
// WiFiManagerParameter custom_mqtt_password("mqttPassword", "Password MQTT", mqttPassword, 100);

// void setupSpiffs()
// {
//   //clean FS, for testing
//   //SPIFFS.format();

//   //read configuration from FS json
//   Serial.println("mounting FS...");

//   if (SPIFFS.begin())
//   {
//     Serial.println("mounted file system");
//   }
//   else
//   {
//     Serial.println("Echec lors du montage du SYSTEME DE FICHIER");
//   }
// }

// void SauvegarderConfigurationReseau()
// {
//   if (shouldSaveConfig)
//   {
//     const char *mqttServeurWiFiManager = custom_mqtt_server.getValue();
//     const char *mqttPortWiFiManager = custom_mqtt_port.getValue();
//     const char *mqttUserWifiManager = custom_mqtt_username.getValue();
//     const char *mqttPassWifiManager = custom_mqtt_password.getValue();

//     Serial.println("saving config");

//     File configFile = SPIFFS.open("/config.json", FILE_WRITE);

//     if (!configFile)
//     {
//       Serial.println("failed to open config.json file for writing");
//     }

//     DynamicJsonDocument doc(1024);
//     deserializeJson(doc, configFile);

//     doc["mqtt_server"] = mqttServeurWiFiManager;
//     doc["mqtt_port"] = mqttPortWiFiManager;
//     doc["mqtt_user"] = mqttUserWifiManager;
//     doc["mqtt_password"] = mqttPassWifiManager;

//     serializeJson(doc, configFile);
//     configFile.close();
//     //end save
//     shouldSaveConfig = false;
//   }
// }

// void ConfigurerMQTT()
// {
//   // SET MQTT SERVER
//   int tentatives = 0;
//   client.setServer(mqttServer, atoi(mqttPort));
//   while (!client.connected() && tentatives < 3)
//   {
//     if (tentatives == 2)
//     {
//       wifiManager.resetSettings();
//       ESP.restart();
//     }

//     Serial.println("Connecting to MQTT...");

//     if (client.connect("ESP32Client", mqttUser, mqttPassword))
//     {
//       Serial.println("connected");
//     }
//     else
//     {
//       Serial.print("failed with state");
//       Serial.print(client.state());
//       tentatives++;
//       delay(2000);
//     }
//   }
// }
// void setup()
// {
//   pinMode(borneBouton, INPUT);

//   wifiManager.setSaveConfigCallback(saveConfigCallback);
//   Serial.begin(115200);
//   delay(10);

//   if (!bme.begin(0x76))
//   {
//     Serial.println("Could not find a valid BME280 sensor, check wiring!");
//     while (1)
//       ;
//   }

//   // initialize LCD
//   lcd.init();
//   // turn on LCD backlight
//   lcd.backlight();

//   setupSpiffs();

//   //add all your parameters here
//   wifiManager.addParameter(&custom_mqtt_server);
//   wifiManager.addParameter(&custom_mqtt_port);
//   wifiManager.addParameter(&custom_mqtt_username);
//   wifiManager.addParameter(&custom_mqtt_password);

//   // We start by connecting to a WiFi network
//   if (!wifiManager.autoConnect(ssid, password))
//   {
//     Serial.println("non connecte :");
//   }
//   else
//   {
//     Serial.print("connecte a:");
//     Serial.println(ssid);
//   }

//   SauvegarderConfigurationReseau();
//   LireFichierConfig();
//   ConfigurerMQTT();
//   delay(100);
// }

// void loop()
// {
//   bool boutonAppuye = digitalRead(borneBouton);
//   Serial.println(boutonAppuye);

//   if (boutonAppuye)
//   {
//     wifiManager.startConfigPortal(ssid, password);
//     SauvegarderConfigurationReseau();
//     LireFichierConfig();
//     ConfigurerMQTT();
//   }

//   // PUBLIE
//   String temperature = String(bme.readTemperature());
//   String humidite = String(bme.readHumidity());
//   String pression = String(bme.readPressure() / 100.0F);
//   String altitude = String(bme.readAltitude());

//   client.publish("stationMeteo/temperature", temperature.c_str());
//   client.publish("stationMeteo/humidite", humidite.c_str());
//   client.publish("stationMeteo/pression", pression.c_str());
//   client.publish("stationMeteo/altitude", altitude.c_str());
//   client.loop(); // Selon la doc, apres doit être lancé tous les moins de 15 secondes !

//   // PRINT LCD
//   lcd.setCursor(0, 0);
//   lcd.print("Temp= ");
//   lcd.print(bme.readTemperature());
//   lcd.print(" *C");

//   lcd.setCursor(0, 1);
//   lcd.print("Press= ");
//   lcd.print(bme.readPressure() / 100.0F);
//   lcd.print(" hPa");
//   delay(2000);

//   lcd.clear();

//   lcd.setCursor(0, 0);
//   lcd.print("Alt= ");
//   lcd.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
//   lcd.print(" m");

//   lcd.setCursor(0, 1);
//   lcd.print("Hum= ");
//   lcd.print(bme.readHumidity());
//   lcd.print(" %");
//   delay(2000);

//   lcd.clear();

//   // PRINT VALEURS SERIAL
//   Serial.println(bme.readTemperature());
//   Serial.println(bme.readPressure() / 100.0F);
//   Serial.println(bme.readHumidity());
//   Serial.println();
// }