/* Routeur solaire développé par le Profes'Solaire v9.15 - 28-11-2023 - professolaire@gmail.com
Merci à Jean-Victor pour l'idée d'optimisation de la gestion des Dimmers
- 2 sorties 16A / 3000 watts
- 1 relais on/off
- 1 serveur web Dash Lite avec On / Off
- support MQTT Mosquito - Home Assistant
- heure NTP
- relay marche forcée : 16A mini
- marche forcée automatique suivant surplus et par rapport au volume ballon
- marche forcée automatique avec sonde de température : 50 degrés min
- mise à jour OTA en wifi
 * ESP32 + JSY-MK-194 (16 et 17) + Dimmer1 24A-600V (35 ZC et 25 PW) + Dimmer 2 24A-600V ( 35 ZC et 26 PW) + écran Oled (22 : SCK et 21 SDA) + relay (13) + relay marche forcée (32) + sonde température DS18B20 (4)
 Utilisation des 2 Cores de l'Esp32
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

      // Librairies //

#include <Arduino.h>
//#include <HardwareSerial.h> // https://github.com/espressif/arduino-esp32
#include <U8g2lib.h> // gestion affichage écran Oled  https://github.com/olikraus/U8g2_Arduino/ //
#include <Wire.h> // pour esp-Dash

#include <WiFi.h> // gestion du wifi
#include <AsyncTCP.h>   //  https://github.com/me-no-dev/AsyncTCP  ///
#include <ESPAsyncWebServer.h>  // https://github.com/me-no-dev/ESPAsyncWebServer  et https://github.com/bblanchon/ArduinoJson
#include <ESPmDNS.h>
  // #include <esp_now.h>
  //#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

//#include <ESPDash.h> // page web Dash  https://github.com/ayushsharma82/ESP-DASH //
#include <NTPClient.h> // gestion de l'heure https://github.com/arduino-libraries/NTPClient //

#include <OneWire.h> // pour capteur de température DS18B20  https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // pour capteur de température DS18B20 https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <ArduinoOTA.h> // mise à jour OTA par wifi

#include <TimeLib.h>
#include <ArduinoJson.h>

//#include <Triac.h>
//#include <RBDdimmer.h>
#include <dimmable_light.h>
#include <routeurWeb.h>

#include <espnow.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////// CONFIGURATION ///// PARTIE A MODIFIER POUR VOTRE INSTALLATION //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const uint8_t TZ_OFFSET = 1;                // faisseau horaire france : gmt+1

    const char *APssid = "SFR_2E48";                    // The name of the Wi-Fi network that will be created
    const char *APpassword = "rsionardishoodbe2rmo";    // The password required to connect to it, leave blank for an open network

//    const char* APssid     = "LudosHomeN"; 
//    const char* APpassword = "4FA23839005492FAC22A193CC4";

    const char* solarEdgeApiKey = "8INR9G7TVYP03QAMRMNKJYRNN0MTVJSQ";   // get api key from solaredge portal

    boolean relayChauffeEau = false;            // si relay pour chauffeEau suppl
    boolean relayPAC = true;                    // si relay pour la PAC
    int puissancePAC = 1000;                    // Puissance de la PAC
    boolean espnowPAC = false;                  // si PAC controlée par ESPNOW
    int relayPACOn = 1000;                      // puissance du surplus pour déclencher le relay //
    int relayPACOff = 800;                      // puissance du surplus pour stopper le relay //
//    time_t tempsOverP = 15 * 60;                // temps en sec d'overproduction avant de passer en PAC on si assez de puissance (relayOn)
//    time_t tempsMinPAC = 3600 * 2;              // si PAC en Marche au moins 2h de marche même si pas assez de puissance
    time_t tempsOverP = 1 * 60;                // DEBUG temps en sec d'overproduction avant de passer en PAC on si assez de puissance (relayOn)
    time_t tempsMinPAC = 60 * 2;               // DEBUG si PAC en Marche au moins 2h de marche même si pas assez de puissance

    boolean oled = true;                        // on / off de l'ecran
    boolean motionSensor = true;                // si module de detection

    boolean sondeTemp = false;                  // si sonde DS18B20
    boolean marcheForceeTemperature = false;    // marche forcée automatique avec sonde de température DS18B20 (50 degrés) : 0 ou 1
    int temperatureEauMin = 50;                 // réglage de la température minimale de l'eau en marche forcée

    boolean marcheForceeVol = true;             // marche forcée automatique suivant le volume du ballon : 0 ou 1
    int volumeBallon = 100;                     // volume du ballon en litres

    int heureOnMarcheForcee = 00;               // heure début marche forcée
    int minuteOnMarcheForcee = 30;              // minute début marche forcée
    bool marcheForceeSec = false;               // si marche forcee secondaire dans la journee
    int heureOnMarcheForceeSec = 12;            // heure début marche forcée sec
    int minuteOnMarcheForceeSec = 30;           // minute début marche forcée sec

    int resistance = 1000;										  // puissance de la resistance du chauffe-eau

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MaxNTPRetries 15

      // pins attribution Broches utilisées//
#define pinTriac 25           // broche de commande du triac
#define pinZeroCross 26       // broche utilisée pour le zéro crossing

#define RelayPAC 13           // relay on/off déclenchement LOW si puissance PV > resistance ballon
#define RelayCauffeEauPin 32  // relay 16A mini marche forcée déclenchement LOW
#define pinDS18B20 4          // broche du capteur DS18B20 //
#define pinMotionSensor 27    // Set GPIOs for the PIR Motion Sensor

// for oled display
#define SDA 21
#define SCL 22

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


   /* -------------   Variables  -------------*/

        // user variables
    const bool debug = true;
    bool initPhase = true;

        // global variables
    enum actionEcranWeb {none, full, horloge, wifi, values, mForcee, mfValues, espnow};

    AsyncWebServer server = AsyncWebServer(80); 
//    AsyncWebServer serverCard = AsyncWebServer(81); 
    AsyncEventSource routeurEvents = AsyncEventSource("/routeurEvents");
    AsyncEventSource routeurParamsEvents = AsyncEventSource("/paramEvents");


        // local variables
    hw_timer_t *timerNTP = NULL;
    hw_timer_t *timerWIFI = NULL;
    hw_timer_t *timerSolarEdge = NULL;

    const uint8_t solarEdgeGetInfos = 15;           // nb secondes between each solaredge requests
//    const unsigned long timeoutEcran = 10 * 60;	    // 10 mn en secondes
    const time_t timeoutEcran = 5 * 60;	      // 5mn en secondes

    uint8_t ntpWait = 1;                        // nb minutes between each NTP packets (MaxNTPRetries) calls
    TaskHandle_t Task1;
    TaskHandle_t Task2;

    bool ntpOK = false;
    boolean marcheForcee = false;               // mise en route en automatique //
    bool marcheForceeDone = false;
    bool debugMarcheForcee = true;
    uint8_t nbMinutesdebugMF = 0;
    struct {
      actionEcranWeb ecran = none;
      actionEcranWeb web = none;
    } typeUpdate;
    struct {
      bool marcheForceeDone = false;
      uint8_t heureOnMarcheForcee;
      uint8_t minuteOnMarcheForcee;
      time_t tempsChauffe;
    } plageMarcheForcee[2];

    wl_status_t WiFiStatus = WL_DISCONNECTED;
    int16_t maxtriesNTP = MaxNTPRetries;
    int previousHour = -1;
    int previousMinute = -1;
    int previousSecond = -1;
    int lastday = -1;
    uint8_t nbSecondes = 0;                     // pour un timer multiple de secondes comme solarEdgeGetInfos
    uint8_t nbSecondesPIR = 0;
    uint8_t nbMinutes = 0; 
    uint8_t nbMinutesEcran = 0; 
    float temperatureEau = -127;  				      // La valeur vaut -127 quand la sonde DS18B20 n'est pas présente
//    unsigned long tempsChauffe = 4185 * volumeBallon * ( temperatureEauMin - 15 ) / resistance;   // Temps de chauffe en secondes = 4 185 x volume du ballon x (température idéale - température actuelle) / puissance du chauffe-eau
    time_t tempsChauffe = 10;                   // Temps de chauffe en secondes = 4 185 x volume du ballon x (température idéale - température actuelle) / puissance du chauffe-eau
    time_t finMarcheForcee;                     // temps de fin de marche forcee ballon (now + temps chauffe)
    time_t debutOverPuissance = 0;
    time_t debutRelayPAC = 0;

    JsonDocument solarEdgeInfo;                   // for v7
//    DynamicJsonDocument solarEdgeInfo(2048);    // for v6
    float GRIDCurrentPower = 0.0;
    float LOADCurrentPower = 0.0;
    float PVCurrentPower = 0.0;
    float currentPower = 0.0;                 // si > 0 on produit trop, si < 0 on ne produit pas assez
    float pasPuissance = resistance / 100;    // puissance resistance est entre 0 et 100 fois pasPuissance
    float valTriac = 0.0;
    float restePuissance = 0.0;

    volatile int candidatePIR = LOW;                                  // the un-filtered new status candidate of the PIR sensor
    volatile int statusPIR = HIGH;                                   	// the noise/spike-filtered status of the PIR sensor
    volatile boolean changePIR = false;                            	  // the indication that a potential status change has happened
    time_t lastReadTime = 0;                     				              // the time stamp of the potential status change (edge timer start)
    volatile unsigned long lastPIR_Time = millis();  
    const long unsigned int delayNoise = 500;                       	// milliseconds. Status changes shorter than this are deemed noise/spike. 100ms works for me.
    bool motion = true;
 

    //NTPClient timeClient(ntpUDP, "fr.pool.ntp.org", 3600, 60000);
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 24*3600*1000);
    const uint8_t journee[][4] = {{8,40, 17,20}, {8,20, 18,00}, {7,40, 18,30}, {7,50, 20,10}, {7,00, 20,40}, {6,30, 21,15}, {6,30, 21,30}, {7,00, 21,00}, {7,30, 20,20}, {8,00, 19,20}, {7,40, 17,30}, {8,20, 17,10}};            
    time_t jour = 0;   
    time_t nuit = 0;   

    // WebRouteur manager
    RouteurSolWebClass routeurWeb;                           // gestion sur serveur web              

    // oled display SSD1306 124*64 I2C
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA);
    // Web Card interface
//    ESPDash dashboard(&serverCard);

    // Solar Edge
    WiFiClientSecure *clientSolarEdge;     // WiFiClient for clientSolarEdge https;
    String solarEdgeCurrentPowerFlowUrl = (String)"https://monitoringapi.solaredge.com/site/4129753/currentPowerFlow?api_key=" + solarEdgeApiKey + "&format=application/json";
    String solarEdgePayload = "{}";       // get response from solaredge (should be a json)
    bool gotSolarEdgePayload = false;

/*    // ESP Dash cards
    Card marcheForceeBtn(&dashboard, BUTTON_CARD, "INFO : Auto/Marche forcée");
    Card Oled(&dashboard, BUTTON_CARD, "Écran On/Off");
    Card chauffeBallonBtn(&dashboard, BUTTON_CARD, "Chauffe Balon forcée");
    Card chauffePAC(&dashboard, BUTTON_CARD, "Chauffe PAC forcée");

    Card horlogeRH(&dashboard, SLIDER_CARD, "Heure de départ: ", "h", 0, 23);
    Card horlogeRM(&dashboard, SLIDER_CARD, "Minute de départ: ", "mn", 0, 59);
    Card marcheForceeVolTemp(&dashboard, SLIDER_CARD, "Type Marche forcée Vol/Temp", "",0,1);    //Temperature ou Volume
    Card tempEauMin(&dashboard, SLIDER_CARD, "Température Eau Min: ", "°C", 0, 99);
    Card tempEau(&dashboard, GENERIC_CARD, "Température Eau Ballon: ", "°C");
    Card marcheForceeSecBtn(&dashboard, BUTTON_CARD, "Marche Foréee Secondaire");
    Card horlogeRHSec(&dashboard, SLIDER_CARD, "Heure de départ secondaire: ", "h", 0, 23);
    Card horlogeRMSec(&dashboard, SLIDER_CARD, "Minute de départsecondaire: ", "mn", 0, 59);

    Card volBallon(&dashboard, GENERIC_CARD, "Volume Ballon: ", "l");
    Card resistanceBallon(&dashboard, GENERIC_CARD, "Resistance Ballon: ", "W");

    Card nbHeureChauffe(&dashboard, GENERIC_CARD, "Temps de Chauffe Ballon: ", "h");
    Card nbMinuteChauffe(&dashboard, GENERIC_CARD, "Temps de Chauffe Ballon: ", "mn");

    Card puissEDF(&dashboard, GENERIC_CARD, "Puissance EDF: ", "kW");
    Card puissMaison(&dashboard, GENERIC_CARD, "Puissance Maison: ", "kW");
    Card puissPanneaux(&dashboard, GENERIC_CARD, "Puissance Panneaux: ", "kW");
    Card puissBallon(&dashboard, GENERIC_CARD, "Puissance au Ballon: ", "W");
    Card pourcentBallon(&dashboard, GENERIC_CARD, "Pourcentage Puissance Ballon: ", "%");
*/


// DS18B20 Class usage
    OneWire oneWire(pinDS18B20); 			// instance de communication avec le capteur de température
    DallasTemperature ds18b20(&oneWire); 	// correspondance entreoneWire et le capteur Dallas de température

// inner classes
//    Triac triac1(pinTriac, pinZeroCross);
    DimmableLight triac1(pinTriac);
//    dimmerLamp triac1(pinTriac, pinZeroCross); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards
    EspNowClass routeurEspnow;


// function definitions here:

    void Task1code( void * pvParameters );
    void Task2code( void * pvParameters );

    bool startWiFi();
    bool getSolarEdgeValues();
    bool WiFiConnect(const char *ssid, const char *passphrase);

    void initOTA();
    int  dstOffset(time_t newTime);
    bool getNTPTime();
    void marcheForceePACSwitch(boolean value);
    void marcheForceeSwitch(boolean value);
    void setRelayPac(uint8_t state);
    void gestEcran(actionEcranWeb action);
    void gestWeb();
    void calculDureeJour(int theMonth);

    void stopEcran();

/*_________________________________________Timers & interuptions functions__________________________________________________________*/

    void IRAM_ATTR onTimerNTP() {  //Interruption every hour
      if(WiFi.status() == WL_CONNECTED){
        ntpOK = getNTPTime();
      }
    }

    void IRAM_ATTR onTimerWIFI() {  //Interruption every minute
      if(WiFi.status() != WL_CONNECTED){
        if(startWiFi()) {
          server.begin();
//          serverCard.begin();
        }
      }
    }

    void IRAM_ATTR onTimerSolarEdge() {  //Interruption every minute
      if(WiFi.status() == WL_CONNECTED){
        clientSolarEdge = new WiFiClientSecure;
        if (clientSolarEdge) {
//          clientSolarEdge -> setCACert(solarEdgeCertificate);
          clientSolarEdge->setInsecure(); // set secure client without certificate
          {  // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
            HTTPClient httpSolarEdge;
            Serial.print("[HTTP] begin...\n");
            if(httpSolarEdge.begin(*clientSolarEdge,solarEdgeCurrentPowerFlowUrl)){
              httpSolarEdge.addHeader("Content-Type", "application/x-www-form-urlencoded");
              httpSolarEdge.addHeader("Content-Type", "application/json");
              int httpResponseCode = httpSolarEdge.GET();
              Serial.printf("[HTTPS] get... code: %d\n", httpResponseCode);
              if (httpResponseCode > 0) { // httpCode will be negative on error
                if (httpResponseCode == HTTP_CODE_OK) {
                  solarEdgePayload = httpSolarEdge.getString();
                  gotSolarEdgePayload = true;
//                  Serial.printf("[HTTPS] get... payload: %s\n", solarEdgePayload.c_str());
                }
              } else {
                if(gotSolarEdgePayload) gotSolarEdgePayload = false;
                Serial.printf("[HTTPS] GET... failed, error: %s\n", httpSolarEdge.errorToString(httpResponseCode).c_str());
              }
            } else {
              if(gotSolarEdgePayload) gotSolarEdgePayload = false;
            }
            httpSolarEdge.end();
          }
          delete clientSolarEdge;
        } else {
          Serial.println("Unable to create client");
        }
      } else {
        if(gotSolarEdgePayload) gotSolarEdgePayload = false;
        Serial.println("Not Connected to wifi");
      }
    }

    void IRAM_ATTR detectsMovementRising() {                      // sensor edge detection interrupt service routine
        changePIR = true;                                        	// status change detected
        candidatePIR = HIGH;  //digitalRead(motionSensor);       // we have a new status candidate (noise/spike might still cause this change)
        lastPIR_Time = millis();	    	                          // mark the time of the change
    }

    void IRAM_ATTR detectsMovementFalling() {                     // sensor edge detection interrupt service routine
        changePIR = true;                                        	// status change detected
        candidatePIR = LOW; // digitalRead(motionSensor);        // we have a new status candidate (noise/spike might still cause this change)
        lastPIR_Time = millis();	    	                          // mark the time of the change
    }


/*_________________________________________Wifi and Web functions__________________________________________________________*/


    bool startWiFi() {
        bool rtn = false;

      if(WiFi.status() != WL_CONNECTED){
        Serial.println(F(" Try to connect the Wifi..."));
        WiFi.mode(WIFI_STA);
        Serial.println(F(" First try with last used wifi ssid ..."));
        if(!WiFiConnect(nullptr,nullptr)){
          Serial.println(F("WiFi cnx with last connection FAILED, Next try with ssids in config ..."));
          if(!WiFiConnect(APssid,APpassword)){
            Serial.println(F("WiFi with saved connections FAILED"));
          }
        }

        if(WiFi.status() == WL_CONNECTED) {
          Serial.println();
          Serial.println(F("WiFi connected"));
          Serial.print(F("IP address: "));
          Serial.print(WiFi.localIP());
          Serial.print(F("\t\t\t MAC address: "));
          Serial.print(WiFi.macAddress());
          Serial.print(F("\t\t\t Wifi Channel: "));
          Serial.println(WiFi.channel());
          rtn = true;
        } else {
          Serial.println(F("\nCan't connect to WIFI ..."));
          rtn = false;
        }
      } else {
        Serial.println(F("\nAlready connected to WIFI "));
        rtn = true;
      }
      return rtn;
    }

    bool WiFiConnect(const char *ssid, const char *passphrase){
       unsigned long mytimeout = millis() / 1000;
       WiFi.persistent( false );
       (ssid == nullptr) ? WiFi.begin() : WiFi.begin(ssid, passphrase); //WiFi connection

        while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED){
            Serial.print(".");
            delay(500);
            if((millis() / 1000) > mytimeout + 10){ // try for less than 10 seconds to connect to WiFi router
              Serial.println();
              break;
            }
        }

        if (WiFi.status() == WL_CONNECTED){
          Serial.println();
          return true;
        }
        return false;
    }

    bool getSolarEdgeValues(){
          bool rtn = false;
          const char* from;
          const char* to;

      if(gotSolarEdgePayload){
        Serial.println("Decoding solaredge payload ...");
        Serial.println(solarEdgePayload);
                // Parse response
/*
              {
                "siteCurrentPowerFlow": {
                  "unit": "W",
                  "connections": [
                    {
                    "from": "GRID",
                    "to": "Load"
                    }
                  ],
                  "GRID": {
                    "status": "Active",
                    "currentPower": 3435.77978515625
                  },
                  "LOAD": {
                    "status": "Active",
                    "currentPower": 3435.77978515625
                  },
                  "PV": {
                    "status": "Idle",
                    "currentPower": 0
                  }
                }
              }
*/

        DeserializationError error = deserializeJson(solarEdgeInfo,solarEdgePayload);   // or deserializeJson(solarEdgeInfo, http.getStream());
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          rtn = false;
        } else {    // Read values
          JsonObject siteCurrentPowerFlow = solarEdgeInfo["siteCurrentPowerFlow"];
          for (JsonObject siteCurrentPowerFlow_connection : siteCurrentPowerFlow["connections"].as<JsonArray>()) {

            from = siteCurrentPowerFlow_connection["from"]; // "PV", ...
            to = siteCurrentPowerFlow_connection["to"]; // "Load", ...

          }
          GRIDCurrentPower = siteCurrentPowerFlow["GRID"]["currentPower"]; //.as<float>();   // 0.0 if not present
          LOADCurrentPower = siteCurrentPowerFlow["LOAD"]["currentPower"]; //.as<float>();
          PVCurrentPower = siteCurrentPowerFlow["PV"]["currentPower"]; //.as<float>();
          Serial.printf("Mesures SolarEdge : grid : %.3f, load : %.3f, PV : %.3f, to : %s, from : %s\n",GRIDCurrentPower,LOADCurrentPower,PVCurrentPower, to, from);
          rtn = true;
        }
        gotSolarEdgePayload = false;
      }
      return rtn;
    }

/*_________________________________________SETUP__________________________________________________________*/

    void setup() {

      Serial.begin(115200);
      u8g2.begin(); // ECRAN OLED
      u8g2.enableUTF8Print(); //nécessaire pour écrire des caractères accentués

                      /* ------- gestion sonde temp ds18b20 ----------*/
      if(sondeTemp) {
        ds18b20.begin(); // initialisation du capteur DS18B20
      }
                      /* ------- gestion triac ----------*/
//      triac1.begin(OFF);
//      triac1.setState(ON);
//    Triac triac1(pinTriac, pinZeroCross);
//        triac1.begin(NORMAL_MODE, ON);
      DimmableLight::setSyncPin(pinZeroCross);
      DimmableLight::begin();

                      /* ------- init de plage Marche Forcéé ----------*/
      plageMarcheForcee[0].heureOnMarcheForcee = heureOnMarcheForcee;               
      plageMarcheForcee[0].minuteOnMarcheForcee = minuteOnMarcheForcee;               
      plageMarcheForcee[0].tempsChauffe = tempsChauffe;               
      plageMarcheForcee[1].heureOnMarcheForcee = heureOnMarcheForceeSec;               
      plageMarcheForcee[1].minuteOnMarcheForcee = minuteOnMarcheForceeSec;               
      plageMarcheForcee[1].tempsChauffe = tempsChauffe/2;               
      
                      /* ------- gestion PAC via relay ----------*/
      if(relayPAC){
        pinMode(RelayPAC,OUTPUT);
      }
                      /* ------- gestion PAC via espnow ----------*/
      if(espnowPAC){
        routeurEspnow.initESPNOW();
      }
                      /* ------- gestion PIR ----------*/
      if(motionSensor){         // PIR is instaled
        pinMode(pinMotionSensor, INPUT);
        initPhase = true;
      }
                      /* ------- gestion relay chauffeEau ----------*/
      if(relayChauffeEau){
        pinMode(RelayCauffeEauPin,OUTPUT);
      }

                      /* ------- gestion des taches sur les deux cores ----------*/
      xTaskCreatePinnedToCore(      //Code pour créer un Task Core 0//
          Task1code,   /* Task function. */
          "Task1",     /* name of task. */
          10000,       /* Stack size of task */
          NULL,        /* parameter of the task */
          1,           /* priority of the task */
          &Task1,      /* Task handle to keep track of created task */
          0);          /* pin task to core 0 */
      delay(500);

      xTaskCreatePinnedToCore(    //Code pour créer un Task Core 1//
          Task2code,   /* Task function. */
          "Task2",     /* name of task. */
          10000,       /* Stack size of task */
          NULL,        /* parameter of the task */
          1,           /* priority of the task */
          &Task2,      /* Task handle to keep track of created task */
          1);          /* pin task to core 1 */
      delay(500);

                      /* ------- gestion du Wifi ----------*/
      if(WiFi.status() != WL_CONNECTED){
        if(startWiFi()) {
          server.begin();
//          serverCard.begin();
          if(espnowPAC) {
            routeurEspnow.initESPNOW();
          }
          typeUpdate.ecran = wifi;
          typeUpdate.web = wifi;
        }
      }

                      /* ------- gestion du Over The Air ----------*/
      initOTA(); // initialisation OTA Wifi

/*
                      /* ------- Entrees page web callback ----------
      marcheForceeBtn.update((marcheForcee)?1:0);
      horlogeRH.update(plageMarcheForcee[0].heureOnMarcheForcee);
      horlogeRM.update(plageMarcheForcee[0].minuteOnMarcheForcee);
      marcheForceeVolTemp.update((marcheForceeVol)?0:1);
      tempEauMin.update(temperatureEauMin);
      tempEau.update(temperatureEau);

      marcheForceeSecBtn.update((marcheForceeSec)?1:0);
      horlogeRHSec.update(plageMarcheForcee[1].heureOnMarcheForcee);
      horlogeRMSec.update(plageMarcheForcee[1].minuteOnMarcheForcee);

      volBallon.update(volumeBallon);
      resistanceBallon.update(resistance);
      nbHeureChauffe.update((int)(plageMarcheForcee[0].tempsChauffe/3600));
      nbMinuteChauffe.update((int)(plageMarcheForcee[0].tempsChauffe%3600/60));
      puissEDF.update(GRIDCurrentPower);    // is 0.0 at init
      puissMaison.update(LOADCurrentPower);
      puissPanneaux.update(PVCurrentPower);
      chauffeBallonBtn.update(0);           // set to OFF
      puissBallon.update(0);
      pourcentBallon.update(0);             // valTriac init = 0
      chauffePAC.update(0);                 // set to off at init
      Oled.update(oled);
      dashboard.sendUpdates();

      horlogeRH.attachCallback([&](int value){
        plageMarcheForcee[0].heureOnMarcheForcee = value;
        horlogeRH.update(heureOnMarcheForcee);
        dashboard.sendUpdates();
      });
      horlogeRM.attachCallback([&](int value){
        plageMarcheForcee[0].minuteOnMarcheForcee = value;
        horlogeRM.update(minuteOnMarcheForcee);
        dashboard.sendUpdates();
      });
      marcheForceeSecBtn.attachCallback([&](bool value){
          marcheForceeSec = value;
          marcheForceeSecBtn.update(value);
          dashboard.sendUpdates();
      });
      horlogeRHSec.attachCallback([&](int value){
        plageMarcheForcee[1].heureOnMarcheForcee = value;
        horlogeRHSec.update(heureOnMarcheForcee);
        dashboard.sendUpdates();
      });
      horlogeRMSec.attachCallback([&](int value){
        plageMarcheForcee[1].minuteOnMarcheForcee = value;
        horlogeRMSec.update(minuteOnMarcheForcee);
        dashboard.sendUpdates();
      });
      marcheForceeVolTemp.attachCallback([&](bool value){
        if(!marcheForcee){
          if(value == 0){
            marcheForceeVol = true;
            marcheForceeTemperature = false;
          } else {
            marcheForceeTemperature = true;
            marcheForceeVol = false;
          }
          marcheForceeVolTemp.update(value);
          dashboard.sendUpdates();
        } else {
          marcheForceeVolTemp.update((marcheForceeVol)?0:1);
          dashboard.sendUpdates();
        }
      });
      tempEauMin.attachCallback([&](int value){
        temperatureEauMin = value;
        tempEauMin.update(value);
        dashboard.sendUpdates();
      });
      chauffeBallonBtn.attachCallback([&](bool value){
        marcheForceeSwitch(value);
        chauffeBallonBtn.update(value);
        dashboard.sendUpdates();
      });
      chauffePAC.attachCallback([&](bool value){
        marcheForceePACSwitch(value);
        chauffePAC.update(value);
        dashboard.sendUpdates();
      });
      Oled.attachCallback([&](bool value){
        Oled.update(value);
        dashboard.sendUpdates();
        if(value == 0) {    // asked to switch off
          stopEcran();
        } else{             // asked to power on
          u8g2.setPowerSave(0);     // weak up !
          (!marcheForcee) ? gestEcran(full) : gestEcran(mForcee);
        }
      });
*/
                /* ------ Init ecran ------*/
      if (oled){
        u8g2.setPowerSave(0); // turn it on
        gestEcran(full);
      }

      initPhase = true;
    }     // end setup

/*_________________________________________TASK CORE 1&2__________________________________________________________*/

  		//programme utilisant le Core 1 de l'ESP32//
    void Task1code( void * pvParameters ){
        bool doMarcheForcee = false;
      for(;;) {
            /* ------- debounce PIR ------*/
        if(motionSensor&&!initPhase){                                               // PIR is instaled
          if (changePIR){			 // get only the last value                                  // if status change has been detected
            if ((millis() - lastPIR_Time) > delayNoise) {                  // and if the change is stabile enough
              statusPIR = candidatePIR ;                                // then, candidate promoted to current status
              changePIR = false;                                        // no more change going on
              motion = true;    // (statusPIR == HIGH);
              Serial.printf("PIR sensor changed is now %s\n",(statusPIR == HIGH)?"HIGH":"LOW");
            }
          }
        }
            /* ----- gestion triac --------*/
        if(!marcheForcee){                  // fonctionnement normal

          if( !plageMarcheForcee[0].marcheForceeDone && 
              ((hour()>plageMarcheForcee[0].heureOnMarcheForcee) || 
               (hour()==plageMarcheForcee[0].heureOnMarcheForcee) && (minute()>=plageMarcheForcee[0].minuteOnMarcheForcee)
              )
            ){
              doMarcheForcee = true;
              plageMarcheForcee[0].marcheForceeDone = true;
              finMarcheForcee = now() + plageMarcheForcee[0].tempsChauffe;
          }   
          if(marcheForceeSec){
            if( !plageMarcheForcee[1].marcheForceeDone && 
                ((hour()>plageMarcheForcee[1].heureOnMarcheForcee) || 
                (hour()==plageMarcheForcee[1].heureOnMarcheForcee) && (minute()>=plageMarcheForcee[1].minuteOnMarcheForcee)
                )
              ){
                doMarcheForcee = true;
                plageMarcheForcee[1].marcheForceeDone = true;
                finMarcheForcee = now() + plageMarcheForcee[1].tempsChauffe;
            }   
          }

          if (doMarcheForcee){        // marche forcé if any
            doMarcheForcee = false;   // clear flag
            Serial.println("Starting Marche forcee");
            marcheForcee = true;
            triac1.turnOn();
            gestEcran(mForcee);
            typeUpdate.web = mForcee;
          } else {                  // pas heure de marche forcée : normal mode
            time_t maintenant = now();
//            Serial.printf("jour is %02d:%02d, nuit is %02d:%02d, now is %02d:%02d\n",hour(jour),minute(jour),hour(nuit),minute(nuit),hour(),minute());
            if((maintenant > jour) && (maintenant < nuit)){   // do it only in daylight
              if( getSolarEdgeValues() ) {
                typeUpdate.ecran = values;
                typeUpdate.web = values;
                currentPower = (PVCurrentPower - LOADCurrentPower)*1000; //Kw en W; si > 0 on produit trop ajouter triac, si < 0 arreter triac on ne produit pas assez
                Serial.printf("\nCurrentPower: %.0fW, PVPower; %.2fkW, LoadPower: %.2fkW\n\n",currentPower,PVCurrentPower,LOADCurrentPower);
                Serial.printf("debutOverPuissance: %02d:%02d, now: %02d:%02d\n",minute(debutOverPuissance),second(debutOverPuissance),minute(),second());
                if ( currentPower > 0 ){ 		    // currentPower > 0 puissance en trop rendue EDF : montée du triac
                  valTriac = currentPower / pasPuissance;
                  if (valTriac > 100) {
                    valTriac = 100;
                    Serial.printf("triac getBrightness : %d\n",triac1.getBrightness());
                    if(triac1.getBrightness()==255){    // triac already started resteP = current
                      restePuissance = currentPower;      // over production modulo resistance
                    } else {
                      restePuissance = currentPower-resistance ; // over production first time
                    }
                    if(debutOverPuissance == 0) debutOverPuissance = now();   // only if not set yet
                  } else {
                    restePuissance = 0.0;
                    debutOverPuissance = 0;      
                  }
                  if( (int)valTriac == 0){
                    triac1.turnOff();
                    Serial.printf("valTriac = 0 stop triac\n");
                  } else {
                    int power = (int)(255*valTriac/100);
                    Serial.printf("valTriac : %.2f, power : %d%, restePuissance: %.2f\n",valTriac, power,restePuissance);
                    triac1.setBrightness(power);
                  }
                } else {    // currentPower < 0 puissanse consommée sur EDF : arret du triac
                  restePuissance = 0.0;
                  debutOverPuissance = 0;     
                  triac1.turnOff();
                  Serial.printf("currentPower est negatif valTriac = 0 et triac off\n");
                }
                        // gestion relayPAC on si puissance > relayOn pendant 15mn et off si puissance < relayoff apres tempsMinPAC
                if(relayPAC || espnowPAC){
                  if(debutRelayPAC == 0) {   // relayPAC pas encore démarrée
                    Serial.printf(" Test si start pac debutOverPuissance:%02d:%02d:%02d, , now:%02d:%02d:%02d, now-debutOP : %d\n\n",hour(debutOverPuissance),minute(debutOverPuissance),second(debutOverPuissance),hour(),minute(),second(),now()-debutOverPuissance);
                    if ( (restePuissance >= relayPACOn ) && (debutOverPuissance != 0) && ((now() - debutOverPuissance) > tempsOverP ) ){   // overpuissance depuis plus de 15mn
                      Serial.printf("\n ==> Start PAC at %2d:%2d:%2d\n",hour(),minute(),second());
                      debutRelayPAC = now();
                      setRelayPac(HIGH);
                    }
                  } else {
                    Serial.printf(" Test si stop PAC now-debutRelayPac:%d, restPuissance:%.2f\n",now()-debutRelayPAC,restePuissance);
                    if( (now()-debutRelayPAC) > tempsMinPAC ){
                      if(restePuissance < relayPACOff){
                        Serial.printf("\n ==> Stop PAC at %2d:%2d:%2d\n",hour(),minute(),second());
                        debutRelayPAC = 0;
                        setRelayPac(LOW);
                      }
                    }
                  }
                }
              }		// end if getSolarEdgeValues()
            }   // end between jour and nuit
          }
        } else {                    // marcheForcee = true cancel when done
//          Serial.println(" Marche Forcée");
          if (marcheForceeVol){
            if( (now()-finMarcheForcee) >= 0 ){
              Serial.println("fin marche forcee");
                triac1.turnOff();
              marcheForcee = false;
              gestEcran(full);
              typeUpdate.web = full;
            }
          } else if (marcheForceeTemperature){
            if( (temperatureEau >= temperatureEauMin ) || ( (now()-finMarcheForcee) >= 0 )){  // stop marchforcee
              triac1.turnOff();
              marcheForcee = false;
              gestEcran(full);
              typeUpdate.web = full;
            }
          }
        }			// end if marcheForcee
        delay(25);
      }				// end for ever
    }

  		//programme utilisant le Core 2 de l'ESP32//
    void Task2code( void * pvParameters ){
      for(;;) {
  /* ------ Check OTA if needed ------ */
        ArduinoOTA.handle();
  /* ------ Gestion ecran selon PIR ------ */
        if(motionSensor&&!initPhase){         // PIR is instaled
          if(motion){             // going from low to high : motion detected !
            Serial.printf("PIR has changed, motion is True\n");
            if(!oled) {           // do stuff only if screen is off
                Serial.println("MOTION DETECTED!!!");
                (!marcheForcee) ? gestEcran(full) : gestEcran(mForcee);
                oled = true;
//                Oled.update(oled);      // update web
//                dashboard.sendUpdates();
            }
            lastReadTime = now();
            motion = false;
          } else {                // motion is still LOW check to Turn off the OLED after the number of seconds defined in the timeSeconds variable
// Serial.printf("test for no motion lastreadtime %d, timeout %d, delta %d\n",lastReadTime,timeoutEcran,(now() - lastReadTime));
            if( (lastReadTime != 0) && ((now() - lastReadTime) > timeoutEcran)) {
                Serial.println("Pas de Motion depuis TimoutEcran ...");
                stopEcran();
                oled = false;
//                Oled.update(oled);      // update web
//                dashboard.sendUpdates();
                lastReadTime = 0;
            }
          } 
        }

  /* ------ TIMER toutes les secondes ------ */
        if(second() != previousSecond){
//          Serial.printf("Second timer: %2d:%2d:%2d\n",hour(),minute(),second()); 
                 /* ------ Check PIR calibration ------*/
          if(motionSensor && initPhase && (nbSecondesPIR++>60)){
              Serial.println("Sensor is now calibrated");
              initPhase = false;
              attachInterrupt(pinMotionSensor, detectsMovementRising, RISING);
  //            attachInterrupt(pinMotionSensor, detectsMovementFalling, FALLING);
            }       

                /* ------ Check WIFI ------*/
          if(WiFi.status() != WL_CONNECTED){
            if(startWiFi()) {
              server.begin();
//              serverCard.begin();
              if(espnowPAC) {
                routeurEspnow.initESPNOW();
              }
              typeUpdate.ecran = wifi;
              typeUpdate.web = wifi;
            }
          }
                /* ------ check espnow has manager ------*/
          if(espnowPAC) {
            if(!routeurEspnow.hasManager()){
              routeurEspnow.sendClientHello();
              typeUpdate.ecran = espnow;
              typeUpdate.web = espnow;
            }
          }
                /* ------ update ecran ------*/
          if (oled){
            if (typeUpdate.ecran != none){
              gestEcran(typeUpdate.ecran);    // values or wifi
            } 
            gestEcran(horloge);               // update hour every secs
            if(WiFiStatus != WiFi.status()) {
              gestEcran(wifi);
            }
          }
                /* ------ update page web ------*/
          if(typeUpdate.web != none){
            gestWeb();
          }  
              /* ------ Check ntp si non OK ------*/
          if(!ntpOK){
            if(maxtriesNTP > 0) {            // stop if counter reached 0 reset next minute
              maxtriesNTP--;
              Serial.printf("ntpOK %s, maxreties %d\n",(ntpOK)?"true":"false",maxtriesNTP);
              if(WiFi.status() == WL_CONNECTED){
                ntpOK = getNTPTime();
                if(ntpOK) {
                  maxtriesNTP = MaxNTPRetries;               // reset counter if success
                  gestEcran(horloge);
                  gestWeb();
                }
              }
            }
          }
              /* ------ ask for new solaredge values if timeout ------*/
          if((now() >= jour) && (now() < nuit)){      // inutile quand pas de soleil
            if(!marcheForcee){                        // inutile lors de marche forcee
              if(nbSecondes++ > solarEdgeGetInfos) {
                Serial.println("calling solaredge");
                onTimerSolarEdge();
                nbSecondes = 0;
              }
            }
          }
              /* ----- reset previous seconds ----*/
          previousSecond = second();
        }     // end timer second

  /* ------ TIMER toutes les minutes ------ */
        if(minute() != previousMinute){
          Serial.printf("Minute timer: %d\n",minute());
          if(previousMinute != -1){
                /* ------ Check temperature si sonde ------*/
            if ( marcheForceeTemperature ){
              if(sondeTemp){                  // should be true if mForceeTemp
                float temperature_brute = -127;
                ds18b20.requestTemperatures();                             // demande de température au capteur //
                temperature_brute = ds18b20.getTempCByIndex(0);            // température en degrés Celcius
                if (temperature_brute < -20 || temperature_brute > 130) {  //Invalide. Pas de capteur ou parfois mauvaise réponse
                  Serial.print("Mesure Température invalide ");
                } else {
                  if(temperatureEau != temperature_brute){
                    temperatureEau = temperature_brute;
                    if(marcheForcee){
                      typeUpdate.ecran = mfValues;
                      typeUpdate.web = mfValues;
                    }
                  }
                }
              } else {    // ?? should not be possible just for debug
                if(marcheForcee){
                  typeUpdate.ecran = mfValues;
                  typeUpdate.web = mfValues;
                }
              } 
            }
                /* ------ Check NTP  ------*/
            if(maxtriesNTP != MaxNTPRetries){
              if(nbMinutes++ > ntpWait) {
                maxtriesNTP = MaxNTPRetries;   // reset counter;
                nbMinutes = 0;
              }
            }  
                /* ------ Refresh ecran every 5 mn ------*/
            if((oled) && (nbMinutesEcran++ > 3)){
              Serial.println("Refresh ecran !");
              (marcheForcee)? gestEcran(mForcee) : gestEcran(full);
              nbMinutesEcran = 0;
            }

            if((debugMarcheForcee) && (nbMinutesdebugMF++ > 2)){
              Serial.println("reset MForceeDone");
              marcheForceeDone = false;
              nbMinutesdebugMF = 0;
            }
          }
          previousMinute = minute();      // next minute
        }

  /* ------ TIMER toutes les heures ------ */
        if(hour() != previousHour){
          Serial.printf("Hour timer: %d; previousHour: %d\n",hour(),previousHour);
              /* ------ Update ntp si OK ------*/
          if(previousHour != -1){                             // do only if not init phase
            if((WiFi.status() == WL_CONNECTED) && ntpOK){     // recheck even if ntp ok if nok then done every secs. 
              ntpOK = getNTPTime();
              if(ntpOK) {
                maxtriesNTP = MaxNTPRetries;               // reset counter if success
                gestEcran(horloge);
                gestWeb();
              }
            }
          } 
          previousHour = hour();
              // change day !
          if(day() != lastday){
            if(lastday == -1){ 
              plageMarcheForcee[0].marcheForceeDone = true;  // at init wait for next day to set marcheforcee
              plageMarcheForcee[1].marcheForceeDone = false;   
             } else {
              plageMarcheForcee[0].marcheForceeDone = false;   
              plageMarcheForcee[1].marcheForceeDone = false;   
             }
            lastday = day();
            calculDureeJour(month());   // change jour et nuit chaque jour
          }
        }
        delay(25);
      }				// end for ever
    }


/*__________________________________________LOOP__________________________________________________________*/

    void loop() {
      // put your main code here, to run repeatedly:
    }

/*__________________________________________ OTA __________________________________________________________*/

    void initOTA() {

      ArduinoOTA.setHostname("Ludo'Solaire routeur");
      ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

      ArduinoOTA
      .onStart([]() {
        String type;
        (ArduinoOTA.getCommand() == U_FLASH) ? (type = "sketch") : (type = "filesystem");
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });

      ArduinoOTA.begin();
    }

/*__________________________________________HELPER_FUNCTIONS______________________________________________*/

    int dstOffset(time_t newTime){  //Adjust for DST
        tmElements_t te;
        time_t dstStart,dstEnd;

        te.Year = year(newTime)-1970;
        te.Month = 3;
        te.Day = 25;
        te.Hour = 3;
        te.Minute = 0;
        te.Second = 0;
        dstStart = makeTime(te);
        dstStart = nextSunday(dstStart);  //first sunday after 25 mars
        te.Month = 10;
        dstEnd = makeTime(te);
        dstEnd = nextSunday(dstEnd);      //first sunday after 25 octobre

        if (newTime>=dstStart && newTime<dstEnd)
            return (3600);                  //Add back in one hours worth of seconds - DST in effect
        else
            return (0);                     //NonDST
    }

    bool getNTPTime(){
      bool rtn = false;

        if(WiFi.status() == WL_CONNECTED){
                    // now connect to ntp server and get time
            timeClient.begin();
            if(timeClient.update()){ // we could update the time
                time_t newTime = timeClient.getEpochTime();   //time in seconds since Jan. 1, 1970
                Serial.print(F(" New TIME from NTP : "));
                Serial.println(timeClient.getFormattedTime());
                newTime = newTime + (3600 * TZ_OFFSET);
                newTime = newTime + dstOffset(newTime);  //Adjust for DLT
                setTime(newTime);
                Serial.print(F(" New TIME in calcuated : "));
                Serial.printf("%02d/%02d/%02d %02d:%02d:%02d \n", day(), month(), year(), hour(), minute(), second() );
                rtn = true;
            }else{
                Serial.println(F("Can't get time from ntp server"));
                rtn = false;
            }
    //        setSyncProvider(timeClient.getEpochTime);       // the function to get the time from the RTC
    //        setSyncInterval(3600*12);          // set refresh to every 12 hour (30 days = 2592000);
        }
        return rtn;
    }

    void marcheForceeSwitch(boolean value){
      if(marcheForcee && !value){       // user asked to switch off
        Serial.println("fin marche forcee web");
        triac1.turnOff();
        marcheForcee = false;
        gestEcran(full);
        typeUpdate.web = full;
      } else if(!marcheForcee && value) {  // user asked to force marche forcée
        Serial.println("Starting Marche forcee web");
        finMarcheForcee = now() + tempsChauffe;
        marcheForcee = true;
        triac1.turnOn();
        gestEcran(mForcee);
        typeUpdate.web = mForcee;
      }
    }

    void marcheForceePACSwitch(boolean value){
      if( (debutRelayPAC != 0) && !value) {   // asked to stop PAC
        Serial.println("Stop PAC by Web");
        debutRelayPAC = 0;
        setRelayPac(LOW);
      } else if( (debutRelayPAC == 0) && value ){  // asked to start PAC
        Serial.println("Start PAC by Web");
        debutRelayPAC = now();
        setRelayPac(HIGH);
      }
      typeUpdate.ecran = values;
    }

    void setRelayPac(uint8_t state) {
      if(relayPAC) {
        digitalWrite(RelayPAC, state);
      } else if(espnowPAC) {
        routeurEspnow.setRouteurDatas(GRIDCurrentPower,LOADCurrentPower,PVCurrentPower,state,debutRelayPAC);
        routeurEspnow.sendRouteurData();
      }
    }

    void gestEcran(actionEcranWeb action){
        char heure[20];

      if(!oled){              // if ecran off
        u8g2.setPowerSave(0); // turn it on
      }
//      Serial.printf("calling gestEcran with action %d\n",action);
      if(action == full){
        u8g2.clearBuffer(); // on efface ce qui se trouve déjà dans le buffer
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(5, 7); 			// position du début du texte (droite et bas)
        u8g2.print("Ludo's S"); 			// écriture de texte
        u8g2.setFont(u8g2_font_unifont_t_symbols);
        u8g2.drawGlyph(37, 11, 0x2600);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(48, 7); 			// position du début du texte
        u8g2.print("laire"); 		// écriture de texte
                // add ip
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(28, 15);
        IPAddress ip = WiFi.localIP();
        byte byte4 = ip[3];
        u8g2.print("v1   Web:");         // affichage dernier byte adresse ip //
        u8g2.print(byte4);          // affichage dernier byte adresse ip //

                // Frame
        u8g2.drawRFrame(25,17,50,17,4); 	  // rectangle x et y haut gauche / longueur / hauteur / arrondi //
                // icones edf,maison,pv
        u8g2.setFont(u8g2_font_streamline_interface_essential_wifi_t);
        u8g2.drawGlyph(1, 62, 0x0031);    // EDF (wifi)
        u8g2.setFont(u8g2_font_streamline_building_real_estate_t);
        u8g2.drawGlyph(54, 62, 0x0032);    // Maison
        u8g2.setFont(u8g2_font_streamline_ecology_t);
        u8g2.drawGlyph(109, 62, 0x003E);    // solar plant
      }
                // Marche Forcee
      if(action == mForcee){
        u8g2.clearBuffer(); // on efface ce qui se trouve déjà dans le buffer
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(5, 7); 			// position du début du texte (droite et bas)
        u8g2.print("Ludo's S"); 			// écriture de texte
        u8g2.setFont(u8g2_font_unifont_t_symbols);
        u8g2.drawGlyph(37, 11, 0x2600);
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(48, 7); 			// position du début du texte
        u8g2.print("laire"); 		// écriture de texte
                // add ip
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(28, 15);
        IPAddress ip = WiFi.localIP();
        byte byte4 = ip[3];
        u8g2.print("v1   Web:");         // affichage dernier byte adresse ip //
        u8g2.print(byte4);          // affichage dernier byte adresse ip //

                // Frame
        u8g2.drawRFrame(23,19,99,20,5); 	// rectangle x et y haut gauche / longueur / hauteur / arrondi //
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(28, 33);
        u8g2.print("Marche forcée");
        u8g2.setFont(u8g2_font_streamline_all_t);
        u8g2.drawGlyph(5, 40, 0x00d9);      // radioactiv
        u8g2.setFont(u8g2_font_6x10_tf);
      }
                // wifi sign
      if((action == wifi) || (action == full) || (action == mForcee)){
        if(WiFi.status() != WL_CONNECTED){
          u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
          u8g2.setDrawColor(0);
          u8g2.drawBox(118, 0,8,8);   // erase wifi symbol
          u8g2.setDrawColor(1);
          WiFiStatus = WL_DISCONNECTED;
        } else {
          u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
          u8g2.drawGlyph(118, 8, 0x51);   // wifi symbol
          WiFiStatus = WL_CONNECTED;
        }
      }
                // espnow icon
      if((action == espnow) || (action == full) || (action == mForcee)){
        u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
        if(routeurEspnow.hasManager()){
          u8g2.drawGlyph(118, 18, 0x46);   // espnow symbol
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawBox(118, 10, 8, 8);   // erase espnow symbol
          u8g2.setDrawColor(1);
        }
      }
                // values
      if((action == values) || (action == full)){
                // conso/prod
        u8g2.setFont(u8g2_font_7x13B_mn);
        if((int)(LOADCurrentPower/10) != 0){
          u8g2.setCursor(35,30);
        } else {
          u8g2.setCursor(44,30);
        }
        u8g2.print(LOADCurrentPower,2);       // print dans le cadre
                // grid power
//          u8g2.setFont(u8g2_font_squeezed_b6_tr);     
        u8g2.setFont(u8g2_font_6x10_tf);
        if(GRIDCurrentPower != 0){
          if((int)(GRIDCurrentPower/10) != 0){
            u8g2.setCursor(21, 64);
          } else {
            u8g2.setCursor(28, 64);
          }
          u8g2.print(GRIDCurrentPower,2);   // print edf/maison  
        }
                // PV power
        if(PVCurrentPower != 0){
          if((int)(GRIDCurrentPower/10) != 0){
            u8g2.setCursor(78, 64);
          } else {
            u8g2.setCursor(85, 64);
          }
          u8g2.print(PVCurrentPower,2);   // print pv:maison
        }
                // fleches 
        u8g2.setFont(u8g2_font_open_iconic_arrow_2x_t);
        if (currentPower > 0) {               
          u8g2.drawGlyph(32, 56, 0x0041);    // over prod donc fleche gauche 
        } else if(currentPower < 0){                              
          u8g2.drawGlyph(30, 56, 0x0042);    // on consomme donc fleche droite
        }
        if(PVCurrentPower != 0){
            u8g2.drawGlyph(84, 56, 0x0041);  // fleche gauche 
        }
                // Face happy/sad
        if (currentPower >= 0) {
          if(currentPower > pasPuissance){            // over production
            u8g2.setFont(u8g2_font_emoticons21_tr);
            u8g2.drawGlyph(5, 37, 0x0036);            // happy with glasses x36=54
          } else {                                    // juste assez
            u8g2.setFont(u8g2_font_emoticons21_tr);
            u8g2.drawGlyph(5, 37, 0x0024);            // normal x24=36
          }
        } else {                                      // on consomme chez EDF
          u8g2.setFont(u8g2_font_emoticons21_tr);
          u8g2.drawGlyph(5, 37, 0x0026);              // sad x26=38
        }
                // chauffe eau icon 
        u8g2.setFont(u8g2_font_open_iconic_thing_2x_t);
//        if(triac1.getPower() != 0){
        if(triac1.getBrightness() != 0){
          u8g2.drawGlyph(97, 15, 0x48);   // chauffeEau symbol
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawBox(97, 0, 16,16);   // erase chauffEau symbol
          u8g2.setDrawColor(1);
        }
                // PAC icon
        u8g2.setFont(u8g2_font_open_iconic_app_2x_t);
        if(debutRelayPAC != 0){
          u8g2.drawGlyph(76, 15, 0x40);   // pac symbol
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawBox(76, 0, 16, 15);   // erase pac symbol
          u8g2.setDrawColor(1);
        }

      }
                // horloge
      if((action == horloge) || (action == full)){
        u8g2.setFont(u8g2_font_6x10_tf);
        if(!marcheForcee) {             // affichage complet pas de marche forcee
          u8g2.setCursor(81,29);
        } else {
          u8g2.setCursor(50, 50);
        }
        sprintf(heure,"%02d:%02d:%02d",hour(),minute(),second());
        u8g2.print(heure);
      }
                // Marche Forcee valeurs
      if((action == mfValues) || (action == mForcee)){
        if ( marcheForceeVol ){
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.setCursor(30, 64);
          sprintf(heure,"Fin %02d:%02d,%02d", hour(finMarcheForcee),minute(finMarcheForcee),second(finMarcheForcee));
          u8g2.print(heure);
        }
        else if ( marcheForceeTemperature ){
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.setCursor(5, 64);
          u8g2.print("Temp Eau:");
          u8g2.setFont(u8g2_font_7x13B_tf);
          u8g2.print(temperatureEau,1);
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.print("C");
        }
                // chauffe eau icon 
        u8g2.setFont(u8g2_font_open_iconic_thing_2x_t);
        if(triac1.getBrightness() != 0){
//        if(triac1.getPower() != 0){
          u8g2.drawGlyph(97, 15, 0x48);   // chauffeEau symbol
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawBox(97, 0, 16, 15);   // erase chauffEau symbol
          u8g2.setDrawColor(1);
        }
                // PAC icon
        u8g2.setFont(u8g2_font_open_iconic_app_2x_t);
        if(debutRelayPAC != 0){
          u8g2.drawGlyph(76, 15, 0x40);   // pac symbol
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawBox(76, 0, 16, 15);   // erase pac symbol
          u8g2.setDrawColor(1);
        }

      } 

      u8g2.sendBuffer();  // l'image qu'on vient de construire est affichée à l'écran
      typeUpdate.ecran = none;
      oled = true;
    }

    void gestWeb(){
        // update main web site
      routeurWeb.OnUpdate();        

/*
        // update cards
      marcheForceeBtn.update((marcheForcee)?1:0);
      tempEau.update(temperatureEau);
      puissEDF.update( ((PVCurrentPower-LOADCurrentPower) > 0) ? -1*GRIDCurrentPower : GRIDCurrentPower);
      puissMaison.update(LOADCurrentPower);
      puissPanneaux.update(PVCurrentPower);
      chauffeBallonBtn.update((triac1.getBrightness() != 0) ? 1 : 0);
//      chauffeBallonBtn.update((triac1.getPower() != 0) ? 1 : 0);
      if(marcheForcee){
        puissBallon.update(resistance);
        pourcentBallon.update((marcheForcee) ? 100 : triac1.getBrightness());
//        pourcentBallon.update((marcheForcee) ? 100 : triac1.getPower());
      } else {
        if(currentPower > 0){
          puissBallon.update((currentPower > resistance) ? resistance : currentPower);
          pourcentBallon.update(triac1.getBrightness());
//          pourcentBallon.update(triac1.getPower());
        } else {
          puissBallon.update(0);
          pourcentBallon.update(0);
        }
      }
      chauffePAC.update( (debutRelayPAC != 0) ? 1 : 0);
      Oled.update(oled);
      dashboard.sendUpdates();
*/      
      typeUpdate.web = none;
    }

    void stopEcran(){
      u8g2.clearBuffer();
      u8g2.sendBuffer();
      u8g2.setPowerSave(1);
      oled = false;
    }

    void calculDureeJour(int theMonth){
        
        
        uint8_t heureDebut = journee[theMonth][0];
        uint8_t mnDebut = journee[theMonth][1];
        uint8_t heureFin = journee[theMonth][2];
        uint8_t mnFin = journee[theMonth][3];
        tmElements_t tm;

      breakTime(now(), tm);  // break time_t into elements stored in tm struct
      tm.Hour = heureDebut;
      tm.Minute = mnDebut;
      jour = makeTime(tm);   
      tm.Hour = heureFin;
      tm.Minute = mnFin;
      nuit = makeTime(tm);   
    }

