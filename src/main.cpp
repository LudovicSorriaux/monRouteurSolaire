/*******************************************************************************
 * @file    main.cpp
 * @brief   Point d'entrée principal - Routeur solaire ESP32
 * @details Setup: WiFiManager, AsyncWebServer, NTP, OLED (U8g2), DS18B20 (4x),
 *          TRIAC dimmer, SolarEdge API. Loop: gestion 2 cores ESP32 (core0:
 *          dimmer/sensors, core1: web/API). Routage surplus solaire vers
 *          chauffe-eau résistif.
 * 
 * @version 1.0
 * @date    2024
 * @author  Ludovic Sorriaux
 * 
 * Routeur solaire développé par Ludovic Sorriaux
 * ESP32 + Dimmer1 24A-600V + écran Oled + relay PAc + sonde température DS18B20 (4)
 * Utilisation des 2 Cores de l'Esp32
 ******************************************************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

      // Librairies //

#include <Arduino.h>
//#include <HardwareSerial.h> // https://github.com/espressif/arduino-esp32

#include <WiFi.h> // gestion du wifi
#include <AsyncTCP.h>   //  https://github.com/me-no-dev/AsyncTCP  ///
#include <ESPAsyncWebServer.h>  // https://github.com/me-no-dev/ESPAsyncWebServer  et https://github.com/bblanchon/ArduinoJson
#include <ESPmDNS.h>
//#include <espnow.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <NTPClient.h> // gestion de l'heure https://github.com/arduino-libraries/NTPClient //

#include <OneWire.h> // pour capteur de température DS18B20  https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // pour capteur de température DS18B20 https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <U8g2lib.h> // gestion affichage écran Oled  https://github.com/olikraus/U8g2_Arduino/ //

#include <TimeLib.h>
#include <ArduinoJson.h>
#include <AceButton.h>  
using namespace ace_button;

//#include <Triac.h>
//#include <RBDdimmer.h>
#include <dimmable_light.h>
#include <routeurWeb.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////// CONFIGURATION ///// PARTIE A MODIFIER POUR VOTRE INSTALLATION //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const uint8_t TZ_OFFSET = 1;                // faisseau horaire france : gmt+1
    boolean espnowPAC = false;                          // si PAC controlée par ESPNOW

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

      // pins attribution Broches utilisées//
#define pinTriac 19           // broche de commande du triac
#define pinZeroCross 18       // broche utilisée pour le zéro crossing

#define RelayPAC 17           // relay on/off déclenchement LOW si puissance PV > resistance ballon
#define RelayCauffeEauPin 16  // relay 16A mini marche forcée déclenchement LOW
#define pinDS18B20 5          // broche du capteur DS18B20 //
#define pinMotionSensor 26    // Set GPIOs for the PIR Motion Sensor

// for oled display
#define SDA 21
#define SCL 22

// boutons
#define BUTTON_ChauffeEau_PIN 23
#define BUTTON_ChauffeEau_ID 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


   /* -------------   Variables  -------------*/

        // user variables
    const bool debug = true;
    bool initPIRphase = true;
    bool initEcran = false;
    bool oled = true;
    bool motion = false;

        // global variables
    enum actionEcranWeb {none, entete, full, horloge, wifi, values, mForcee, mfValues, modeManu, modeManuValues, httpError};
    
    struct_configuration config;
    structMarcheForcee plageMarcheForcee[2];


    AsyncWebServer server = AsyncWebServer(80); 
    AsyncEventSource routeurEvents = AsyncEventSource("/routeurEvents");
    AsyncEventSource routeurParamsEvents = AsyncEventSource("/paramEvents");

    boolean marcheForcee = false;               // mise en route en automatique //

    float GRIDCurrentPower = 0.0;
    float LOADCurrentPower = 0.0;
    float PVCurrentPower = 0.0;
    float currentPower = 0.0;                 // si > 0 on produit trop, si < 0 on ne produit pas assez
    float pasPuissance = 0.0;                 // puissance resistance est entre 0 et 100 fois pasPuissance
    float valTriac = 0.0;
    float restePuissance = 0.0;
    char from[10];
    char to[10];



    time_t debutRelayPAC = 0;

        // local variables
    bool modeManuEau = false;
    bool modeManuPAC = false;
    bool flgSetManuEau = false;
    bool flgSetManuPac = false;

    int httpResponseCode = 0;
    bool mfChauffeEau = false;
    bool mfPAC = false;
    bool oldChauffeEau = false;
    bool oldPAC = false;
    bool oldModeManu = false;
    bool httpErreur = false;
    
    const int solarEdgeGetInfosNuit = 30*60;          // 300(5*60) Debug:30 nb secondes between each solaredge requests
    const int solarEdgeGetInfosJour = 1*60;           // 150(2*60+30) Debug:30 nb secondes between each solaredge requests
    int solarEdgeGetInfos = solarEdgeGetInfosJour;       

    const time_t timeoutEcran = 5*60;	                // 5*60 Debug : 60 en secondes
    time_t lastReadTime = 0;                     	    // the time stamp of the potential screen change (edge timer start)

    int16_t maxtriesNTP = MaxNTPRetries;
    uint8_t ntpWait = 60;                             // nb minutes between each NTP packets (MaxNTPRetries) calls
    bool ntpOK = false;

    TaskHandle_t Task1;
    TaskHandle_t Task2;

    actionEcranWeb typeUpdateEcran = none;

    bool WiFiConnectedOnce = false;
    wl_status_t OldWifiStatus = WL_DISCONNECTED;
    wl_status_t WifiStatus = WL_DISCONNECTED;

    int previousHour = -1;
    int previousMinute = -1;
    int previousSecond = -1;
    int lastday = -1;
    unsigned int nbSecondes = 0;                     // pour un timer multiple de secondes comme solarEdgeGetInfos
    unsigned int nbSecondesPIR = 0;
    unsigned int nbMinutes = 0; 
    unsigned int nbMinutesEcran = 0; 

    float temperatureEau = -127;  				          // La valeur vaut -127 quand la sonde DS18B20 n'est pas présente
    time_t tempsChauffe = 0;                        // en Sec = 4H par def // Temps de chauffe en secondes = 4 185 x volume du ballon x (température idéale - température actuelle) / puissance du chauffe-eau
    time_t finMarcheForcee = 0;                     // temps de fin de marche forcee ballon (now + temps chauffe)
    time_t debutOverPuissance = 0;

    JsonDocument solarEdgeInfo;                   

    volatile int candidatePIR = LOW;                // the un-filtered new status candidate of the PIR sensor
    volatile int statusPIR = HIGH;                  // the noise/spike-filtered status of the PIR sensor
    volatile boolean changePIR = false;             // the indication that a potential status change has happened
    volatile unsigned long lastPIR_Time = millis();  
    const long unsigned int delayNoise = 500;       // milliseconds. Status changes shorter than this are deemed noise/spike. 100ms works for me.

    WiFiUDP ntpUDP;
    //NTPClient timeClient(ntpUDP, "fr.pool.ntp.org", 3600, 60000);
    NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 24*3600*1000);
    const uint8_t journee[][4] = {{8,40, 17,20}, {8,20, 18,00}, {7,40, 18,30}, {7,50, 20,10}, {7,00, 20,40}, {6,30, 21,15}, {6,30, 21,30}, {7,00, 21,00}, {7,30, 20,20}, {8,00, 19,20}, {7,40, 17,30}, {8,20, 17,10}};            
    time_t jour = 0;   
    time_t nuit = 0;   

    // WebRouteur manager
    RouteurSolWebClass routeurWeb;                // gestion sur serveur web              

    // oled display SSD1306 124*64 I2C
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA);

    // bouttons 
    AceButton buttonChauffeEau;

    // Solar Edge
    WiFiClientSecure *clientSolarEdge;          // WiFiClient for clientSolarEdge https;
    String solarEdgeCurrentPowerFlowUrl = (String)"https://monitoringapi.solaredge.com/site/4129753/currentPowerFlow?api_key=";  // + config.solarEdge + "&format=application/json";
    
    String solarEdgePayload = "{}";             // get response from solaredge (should be a json)
    bool gotSolarEdgePayload = false;

    // DS18B20 Class usage
    OneWire oneWire(pinDS18B20); 			          // instance de communication avec le capteur de température
    DallasTemperature ds18b20(&oneWire); 	      // correspondance entreoneWire et le capteur Dallas de température

    // inner classes
    DimmableLight triac1(pinTriac);             // syncPin in setup
    //    DimmableLight triac1(pinTriac, pinZeroCross); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards

// function definitions here:
      // timer and interrupts
    void detectsMovementRising();               // sensor edge detection interrupt service routine
    void detectsMovementFalling();              // sensor edge detection interrupt service routine
  
      // task cores funtions 
    void Task1code( void * pvParameters );
    void Task2code( void * pvParameters );  

      // wifi funtions 
    const char* wl_status_to_string(wl_status_t status);
    char* findPassword(const char *ssid);
    bool WiFiConnect(const char *ssid, const char *passphrase);
    bool ConnectWithConfigFileCredentials();
    bool ConnectWithStoredCredentials();
    bool startWiFi();

      // Web functions;
    bool getSolarEdgeValues();
    void IRAM_ATTR getSolarEdgeInfos();  //Interruption every minute
    const char* http_status_to_string(int status);

    
      // Configuration Functions
    void startSPIFFS();
    void listAllFilesInDir(String dir_path,uint8_t deep);
    void printConfiguration();
    void loadConfiguration();
    void saveConfiguration();
    void saveNewConfiguration(const char *adminPassword,const char *user,const char * user_password,const char * ssid, const char * ssid_password);
    void resetWifiSettingsInConfig();
    String formatBytes(size_t bytes);

      // time functions 
    int  dstOffset(time_t newTime);
    bool getNTPTime();
    void calculDureeJour(int theMonth);

      // routeur functions
    void marcheForceePACSwitch(boolean value);
    void marcheForceeSwitch(boolean value);
    void setRelayPac(uint8_t state);
    void handleButtonEvent(AceButton* button, uint8_t eventType, uint8_t buttonState);
    void changePIRmode(bool val);

      // Feed Back ecran or web fnctions
    void gestEcran(actionEcranWeb action);
    void stopEcran();
    void gestWeb();


/*_________________________________________Timers & interuptions functions__________________________________________________________*/

  /**
   * @brief ISR détection front montant du capteur PIR - enregistre candidat HIGH pour détection mouvement
   * 
   * Interrupteur matériel (IRAM_ATTR) déclenché sur RISING du pin PIR.
   * Positionne candidatePIR=HIGH, changePIR=true et mémorise timestamp pour filtrage anti-rebond
   * (delayNoise 500ms). Ne modifie pas statusPIR directement (fait dans loop pour éviter glitches).
   */
    void IRAM_ATTR detectsMovementRising() {
        changePIR = true;                                        	// Status change detected
        candidatePIR = HIGH;                                         // Nouveau candidat (filtrage anti-rebond dans Task2code)
        lastPIR_Time = millis();	    	                          // Timestamp pour debouncing logiciel
    }

  /**
   * @brief ISR détection front descendant du capteur PIR - enregistre candidat LOW pour fin mouvement
   * 
   * Interrupteur matériel déclenché sur FALLING du pin PIR.
   * Positionne candidatePIR=LOW, changePIR=true et timestamp.
   * Filtrage anti-rebond géré dans Task2code (debouncing logiciel via delayNoise).
   */
    void IRAM_ATTR detectsMovementFalling() {
        changePIR = true;                                        	// Status change detected
        candidatePIR = LOW;                                          // Nouveau candidat fin mouvement
        lastPIR_Time = millis();	    	                          // Timestamp pour debouncing logiciel
    }

    #if defined(ESP8266)
      void WiFiStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
        Serial.print("\t[WIFI] Station connected: ");
        Serial.println("\t[WIFI] Station connected: ");
        Serial.print("\t\tIP address: ");
        Serial.print(WiFi.localIP());
        Serial.print("\t\t MAC address: ");
        Serial.print(WiFi.macAddress());
        Serial.print("\t\t Wifi Channel: ");
        Serial.println(WiFi.channel());
        WiFi.printDiag(Serial);
      }
      
      void WiFiStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
        Serial.print("\t[WIFI] Station disconnected: ");
      }
    #else
      String decodeWiFiEvent(WiFiEvent_t event) {
        String rtnStr;

        switch (event) {
          case ARDUINO_EVENT_WIFI_READY:               rtnStr = "WiFi interface ready"; break;
          case ARDUINO_EVENT_WIFI_SCAN_DONE:           rtnStr = "Completed scan for access points"; break;
          case ARDUINO_EVENT_WIFI_STA_START:           rtnStr = "WiFi client started"; break;
          case ARDUINO_EVENT_WIFI_STA_STOP:            rtnStr = "WiFi clients stopped"; break;
          case ARDUINO_EVENT_WIFI_STA_CONNECTED:       rtnStr = "Connected to access point"; break;
          case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:    rtnStr = "Disconnected from WiFi access point"; break;
          case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: rtnStr = "Authentication mode of access point has changed"; break;
          case ARDUINO_EVENT_WIFI_STA_GOT_IP:          rtnStr = "Obtained IP address: " + WiFi.localIP().toString(); break;
          case ARDUINO_EVENT_WIFI_STA_LOST_IP:        rtnStr = "Lost IP address and IP address is reset to 0"; break;
          case ARDUINO_EVENT_WPS_ER_SUCCESS:          rtnStr = "WiFi Protected Setup (WPS): succeeded in enrollee mode"; break;
          case ARDUINO_EVENT_WPS_ER_FAILED:           rtnStr = "WiFi Protected Setup (WPS): failed in enrollee mode"; break;
          case ARDUINO_EVENT_WPS_ER_TIMEOUT:          rtnStr = "WiFi Protected Setup (WPS): timeout in enrollee mode"; break;
          case ARDUINO_EVENT_WPS_ER_PIN:              rtnStr = "WiFi Protected Setup (WPS): pin code in enrollee mode"; break;
          case ARDUINO_EVENT_WIFI_AP_START:           rtnStr = "WiFi access point started"; break;
          case ARDUINO_EVENT_WIFI_AP_STOP:            rtnStr = "WiFi access point  stopped"; break;
          case ARDUINO_EVENT_WIFI_AP_STACONNECTED:    rtnStr = "Client connected"; break;
          case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: rtnStr = "Client disconnected"; break;
          case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:   rtnStr = "Assigned IP address to client"; break;
          case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:  rtnStr = "Received probe request"; break;
          case ARDUINO_EVENT_WIFI_AP_GOT_IP6:         rtnStr = "AP IPv6 is preferred"; break;
          case ARDUINO_EVENT_WIFI_STA_GOT_IP6:        rtnStr = "STA IPv6 is preferred"; break;
          case ARDUINO_EVENT_ETH_GOT_IP6:             rtnStr = "Ethernet IPv6 is preferred"; break;
          case ARDUINO_EVENT_ETH_START:               rtnStr = "Ethernet started"; break;
          case ARDUINO_EVENT_ETH_STOP:                rtnStr = "Ethernet stopped"; break;
          case ARDUINO_EVENT_ETH_CONNECTED:           rtnStr = "Ethernet connected"; break;
          case ARDUINO_EVENT_ETH_DISCONNECTED:        rtnStr = "Ethernet disconnected"; break;
          case ARDUINO_EVENT_ETH_GOT_IP:              rtnStr = "Obtained IP address"; break;
          default:                                    break;
        }
        return rtnStr;
      }
    
      void WiFiDebugEvent(WiFiEvent_t event, WiFiEventInfo_t info) { 
        Serial.printf("\t[WiFi-event] Got Event: %d : %s\n", event,decodeWiFiEvent(event).c_str()); 
      }

      void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
        Serial.println("\t[WiFi-event] Connected to AP successfully!");
        Serial.print("\t\tIP address: ");
        Serial.print(WiFi.localIP());
        Serial.print("\t\t MAC address: ");
        Serial.print(WiFi.macAddress());
        Serial.print("\t\t Wifi Channel: ");
        Serial.println(WiFi.channel());
        WifiStatus = WL_CONNECTED;
        WiFi.setAutoReconnect(true);
      }
      
      void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
        Serial.println("\t[WiFi-event] WiFi connected");
        Serial.print("\t\t IP address: ");
        Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
      }
    
      void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
        if(WifiStatus == WL_DISCONNECTED){
          Serial.println("\t[WiFi-event] Already been called as Disconnected !");
        } else {
          Serial.println("\t[WiFi-event] Disconnected from WiFi access point");
          Serial.print("\t[WiFi-event] WiFi lost connection. Reason: ");
          Serial.println(info.wifi_sta_disconnected.reason);
          WifiStatus = WL_DISCONNECTED;
        }
      }
    #endif
  
  
    
/*_________________________________________ Wifi functions __________________________________________________________*/

    const char* wl_status_to_string(wl_status_t status) {
      switch (status) {
        case WL_NO_SHIELD: return "WL_NO_SHIELD";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
    #if defined(ESP8266)
        case WL_WRONG_PASSWORD: return "WL_WRONG_PASSWORD";
    #endif
      }
      return "";
    }

    char* findPassword(const char *ssid){
      for(int i=0; i<MAX_WIFI; i++){
        if(strcmp(  ssid,config.wifi[i].ssid) == 0){
          return config.wifi[i].ssid_passwd;
          break;
        }    
      }
      return nullptr;
    }

    bool WiFiConnect(const char *ssid, const char *passphrase){
       unsigned long mytimeout = millis() / 1000;
       WiFi.persistent( true );
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

    bool ConnectWithStoredCredentials(){
      const char *ssid = "null";
      const char *password = nullptr;
      int retryNetworks = 4;

      while (retryNetworks-- > 0){
        Serial.printf("Retry %d \n", retryNetworks);
        Serial.print("Looking for networks : ");
        int networksCount = WiFi.scanNetworks(false);
        Serial.printf("%d\n", networksCount);
        for (int j = 0; j < networksCount; j++){
          Serial.printf("network %d : %s \n", j+1, WiFi.SSID(j).c_str());
        }
        if (networksCount >= 0){
          for (int i = 0; i < networksCount; i++){
            if(strcmp(ssid,WiFi.SSID(i).c_str()) != 0){     // only if new ssid isn't the last one. 
              ssid = strdup(WiFi.SSID(i).c_str());
              Serial.printf("ssid: %s \n", ssid);
              password = findPassword(ssid);  

              if (*ssid != 0x00 && ssid && password){
                Serial.printf("Trying to connect to ssid %s with pwd : %s\n",ssid,password);
                WiFi.persistent(true);
                if (WiFiConnect(ssid, password)){
                  Serial.println("Connected to WiFi network with ssid from saved params");
                  WiFi.scanDelete();
                  return true;
                } else {
                  Serial.println("\nCan't connect to this WIFI ");    
                  Serial.println(wl_status_to_string(WiFi.status()));
                }
              }
            }
          }
        }
        delay(250);
      }
    return false;
  }

    bool ConnectWithConfigFileCredentials(){
      char ssid[MAX_NAME_SIZE];
      char password[MAX_NAME_SIZE*2];
      bool rtn = false;

      for( int i=0;i<MAX_WIFI;i++){
        strlcpy(ssid, config.wifi[i].ssid,MAX_NAME_SIZE);
        strlcpy(password,config.wifi[i].ssid_passwd,MAX_NAME_SIZE*2);
        if(ssid[0] != '\0'){
          Serial.printf("Trying to connect to ssid %s with pwd : %s\n",ssid,password);
          if (WiFiConnect(ssid, password)){
            Serial.println("Connected to WiFi network with ssid from saved params");
            return true;
            break;
          } else {
            Serial.printf("\nCan't connect to %s WIFI : error : %s\n",ssid,wl_status_to_string(WiFi.status()));    
          }
        } else {    // last ssid
          Serial.println("Last wifi tested !!");    
          rtn = false;
          break;
        }
      } // end for
      return rtn;
    }

    bool startWiFi() {
      bool rtn = false;

      if(WiFi.status() != WL_CONNECTED){
        Serial.println(F(" Try to connect the Wifi..."));
        WiFi.mode(WIFI_STA);
        if(WiFiConnectedOnce) {     // try reconnect is easier
          Serial.println(F(" First try with last connected wifi ssid ..."));
          rtn = WiFi.reconnect();
          if(rtn) {
            unsigned long mytimeout = millis() / 1000;
            while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED){
              Serial.print(".");
              delay(500);
              if((millis() / 1000) > mytimeout + 10){ // try for less than 10 seconds to connect to WiFi router
                Serial.println();
                rtn = false;
                break;
              }
            }
          } 
          if(!rtn) {
            Serial.println(F(" Reconnect was unsussefull ! ..."));
            WiFi.mode(WIFI_AP); // cannot erase if not in STA mode !
            WiFi.disconnect();    // to clean up before restesting
            WiFi.persistent(true);
          }
        }
        if(!rtn){
          Serial.println(F(" First try with last used wifi ssid ..."));
          if(!WiFiConnect(nullptr,nullptr)){
            Serial.println(F("WiFi cnx with last connection FAILED, Next try using configFile data ..."));
            if(!ConnectWithConfigFileCredentials()){
              Serial.println(F("WiFi cnx with last connection FAILED, Next try with ssids in config ..."));
              if(!ConnectWithStoredCredentials()){
                  Serial.println(F("WiFi with saved connections FAILED"));
              }
            }
          }
        }
        if(WiFi.status() == WL_CONNECTED) {
          Serial.println();
          Serial.println("WiFi connected :");
          Serial.print("\t\tIP address: ");
          Serial.print(WiFi.localIP());
          Serial.print("\t\t MAC address: ");
          Serial.print(WiFi.macAddress());
          Serial.print("\t\t Wifi Channel: ");
          Serial.println(WiFi.channel());
          WiFi.printDiag(Serial);
          WiFi.setAutoReconnect(true);
          WiFiConnectedOnce = true;      
          OldWifiStatus = WL_DISCONNECTED;
          WifiStatus = WL_CONNECTED;
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

/*_________________________________________ Web functions __________________________________________________________*/

    bool getSolarEdgeValues(){
            bool rtn = false;
            const char *localFrom;
            const char *localTo;
         

        if(gotSolarEdgePayload){
          Serial.printf("Decoding solaredge payload ... %s\n",solarEdgePayload.c_str());
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
              localFrom = siteCurrentPowerFlow_connection["from"]; // "LOAD" or 'GRID"
              localTo = siteCurrentPowerFlow_connection["to"]; // "Load", ...
              if((strcmp(localFrom,"GRID")==0)||(strcmp(localFrom,"LOAD")==0)) break;
            }
            strncpy(from,localFrom, sizeof(from));
            strncpy(to,localTo, sizeof(to));

            GRIDCurrentPower = siteCurrentPowerFlow["GRID"]["currentPower"]; //.as<float>();   // 0.0 if not present
            LOADCurrentPower = siteCurrentPowerFlow["LOAD"]["currentPower"]; //.as<float>();
            PVCurrentPower = siteCurrentPowerFlow["PV"]["currentPower"]; //.as<float>();
            Serial.printf("Mesures SolarEdge : grid : %.3f, load : %.3f, PV : %.3f, to : %s, from : %s\n",GRIDCurrentPower,LOADCurrentPower,PVCurrentPower, to, from);
            rtn = true;
// DEBUG
//            strcpy(from,"LOAD");
//            PVCurrentPower = 2.5;
          }
          gotSolarEdgePayload = false;
        }
        return rtn;
      }

    void getSolarEdgeInfos() {  //Interruption every minute
      if(WiFi.status() == WL_CONNECTED){
        clientSolarEdge = new WiFiClientSecure;
        if (clientSolarEdge) {
//          clientSolarEdge -> setCACert(solarEdgeCertificate);
          clientSolarEdge->setInsecure(); // set secure client without certificate
          {  // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
            HTTPClient httpSolarEdge;
            Serial.print("[HTTPS] begin...\n");
            if(httpSolarEdge.begin(*clientSolarEdge,solarEdgeCurrentPowerFlowUrl + config.solarEdge)){
              Serial.print("HTTPS started.. doing the GET request\n");
              httpSolarEdge.addHeader("Content-Type", "application/x-www-form-urlencoded");
              httpSolarEdge.addHeader("Content-Type", "application/json");
              httpSolarEdge.setConnectTimeout(500);    // connexion timeout 
              httpSolarEdge.setTimeout(5000);          // response timeout
              httpResponseCode = httpSolarEdge.GET();
              Serial.printf("[HTTPS] get... code: %d\n", httpResponseCode);
              if (httpResponseCode > 0) { // httpCode will be negative on error
                if (httpResponseCode == HTTP_CODE_OK) {
                  solarEdgePayload = httpSolarEdge.getString();
                  gotSolarEdgePayload = true;
                  Serial.printf("[HTTPS] get... payload: %s\n", solarEdgePayload.c_str());
                }
              } else {
                if(gotSolarEdgePayload) gotSolarEdgePayload = false;
                Serial.printf("[HTTPS] GET... failed, error: %s\n", httpSolarEdge.errorToString(httpResponseCode).c_str());
                gestEcran(httpError);
              }
            } else {
              if(gotSolarEdgePayload) gotSolarEdgePayload = false;
              Serial.printf("[HTTPS] Unable to connect\n");
              httpResponseCode = -12;                   //case -12: return "Unable to connect";
              gestEcran(httpError);
            }
            httpSolarEdge.end();
          }   // end scopping 
          delete clientSolarEdge;
        }
      } else {
        if(gotSolarEdgePayload) gotSolarEdgePayload = false;
        Serial.println("Not Connected to wifi");
        httpResponseCode = -13;                     // case -13: return "Not Connected to wifi";
        gestEcran(httpError);
      }
    }

    const char* http_status_to_string(int status) {
      switch (status) {
        case HTTP_CODE_OK: return "CODE OK";                                  // = 200,
        case HTTP_CODE_MOVED_PERMANENTLY: return "MOVED PERMANENTLY";         //  301,
        case HTTP_CODE_FOUND: return "FOUND";                                 //  302,
        case HTTP_CODE_NOT_MODIFIED: return "NOT MODIFIED";                   //  304,
        case HTTP_CODE_BAD_REQUEST: return "BAD REQUEST";                     //  400,
        case HTTP_CODE_UNAUTHORIZED: return "UNAUTHORIZED";                   //  401,
        case HTTP_CODE_FORBIDDEN: return "FORBIDDEN";                         //  403,
        case HTTP_CODE_NOT_FOUND: return "NOT FOUND";                         //  404,
        case HTTP_CODE_TOO_MANY_REQUESTS: return "TOO MANY REQUESTS";         //  429,
        case HTTP_CODE_INTERNAL_SERVER_ERROR: return "INTERNAL SERVER ERROR"; //  500,
        case HTTP_CODE_NOT_IMPLEMENTED: return "NOT IMPLEMENTED";             //  501,
        case HTTP_CODE_BAD_GATEWAY: return "BAD GATEWAY";                     //  502,
        case HTTP_CODE_SERVICE_UNAVAILABLE: return "SERVICE UNAVAILABLE";     //  503,

        case -1: return "CONNECTION FAILED";                                  //  (-1) HTTPC_ERROR_CONNECTION_REFUSED deprecated
        case HTTPC_ERROR_SEND_HEADER_FAILED: return "SEND HEADER FAILED";     //  (-2)
        case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "SEND PAYLOAD FAILED";   //  (-3)
        case HTTPC_ERROR_NOT_CONNECTED: return "NOT CONNECTED";               //  (-4)
        case HTTPC_ERROR_CONNECTION_LOST: return "CONNECTION LOST";           //  (-5)
        case HTTPC_ERROR_NO_STREAM: return "NO STREAM";                       //  (-6)
        case HTTPC_ERROR_NO_HTTP_SERVER: return "NO HTTP SERVER";             //  (-7)
        case HTTPC_ERROR_TOO_LESS_RAM: return "TOO LESS RAM";                 //  (-8)
        case HTTPC_ERROR_ENCODING: return "ENCODING";                         //  (-9)
        case HTTPC_ERROR_STREAM_WRITE: return "STREAM WRITE";                 //  (-10)
        case HTTPC_ERROR_READ_TIMEOUT: return "READ TIMEOUT";                 //  (-11)
        case -12: return "Unable to connect";                                 //  (-12 ludo's)
        case -13: return "Not Connected to wifi";                             //  (-13 ludo's)
      }
      return "";
    }

/*__________________________________________________________Config_FUNCTIONS__________________________________________________________*/

    void startSPIFFS() { // Start the SPIFFS and list all contents

      if(!LittleFS.begin()){          // Start the SPI Flash File System (LittleFS)
        Serial.println("An Error has occurred while mounting SPIFFS");
      } else {
        Serial.println("FS started. Contents:");
        listAllFilesInDir("/",0);
      }
    }

    // list files in FileSystem
    void listAllFilesInDir(String dir_path,uint8_t deep){
      String tabs;
      for (uint8_t i=0;i<deep;i++){
        tabs += '\t';
      }
      File dir = LittleFS.open(dir_path,"r");
      while(true) {
      File file = dir.openNextFile();
        if (!file) {
          break;                    // no more files
        }
        if (file.isDirectory()) {
          // print directory names
          Serial.print(tabs + "Dir: ");
          Serial.println(dir_path + file.name() + "/");
          // recursive file listing inside new directory
          listAllFilesInDir(dir_path + file.name() + "/",deep+1);
        } else {
          // print file names
          String toPrint = tabs + "File: %s%s\t%s\n";
          Serial.printf(toPrint.c_str(),dir_path.c_str(),file.name(),formatBytes(file.size()).c_str());
        }
      }
    }

    // Prints the content of a file to the Serial
    void printConfiguration() {
      uint8_t i = 0;

      Serial.println(F("Configuration in config struct is :"));
      Serial.printf("AdminPassord: %s\n",config.adminPassword);
      for(i=0;i<MAX_USERS;i++){
        Serial.printf("User %d : name %s, password %s\n",i,config.users[i].user, config.users[i].user_passwd);
      }
      for(i=0;i<MAX_WIFI;i++){
        Serial.printf("Wifi %d : ssid %s, ssid_password %s\n",i,config.wifi[i].ssid, config.wifi[i].ssid_passwd);
      }
      Serial.printf("volumeBallon: %d\n",config.volumeBallon);
      Serial.printf("puissanceBallon: %d\n",config.puissanceBallon);
      Serial.printf("heureBackup: %s\n",config.heureBackup);
      Serial.printf("tempEauMin: %d\n",config.tempEauMin);
      Serial.printf("secondBackup: %s\n",(config.secondBackup)?"true":"false");
      Serial.printf("heureSecondBackup: %s\n",config.heureSecondBackup);
      Serial.printf("sondeTemp: %s\n",(config.sondeTemp)?"true":"false");
      Serial.printf("pacPresent: %s\n",(config.pacPresent)?"true":"false");
      Serial.printf("puissancePac: %d\n",config.puissancePac);
      Serial.printf("puissPacOn: %d\n",config.puissPacOn);
      Serial.printf("puissPacOff: %d\n",config.puissPacOff);
      Serial.printf("tempsOverProd: %d\n",config.tempsOverProd);
      Serial.printf("tempsMinPac: %d\n",config.tempsMinPac);
      Serial.printf("afficheur: %s\n",(config.afficheur)?"true":"false");
      Serial.printf("motionSensor: %s\n",(config.motionSensor)?"true":"false");
      Serial.println();
    } 

    // Loads the configuration from a file
    void loadConfiguration() {
        JsonDocument jsonConfig;         // config file
        JsonArray table;
        JsonObject param;
        int i = 0, j = 0;
        uint8_t max_users = 0;
        uint8_t max_wifis = 0;
        String theStr;
    
      // Open file for reading
    //      File configFile = SPIFFS.open("/cfg/cuve.cfg","r");    
      File configFile = LittleFS.open("/cfg/routeur.cfg","r");    
      if (!configFile) {
        Serial.println(F("Cant to open config file"));
        return;
      }
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(jsonConfig, configFile);
      if(error.code() == DeserializationError::Ok){
        Serial.println(F("Deserialization succeeded"));
        configFile.seek(0,SeekSet);
        while (configFile.available()) {
          Serial.write(configFile.read());
        }
        Serial.println();
        // Copy values from the JsonDocument to the Config
        strlcpy(config.adminPassword,                      // <- destination
                jsonConfig["adminPassword"] | "manager",   // <- source
                MAX_USERNAME_SIZE               // <- destination's capacity
        );
        if (jsonConfig["users"].is<JsonArray>()){
          max_users = jsonConfig["users"].size();
          table = jsonConfig["users"];
          for (j=0;j<max_users;j++){
            strlcpy(config.users[j].user,table[j]["name"],MAX_USERNAME_SIZE);
            strlcpy(config.users[j].user_passwd,table[j]["password"],MAX_USERNAME_SIZE);
          }
        }  
        if (jsonConfig["wifi"].is<JsonArray>()){
          max_wifis = jsonConfig["wifi"].size();
          table = jsonConfig["wifi"];
          for (j=0;j<max_wifis;j++){
            strlcpy(config.wifi[j].ssid,table[j]["ssid"],MAX_NAME_SIZE);
            strlcpy(config.wifi[j].ssid_passwd,table[j]["password"],MAX_NAME_SIZE*2);
          }
        }  
        if (jsonConfig["solarEdge"].is<String>()){
          strlcpy(config.solarEdge,jsonConfig["solarEdge"],sizeof(config.solarEdge));
        }
        if (jsonConfig["parametres"].is<JsonObject>()){
          param = jsonConfig["parametres"];
          config.volumeBallon = param["volumeBallon"].as<int>();
          config.puissanceBallon = param["puissanceBallon"].as<int>();
          theStr = param["heureBackup"].as<String>();
          Serial.printf("heureBackup: %s\n",theStr.c_str());
          strcpy(config.heureBackup, theStr.c_str());
          Serial.printf("heureBackup(config): %s\n",config.heureBackup);
          config.tempEauMin = param["tempEauMin"].as<int>();
          config.secondBackup = param["secondBackup"].as<bool>();
          theStr = param["heureSecondBackup"].as<String>();
          Serial.printf("heureSecBackup: %s\n",theStr.c_str());
          strcpy(config.heureSecondBackup, theStr.c_str());
          Serial.printf("heureSecBackup(config): %s\n",config.heureSecondBackup);
          config.sondeTemp = param["sondeTemp"].as<bool>();
          if(config.sondeTemp){           // setup right variables
            temperatureEau = 0.0;
          } else {
            temperatureEau = -127.0;            // valeur pour sonde non presente
          }
          config.pacPresent = param["pacPresent"].as<bool>();
          config.puissancePac = param["puissancePac"].as<int>();
          config.puissPacOn = param["puissPacOn"].as<int>();
          config.puissPacOff = param["puissPacOff"].as<int>();
          config.tempsOverProd = param["tempsOverProd"].as<int>();
          config.tempsMinPac = param["tempsMinPac"].as<int>();
          config.afficheur = param["afficheur"].as<bool>();
          config.motionSensor = param["motionSensor"].as<bool>();       
          
          /* ------- init de plage Marche Forcéé ----------*/
          theStr=(String)config.heureBackup;
          plageMarcheForcee[0].heureOnMarcheForcee = atoi(theStr.substring(0,theStr.indexOf(":")).c_str());
          plageMarcheForcee[0].minuteOnMarcheForcee = atoi(theStr.substring(theStr.indexOf(":")+1).c_str());                

          theStr=(String)config.heureSecondBackup;
          plageMarcheForcee[1].heureOnMarcheForcee = atoi(theStr.substring(0,theStr.indexOf(":")).c_str());              
          plageMarcheForcee[1].minuteOnMarcheForcee = atoi(theStr.substring(theStr.indexOf(":")+1).c_str());                 
          Serial.printf("backup:%d:%d, secBackup:%d:%d, configB:%s, configBS:%s\n\n",plageMarcheForcee[0].heureOnMarcheForcee,plageMarcheForcee[0].minuteOnMarcheForcee,plageMarcheForcee[1].heureOnMarcheForcee,plageMarcheForcee[0].minuteOnMarcheForcee,config.heureBackup,config.heureSecondBackup);
        }
        Serial.println(F("JsonConfig file loaded !!"));
        // print the json config file
      } else {
        switch (error.code()) {
          case DeserializationError::InvalidInput:
              Serial.print(F("Invalid input! "));
              break;
          case DeserializationError::NoMemory:
              Serial.print(F("Not enough memory "));
              break;
          default:
              Serial.print(F("Deserialization failed "));
              break;
        }
        Serial.println(F("Deserialize error Failed to interpret config file, using default configuration : adminPw=manager, user=ludovic"));
            // need to load defaults options and wifi params too ....
        strlcpy(config.adminPassword,"manager",MAX_USERNAME_SIZE);
        strlcpy(config.users[0].user,"ludovic",MAX_USERNAME_SIZE);
        strlcpy(config.users[0].user_passwd,"Ludovic",MAX_USERNAME_SIZE);
        strlcpy(config.wifi[0].ssid,"SFR_2E48",MAX_NAME_SIZE);
        strlcpy(config.wifi[0].ssid_passwd,"rsionardishoodbe2rmo",MAX_NAME_SIZE*2);
        strlcpy(config.solarEdge,"8INR9G7TVYP03QAMRMNKJYRNN0MTVJSQ",sizeof(config.solarEdge));
        config.volumeBallon = 150;
        config.puissanceBallon = 1500;
        strlcpy(config.heureBackup,"20:00",sizeof(config.heureBackup));
        config.tempEauMin = 50;
        config.secondBackup = false;
        strlcpy(config.heureSecondBackup,"14:00",sizeof(config.heureSecondBackup));
        config.sondeTemp = false;
        temperatureEau = -127.0;            // valeur pour sonde non presente
        config.pacPresent = true;
        config.puissancePac = 1000;
        config.puissPacOn = 1000;
        config.puissPacOff = 800;
        config.tempsOverProd = 900;
        config.tempsMinPac = 7200;
        config.afficheur = true;
        config.motionSensor = false;
      }
      // Close the file (Curiously, File's destructor doesn't close the file)
      configFile.close();
      printConfiguration();
    }

    // Saves the configuration to a file
    void saveConfiguration() {
          JsonDocument jsonConfig;         // config file
          JsonArray users, wifis;
          JsonObject parametres;
          String jsonBuff;
          int i,j;

      // Open file for writing
      File configFile = LittleFS.open("/cfg/routeur.cfg", "w");   
      if (!configFile) {
        Serial.println(F("Failed to open config file for writing"));
        return;
      }

      jsonConfig["adminPassword"] = config.adminPassword;
      users = jsonConfig["users"].to<JsonArray>();
      for(i=0;i<MAX_USERS;i++){
        if(config.users[i].user[0] != '\0'){
          JsonObject user = users.add<JsonObject>();
          user["name"] = config.users[i].user;
          user["password"] = config.users[i].user_passwd;
        }
      }
      wifis = jsonConfig["wifi"].to<JsonArray>();
      for(i=0;i<MAX_WIFI;i++){
        if(config.wifi[i].ssid[0] != '\0'){
          JsonObject wifi = wifis.add<JsonObject>();
          wifi["ssid"] = config.wifi[i].ssid;
          wifi["password"] = config.wifi[i].ssid_passwd;
        }
      }
      jsonConfig["solarEdge"] = config.solarEdge;
      parametres = jsonConfig["parametres"].to<JsonObject>();
        parametres["volumeBallon"] = config.volumeBallon;
        parametres["puissanceBallon"] = config.puissanceBallon;
        parametres["heureBackup"] = config.heureBackup;
        parametres["tempEauMin"] = config.tempEauMin;
        parametres["secondBackup"] =   config.secondBackup;
        parametres["heureSecondBackup"] = config.heureSecondBackup;
        parametres["sondeTemp"] = config.sondeTemp;
        parametres["pacPresent"] = config.pacPresent;
        parametres["puissancePac"] = config.puissancePac;
        parametres["puissPacOn"] = config.puissPacOn;
        parametres["puissPacOff"] = config.puissPacOff;
        parametres["tempsOverProd"] = config.tempsOverProd;
        parametres["tempsMinPac"] = config.tempsMinPac;
        parametres["afficheur"] = config.afficheur;
        parametres["motionSensor"] = config.motionSensor;

      serializeJson(jsonConfig, jsonBuff);
      configFile.print(jsonBuff); 
      delay(1); 
      configFile.close();   //Close the file
      Serial.println (F("Json config file saved : "));
      Serial.println(jsonBuff);
      printConfiguration();
    }

    void saveNewConfiguration(const char *adminPassword,const char *user,const char * user_password,const char * ssid, const char * ssid_password) {
          bool foundUser = false, foundSSID = false;
          int i = 0;

      if(adminPassword != nullptr){
        Serial.printf("Saving new adminPassword: %s\n", adminPassword);
        strlcpy(config.adminPassword,adminPassword,sizeof(config.adminPassword));
      }

      if(user != nullptr){        // new user
        Serial.printf("Saving new user : name : %s, Pwd : %s\n",user, user_password);
        for(i=0;i<MAX_USERS;i++){
          if(config.users[i].user[0]==0){ // no more users
            break;
          } else if(strcmp(config.users[i].user,user)==0 ) {                         // if user exists  
            strlcpy(config.users[i].user_passwd,user_password,MAX_USERNAME_SIZE);
            foundUser = true;
            break;
          }
        }
        if(!foundUser){
          if (i<MAX_USERS){                                            // room for new info
            strlcpy(config.users[i].user,user,MAX_USERNAME_SIZE);
            strlcpy(config.users[i].user_passwd,user_password,MAX_USERNAME_SIZE);
          } else {                                                    // no space replace first entry (oldest)  
            strlcpy(config.users[0].user,user,MAX_USERNAME_SIZE);
            strlcpy(config.users[0].user_passwd,user_password,MAX_USERNAME_SIZE);
          }
        }
      }
      if(ssid != nullptr){        // new ssid
        Serial.printf("Saving wifi config : SSID : %s, SSIDPwd : %s\n",ssid, ssid_password);
        for(i=0;i<MAX_WIFI;i++){
          if(config.wifi[i].ssid[0]==0){ // no more ssids
            break;
          } else if(strcmp(config.wifi[i].ssid,ssid)==0 ) {                         // if ssid exists  
            strlcpy(config.wifi[i].ssid_passwd,ssid_password,sizeof(config.wifi[i].ssid_passwd));
            foundSSID = true;
            break;
          }
        }
        if(!foundSSID){
          if (i<MAX_WIFI){                                            // room for new info
            strlcpy(config.wifi[i].ssid,user,sizeof(config.wifi[0].ssid));
            strlcpy(config.wifi[i].ssid_passwd,ssid_password,sizeof(config.wifi[0].ssid_passwd));
          } else {                                                    // no space replace first entry (oldest)  
            strlcpy(config.wifi[0].ssid,user,sizeof(config.wifi[0].ssid));
            strlcpy(config.wifi[0].ssid_passwd,ssid_password,sizeof(config.wifi[0].ssid_passwd));
          }
        }
      }
      saveConfiguration();
    }

    void resetWifiSettingsInConfig() {
      for(uint8_t i=0;i<MAX_WIFI;i++){
        strcpy(config.wifi[i].ssid,"");
        strcpy(config.wifi[i].ssid_passwd,"");
      }
      saveConfiguration();
    }  

    String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
      String rtn;
      if (bytes < 1024) {
        rtn = String(bytes) + "B";
      } else if (bytes < (1024 * 1024)) {
        rtn = String(bytes / 1024.0) + "KB";
      } else if (bytes < (1024 * 1024 * 1024)) {
        rtn = String(bytes / 1024.0 / 1024.0) + "MB";
      }
      return rtn;
    }

/*_________________________________________SETUP__________________________________________________________*/

    void setup() {

      Serial.begin(115200);
      u8g2.begin(); // ECRAN OLED
      u8g2.setPowerSave(0);   // turn it on
      u8g2.enableUTF8Print(); //nécessaire pour écrire des caractères accentués
      u8g2.setFont(u8g2_font_ncenB14_tr);
      u8g2.setCursor(10, 30);
      u8g2.print("Init Solar");
      u8g2.setCursor(10, 48);
      u8g2.print("Routeur !");
      u8g2.sendBuffer();

      // Configuration 
      startSPIFFS();               // Start the SPIFFS and list all contents 
      loadConfiguration();
      pasPuissance = config.puissanceBallon / 100;    // puissance resistance est entre 0 et 100 fois pasPuissance

      if (!config.sondeTemp){             // marche forcee volume  
        tempsChauffe = 4185 * config.volumeBallon * ( config.tempEauMin - temperatureEau ) / config.puissanceBallon;   // Temps de chauffe en secondes = 4 185 x volume du ballon x (température idéale - température actuelle) / puissance du chauffe-eau
      } else {
        tempsChauffe = 4185 * config.volumeBallon * ( config.tempEauMin - 15 ) / config.puissanceBallon;   // en sec = 4h Temps de chauffe en secondes = 4 185 x volume du ballon x (température idéale - température actuelle) / puissance du chauffe-eau
      }  

        /* ------- gestion sonde temp ds18b20 ----------*/
      ds18b20.begin(); // initialisation du capteur DS18B20

        /* ------- gestion triac ----------*/
//      triac1.begin(OFF);
//      triac1.setState(ON);
//      Triac triac1(pinTriac, pinZeroCross);
//      triac1.begin(NORMAL_MODE, ON);
      DimmableLight::setSyncPin(pinZeroCross);
      DimmableLight::begin();
     
        /* ------- gestion PAC via relay ----------*/
      pinMode(RelayPAC,OUTPUT);
        /* ------- gestion PIR ----------*/
      pinMode(pinMotionSensor, INPUT);
      initPIRphase = true;

        /* ------- gestion des bouttons ----------*/
      pinMode(BUTTON_ChauffeEau_PIN, INPUT_PULLUP);
      buttonChauffeEau.init(BUTTON_ChauffeEau_PIN, HIGH, BUTTON_ChauffeEau_ID); 
              // Configure the ButtonConfig with the event handler, and enable all higher level events.
      ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
      buttonConfig->setEventHandler(handleButtonEvent);
//      buttonConfig->setFeature(ButtonConfig::kFeatureClick);
//      buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
//      buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
      buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
//      buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
                
        /* ------- gestion du Wifi ----------*/
      WiFi.persistent(true);
      WiFi.mode(WIFI_STA);
      #if defined(ESP8266)
        WiFi.onStationModeConnected(&WiFiStationConnected);
        WiFi.onStationModeDisconnected(&WiFiStationDisconnected);
      #else
        WiFi.onEvent(WiFiDebugEvent);
        WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
      #endif
      
      if(WiFi.status() != WL_CONNECTED){
        if(startWiFi()) {
          Serial.println("connected...yeey :)");
          // NTP
          while(maxtriesNTP-- != 0){   // wait to get a valid time
            ntpOK = getNTPTime();
            if (ntpOK) {
              maxtriesNTP = MaxNTPRetries;               // reset counter if success
              calculDureeJour(month()-1); 
              break;
            }
          }
          routeurWeb.startup();
        }
      }
        /* ------ Init ecran ------*/
      if (config.afficheur){
        u8g2.setPowerSave(0); // turn it on
        oled = true;
        initEcran = true;
      }

        /* ------ Init plageMarcheForcee ------*/


        tmElements_t tm;
        uint8_t heure;
        uint8_t minute;
        size_t newTimeMarcheForce;
        breakTime(now(), tm);  // break time_t into elements stored in tm struct
        sscanf(config.heureBackup, "%hhu:%hhu", &heure, &minute);
        tm.Hour = heure;
        tm.Minute = minute;
        newTimeMarcheForce = makeTime(tm);
        if(now()<newTimeMarcheForce){ // in future
          plageMarcheForcee[0].marcheForceeDone = false;
        } else {
          plageMarcheForcee[0].marcheForceeDone = true;  // at init wait for next day to set marcheforcee
        }   
        sscanf(config.heureSecondBackup, "%hhu:%hhu", &heure, &minute);
        tm.Hour = heure;
        tm.Minute = minute;
        newTimeMarcheForce = makeTime(tm);
        if(now()<newTimeMarcheForce){ // in future
          plageMarcheForcee[1].marcheForceeDone = false;
        } else {
          plageMarcheForcee[1].marcheForceeDone = true;   
        }

  /**
   * @brief Init ESP32 : Serial, OLED, LittleFS, config, DS18B20, TRIAC, WiFi, NTP, WebServer, 2 tasks (cores 0/1)
   * 
   * Séquence complète :
   * 1. Serial 115200, U8g2 OLED splash 'Init Solar Routeur'
   * 2. startSPIFFS()+loadConfiguration() - charge /cfg/routeur.cfg
   * 3. Calcul pasPuissance = puissanceBallon/100 pour contrôle TRIAC (0-100%)
   * 4. Calcul tempsChauffe (formule 4185*volume*(tempIdéale-tempActuelle)/puissance)
   * 5. ds18b20.begin() - init sondes température DS18B20
   * 6. DimmableLight::setSyncPin(pinZeroCross) + DimmableLight::begin() - init TRIAC dimmer
   * 7. pinMode RelayPAC/PIR/button, init AceButton avec handleButtonEvent
   * 8. WiFi.mode(WIFI_STA) + callbacks ESP32 (WiFiStationConnected, WiFiGotIP, WiFiStationDisconnected)
   * 9. startWiFi() - cascade connexion WiFi (reconnect / config / scan)
   * 10. getNTPTime() loop MaxNTPRetries - synchro horloge NTP
   * 11. calculDureeJour() - calcul timestamps jour/nuit depuis table journee[][]
   * 12. routeurWeb.startup() - démarre AsyncWebServer + mDNS
   * 13. Init plageMarcheForcee[] (heureOnMarcheForcee/minuteOnMarcheForcee) depuis config
   *     avec breakTime/makeTime pour vérifier si dans le futur
   * 14. xTaskCreatePinnedToCore(Task1code, core0) - TRIAC/SolarEdge/PAC
   * 15. xTaskCreatePinnedToCore(Task2code, core1) - OLED/PIR/WiFi/NTP/timers
   * 
   * Variables clés initialisées :
   * - pasPuissance : puissanceBallon/100 (pas de contrôle TRIAC 0-100%)
   * - tempsChauffe : durée marche forcée calculée (secondes)
   * - plageMarcheForcee[0/1] : marche forcée primaire/secondaire avec marcheForceeDone flag
   */
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

      lastReadTime = now();
    }     // end setup

/*_________________________________________TASK CORE 1&2__________________________________________________________*/

  /**
   * @brief Core 0 : gestion TRIAC dimmer, lecture SolarEdge, régulation surplus PV, contrôle PAC, marche forcée
   * 
   * Boucle infinie delay(25ms) :
   * 1. Debounce PIR (candidatePIR->statusPIR si delayNoise>500ms)
   * 2. Check plageMarcheForcee[0/1] (heure/minute) → démarre MF si temperatureEau<tempEauMin
   * 3. Mode normal : getSolarEdgeValues(), calcul currentPower=(PV-LOAD)*1000W,
   *    ajuste valTriac (brightness 0-255), gestion restePuissance
   * 4. Contrôle relay PAC si debutOverPuissance>tempsOverProd et restePuissance>puissPacOn,
   *    stop si >tempsMinPac et <puissPacOff
   * 5. Mode marcheForcee : check finMarcheForcee (volume ou température)
   * 6. gestEcran()+gestWeb()
   * 
   * Variables clés :
   * - valTriac : 0-100% (converti en brightness 0-255 pour DimmableLight)
   * - currentPower : surplus PV disponible (W), positif=production, négatif=consommation
   * - restePuissance : surplus après chauffe-eau, utilisé pour PAC
   * - marcheForcee : true si marche forcée active (TRIAC 100%)
   */
    void Task1code( void * pvParameters ){
        bool doMarcheForcee = false;

      for(;;) {
            /* ------- debounce PIR ------*/
        if(config.motionSensor&&!initPIRphase){                                               // PIR is instaled
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
        if(!marcheForcee){                      // fonctionnement normal
          if( !plageMarcheForcee[0].marcheForceeDone && 
              ((hour()>plageMarcheForcee[0].heureOnMarcheForcee) || 
               (hour()==plageMarcheForcee[0].heureOnMarcheForcee) && (minute()>=plageMarcheForcee[0].minuteOnMarcheForcee)
              )
            ){
              Serial.printf("Will start MF de base, heure:%d, minuteNow:%d, plageHMF:%d, plageMMF:%d\n",hour(),minute(),plageMarcheForcee[0].heureOnMarcheForcee,plageMarcheForcee[0].minuteOnMarcheForcee);
              plageMarcheForcee[0].marcheForceeDone = true;
              if((config.sondeTemp) && (temperatureEau < config.tempEauMin )){             // only do it if needed  
                doMarcheForcee = true;
                finMarcheForcee = now() + tempsChauffe;
              }
          }   
          if(config.secondBackup){
            if( !plageMarcheForcee[1].marcheForceeDone && 
                ((hour()>plageMarcheForcee[1].heureOnMarcheForcee) || 
                (hour()==plageMarcheForcee[1].heureOnMarcheForcee) && (minute()>=plageMarcheForcee[1].minuteOnMarcheForcee)
                )
              ){
                Serial.printf("Will start MF secondaire, heureNow:%d, minuteNow:%d, plageHMF:%d, plageMMF:%d\n",hour(),minute(),plageMarcheForcee[1].heureOnMarcheForcee,plageMarcheForcee[1].minuteOnMarcheForcee);
                plageMarcheForcee[1].marcheForceeDone = true;
                if((config.sondeTemp) && (temperatureEau < config.tempEauMin )){             // only do it if needed  
                  doMarcheForcee = true;
                  finMarcheForcee = now() + tempsChauffe / 2;
                }
            }   
          }

          if (doMarcheForcee){        // marche forcé if any
            doMarcheForcee = false;   // clear flag
            Serial.println("  ==>  Starting Marche forcee\n");
            marcheForcee = true;
            mfChauffeEau = true;
            triac1.turnOn();
            valTriac=100;
            gestEcran(mForcee);
            gestWeb();
          } else {                    // pas heure de marche forcée : normal mode
            time_t maintenant = now();
//            if((maintenant > jour) && (maintenant < nuit)){   // do it only in daylight
              if( getSolarEdgeValues() ) {
                // Calcul surplus = Production PV - Consommation maison (kW → W)
                // Si > 0 : surplus à router vers chauffe-eau
                // Si < 0 : consommation > production → stop TRIAC
                currentPower = (PVCurrentPower - LOADCurrentPower)*1000;
                Serial.printf("\nCurrentPower: %.0fW, PVPower; %.2fkW, LoadPower: %.2fkW\n\n",currentPower,PVCurrentPower,LOADCurrentPower);
                Serial.printf("    debutOverPuissance: %02d:%02d, now: %02d:%02d\n\n",minute(debutOverPuissance),second(debutOverPuissance),minute(),second());
                if ( currentPower >= 0 ){ 		    // Surplus disponible : router vers chauffe-eau
                  // Calcul % TRIAC : currentPower / pasPuissance (pasPuissance = puissanceBallon/100)
                  // Exemple : 2000W surplus, 3000W ballon → valTriac = 2000/(3000/100) = 66%
                  valTriac = currentPower / pasPuissance;
                  Serial.printf(" ValTriac calculated is: %.2f\n",valTriac);
                  if (valTriac > 100) {
                    valTriac = 100;  // Clip à 100% max
                    // Calcul restePuissance pour PAC (surplus après chauffe-eau 100%)
                    if(triac1.getBrightness()==255){    // TRIAC déjà à fond
                      restePuissance = currentPower;      // Reste = surplus total (modulo résistance)
                    } else {
                      restePuissance = currentPower-config.puissanceBallon ; // 1ère fois à 100%
                    }
                    if(debutOverPuissance == 0) debutOverPuissance = now();   // Mémoriser début surplus
                  } else {
                    restePuissance = 0.0;  // Pas assez pour PAC
                    debutOverPuissance = 0;      
                  }
                  if( (int)valTriac == 0){
                    mfChauffeEau = false;
                    triac1.turnOff();
                    Serial.printf(" ValTriac = 0 stop triac\n");
                  } else {
                    mfChauffeEau = true;
                    unsigned int brightness = (int)(255*valTriac/100);
                    unsigned int ballonW = (int)((valTriac/100)*config.puissanceBallon);
                    Serial.printf(" ValTriac : %.2f, brightness : %d%, ballonW : %d, restePuissance: %.2f\n",valTriac,brightness,ballonW,restePuissance);
                    triac1.setBrightness(brightness);
                  }
                } else {    // currentPower < 0 puissanse consommée sur EDF : arret du triac
                  mfChauffeEau = false;
                  restePuissance = 0.0;
                  debutOverPuissance = 0;     
                  triac1.turnOff();
                  valTriac = 0;
                  Serial.printf(" CurrentPower est negatif valTriac = 0 et triac off\n\n");
                }
                gestEcran(values);
                gestWeb();
                // Gestion relay PAC (hystérésis temporelle pour éviter cycles start/stop)
                // ON : si restePuissance > puissPacOn pendant tempsOverProd secondes (ex: 15min)
                // OFF : si restePuissance < puissPacOff après tempsMinPac min (ex: 30min)
                if(config.pacPresent){
                  if(debutRelayPAC == 0) {   // PAC éteinte
                    Serial.printf("\n==> Test si start pac: debutOverPuissance:%02d:%02d:%02d, , now:%02d:%02d:%02d, delta(now - debutOP)(sec):%d, temps OverP: %i\n\n",hour(debutOverPuissance),minute(debutOverPuissance),second(debutOverPuissance),hour(),minute(),second(),now()-debutOverPuissance,config.tempsOverProd);
                    // Conditions démarrage PAC : surplus stable (restePuissance ≥ puissPacOn) pendant tempsOverProd
                    if ( (restePuissance >= config.puissPacOn ) && (debutOverPuissance != 0) && ((now() - debutOverPuissance) > config.tempsOverProd ) ){
                      Serial.printf("\n ==> Start PAC at %2d:%2d:%2d\n",hour(),minute(),second());
                      mfPAC = true;
                      debutRelayPAC = now();
                      setRelayPac(HIGH);
                    }
                  } else {  // PAC allumée
                    Serial.printf("\n==>Test si stop PAC delta(now-debutRelayPac)(s):%d, tempsMinPac:%i, restPuissance:%.2f\n\n",now()-debutRelayPAC,config.tempsMinPac, restePuissance);
                    // Condition arrêt PAC : si restePuissance < puissPacOff ET PAC tourne depuis > tempsMinPac
                    // (évite arrêt prématuré : PAC doit tourner au moins tempsMinPac secondes)
                    if( (!modeManuPAC) && (now()-debutRelayPAC) > config.tempsMinPac ){
                      if(restePuissance < config.puissPacOff){
                        Serial.printf("\n ==> Stop PAC at %2d:%2d:%2d\n",hour(),minute(),second());
                        mfPAC = false;
                        debutRelayPAC = 0;
                        setRelayPac(LOW);
                      } else {
                        Serial.println("Reste puissance > puissPACOff, don't stop PAC");
                      }
                    }
                  }
                }
              }		// end if getSolarEdgeValues()
//            }   // end between jour and nuit
          }
        } else {                      // marcheForcee = true cancel when done
          if(!(modeManuEau || modeManuPAC)){    // only if not in manual mode
            if (!config.sondeTemp){             // marche forcee volume  
              if( (now()-finMarcheForcee) >= 0 ){
                Serial.println("  ==> FIN marche forcee");
                triac1.turnOff();
                mfChauffeEau = false;
                valTriac=0;
                marcheForcee = false;
                gestEcran(full);
                gestWeb();
              }
            } else {                              // MarcheForcee temperature
              if( (temperatureEau >= config.tempEauMin ) || ( (now()-finMarcheForcee) >= 0 )){  // stop marchforcee
                Serial.println("  ==>  FIN marche forcee");
                triac1.turnOff();
                mfChauffeEau = false;
                valTriac=0;
                marcheForcee = false;
                gestEcran(full);
                gestWeb();
              }
            }
          } else {    // in manual mode
            if( getSolarEdgeValues() ) {
              gestEcran(modeManuValues);
              gestWeb();
            }
          }
        }			// end if marcheForcee
        delay(25);
      }				// end for ever
    }

  /**
   * @brief Core 1 : gestion écran OLED, PIR calibration, WiFi reconnect, NTP updates, timers secondes/minutes/heures
   * 
   * Boucle infinie delay(25ms) :
   * 1. Init écran si WiFi+initEcran
   * 2. Gestion motion PIR->allume OLED
   * 3. Timeout écran (timeoutEcran 5min)
   * 4. Check buttons (buttonChauffeEau)
   * 5. Timer secondes : PIR calibration 60s, WiFi reconnect, gestEcran(horloge),
   *    NTP retry si !ntpOK, appel getSolarEdgeInfos() toutes les solarEdgeGetInfos secondes
   * 6. Timer minutes : ds18b20.requestTemperatures(), NTP retry (ntpWait 60min),
   *    refresh écran 3min, switch solarEdgeGetInfos jour/nuit
   * 7. Timer heures : getNTPTime(), calculDureeJour(), reset plageMarcheForcee[]
   * 
   * Variables clés :
   * - nbSecondes/nbMinutes/nbHeures : timers incrémentaux pour actions périodiques
   * - solarEdgeGetInfos : 60s (jour) ou 1800s (nuit) entre appels API SolarEdge
   * - initPIRphase : calibration 60s après boot avant attachInterrupt()
   * - oled : true si écran allumé, false si éteint (power save)
   */
    void Task2code( void * pvParameters ){
      for(;;) {
        if(config.afficheur){                          // if ecran gestion
          /* ------ initialisation ecran ------ */
          if(initEcran){
            if(WiFi.status() == WL_CONNECTED){
              gestEcran(entete);
              (marcheForcee) ? gestEcran(mForcee) : gestEcran(full);
              Serial.println("===>>   calling solarEdge");
              getSolarEdgeInfos();
              nbSecondes = 0;
              initEcran = false;
            }
          }
          /* ------ Gestion ecran selon PIR ------ */
          if(config.motionSensor&&!initPIRphase){         // PIR is instaled
            if(motion){             // going from low to high : motion detected !
              if(!oled) {           // do stuff only if screen is off
                Serial.println("MOTION DETECTED!!!");
                u8g2.setPowerSave(0); // turn display on
                oled = true;
                gestEcran(typeUpdateEcran);
              }
              lastReadTime = now();
              motion = false;
            } 
          }
          /* ------ Gestion timeout ecran ------ */
          if( (lastReadTime != 0) && ((now() - lastReadTime) > timeoutEcran)) {   // motion is still LOW check to Turn off the OLED after the number of seconds defined in the timeSeconds variable
              Serial.println("   !!!  TimoutEcran ... Ecran OFF !!!");
              stopEcran();      // turn display off and clear buffer
              oled = false;
              lastReadTime = 0;
          }
        }
                /* ------- gestion buttons ------*/
        buttonChauffeEau.check();
  
        if(flgSetManuEau){
          marcheForceeSwitch(!modeManuEau);
          flgSetManuEau = false;
        } 
    
  
  /* ------ TIMER toutes les secondes ------ */
        if(second() != previousSecond){
//          Serial.printf("Second timer: %2d:%2d:%2d\n",hour(),minute(),second()); 
                 /* ------ Check PIR calibration ------*/
          if(config.motionSensor && initPIRphase && (nbSecondesPIR++>60)){
              Serial.println("Sensor is now calibrated");
              initPIRphase = false;
              attachInterrupt(pinMotionSensor, detectsMovementRising, RISING);
  //            attachInterrupt(pinMotionSensor, detectsMovementFalling, FALLING);
            }       

                /* ------ Check WIFI ------*/
          if(WiFi.status() != WL_CONNECTED){
            if(startWiFi()) {
              routeurWeb.startup();
              getSolarEdgeInfos();
            }
          }

          /* ------ update ecran (horloge)------*/
          if (config.afficheur && oled){
              gestEcran(horloge);    
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
                  calculDureeJour(month()-1); 
                  gestEcran(horloge);
                  lastReadTime = now();
                }
              }
            }
          }
            /* ------ ask for new routeurSol values if norm al or mode manu ------*/
          if(!marcheForcee || modeManuEau || modeManuPAC){                                // innutile en marche forcee else sortie de Mforcee gerée next minute 
            if(nbSecondes++ > solarEdgeGetInfos) {
              Serial.println("===>>   calling solaredge");
              getSolarEdgeInfos();
              nbSecondes = 0;
            }
          }
              /* ----- reset previous seconds ----*/
          previousSecond = second();
        }     // end timer second

  /* ------ TIMER toutes les minutes ------ */
        if(minute() != previousMinute){
                /* ------ Debug show now and plage Marche Forcee status  ------*/
          Serial.printf("\nNew Minute : jour is %02d:%02d, nuit is %02d:%02d, NOW is %02d:%02d:%02d\n",hour(jour),minute(jour),hour(nuit),minute(nuit),hour(),minute(),second());
          Serial.printf("Next Solar Edge call in %dsec\n",solarEdgeGetInfos-nbSecondes);
          Serial.printf("\nheureOnMarcheForcee: %02d:%02dmn, marcheForceeDone: %s\n",plageMarcheForcee[0].heureOnMarcheForcee,plageMarcheForcee[0].minuteOnMarcheForcee,(plageMarcheForcee[0].marcheForceeDone)?"true":"false");   
          Serial.printf("SecondMarcheForce: %s, heureOnMarcheSecForcee: %02d:%02d, marcheForceeSecDone: %s\n\n",(config.secondBackup)?"true":"false",plageMarcheForcee[1].heureOnMarcheForcee,plageMarcheForcee[1].minuteOnMarcheForcee,(plageMarcheForcee[1].marcheForceeDone)?"true":"false");   
          if(previousMinute != -1){
                /* ------ Check temperature si sonde ------*/
            if(config.sondeTemp){                  // should be true if mForceeTemp
              float temperature_brute = -127;
              ds18b20.requestTemperatures();                             // demande de température au capteur //
              temperature_brute = ds18b20.getTempCByIndex(0);            // température en degrés Celcius
              if (temperature_brute < -20 || temperature_brute > 130) {  //Invalide. Pas de capteur ou parfois mauvaise réponse
                Serial.print("Mesure Température invalide ");
              } else {
                if(temperatureEau != temperature_brute){
                  temperatureEau = temperature_brute;
                  if(marcheForcee){
                    gestEcran(mfValues);
                    gestWeb();
                  }
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
                /* ------ Refresh ecran every 3 mn ------*/
            if(config.afficheur && oled && (nbMinutesEcran++ > 3)){
              Serial.println("Refresh ecran !");
              gestEcran(typeUpdateEcran);
              nbMinutesEcran = 0;
            }
              /* ------ out of mode marche forcee ------*/
            if(marcheForcee){                                
//            if(now() > finMarcheForcee){
              Serial.println("===>>   calling routeurSol");
              getSolarEdgeInfos();
//            }
            }
              /* ------ Gestion  solarEdgeGetInfos selon nuit et jour ------*/
            time_t maintenant = now();
            if((maintenant > jour) && (maintenant < nuit)){   // We are in daylight
              if(solarEdgeGetInfos != solarEdgeGetInfosJour){
                Serial.printf("Going to Jour (%d) getSolarEdgeInfo settings\n",solarEdgeGetInfosJour);
                solarEdgeGetInfos = solarEdgeGetInfosJour;
                getSolarEdgeInfos();                             // get values before change
              }
            } else {
              if(solarEdgeGetInfos != solarEdgeGetInfosNuit){
                Serial.printf("Going to Nuit (%d) getSolarEdgeInfo settings\n",solarEdgeGetInfosNuit);
                solarEdgeGetInfos = solarEdgeGetInfosNuit;
                getSolarEdgeInfos();                             // get values before change
              }
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
                calculDureeJour(month()-1); 
                gestEcran(horloge);
                lastReadTime = now();
              }
            }
          } 
          previousHour = hour();
              // change day !
          if((lastday !=-1) && (day() != lastday)){
            plageMarcheForcee[0].marcheForceeDone = false;   
            plageMarcheForcee[1].marcheForceeDone = false;   
            lastday = day();
            calculDureeJour(month()-1);   // change jour et nuit chaque jour
          }
        }
        delay(25);
      }				// end for ever
    }


/*__________________________________________LOOP__________________________________________________________*/

    void loop() {
      // put your main code here, to run repeatedly:
    }

/*__________________________________________TIME_FUNCTIONS______________________________________________*/

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
      lastday = day();
    }

/*__________________________________________ROUTEUR_FUNCTIONS______________________________________________*/

    void marcheForceeSwitch(boolean value){
      Serial.printf("Marche Forcée Chauffe Eau Switch: val=%s, TriacOn=%s\n",(value)?"true":"false",(valTriac != 0.0)?"true":"false");
      if((valTriac != 0.0) && !value){       // user asked to switch off
        Serial.println("\n ==> Stop Chauffe Eau en Mode Manuel");
        triac1.turnOff();
        valTriac=0;
        finMarcheForcee = now();
        mfChauffeEau = false;
        marcheForcee = false;
        modeManuEau = false;
        gestEcran(full);
        gestWeb();
//      } else if((valTriac == 0.0) && value) {  // user asked to force marche forcée
      } else if(value) {        // user asked to force marche forcée
        Serial.println(" ==> Start Chauffe Eau en Mode Manuel");
        mfChauffeEau = true;
        marcheForcee = true;
        modeManuEau = true;
        triac1.turnOn();
        valTriac=100;
        gestEcran(modeManu);
        gestWeb();
      } else {                              // nothing to do already in marcheForcee or switch already off
        Serial.println("Chauffe Eau Switch : nothing to do already running or switch already off");
      }
    }

    void marcheForceePACSwitch(boolean value){
      if(config.pacPresent) {
        if( (debutRelayPAC != 0) && !value) {   // asked to stop PAC
          Serial.printf("\n ==> Stop PAC en Mode Manuel at %2d:%2d:%2d\n",hour(),minute(),second());
          modeManuPAC = false;
          mfPAC = false;
          debutRelayPAC = 0;
          setRelayPac(LOW);
          gestEcran(full);
          gestWeb();
        } else if( (debutRelayPAC == 0) && value ){  // asked to start PAC
          Serial.println(" ==> Start PAC  en Mode Manuel");
          modeManuPAC = true;
          mfPAC = true;
          debutRelayPAC = now();
          setRelayPac(HIGH);
          gestEcran(modeManu);
          gestWeb();
        }
      }
    }

    void setRelayPac(uint8_t state) {
      if(config.pacPresent) {
        digitalWrite(RelayPAC, state);
      }
    }

    void handleButtonEvent(AceButton* button, uint8_t eventType, uint8_t buttonState){
      // Print out a message for all events, for both buttons.
      static bool wakedUp = false;
    Serial.printf("handleEvent(): pin: %d; eventType: %s; buttonState: %s\n",button->getPin(),(const char*)AceButton::eventName(eventType),(button->isReleased(buttonState))?"Low":"High");

      // do stuff
    switch (eventType) {
      case AceButton::kEventPressed:
        if (config.afficheur && !oled) {    // if screen is off then ask to turn it back on
          u8g2.setPowerSave(0); // turn display on
          oled = true;
          gestEcran(typeUpdateEcran);
          wakedUp = true;
        }
      break;
      case AceButton::kEventReleased:
        if(wakedUp){                      // waked up display so don't do nothing when clicked
          wakedUp = false;
          lastReadTime = now();
        } else {                          // normal click 
          switch(button->getId()){
            case BUTTON_ChauffeEau_ID :
                flgSetManuEau = true;
            break;
          }   // end inner switch for kEventClicked              
        }
      break;
    }
  }

    void changePIRmode(bool val){     // called from web setup params
      if(val){                        // change from false to true => set up after calibration 
        initPIRphase = true;
      } else {                        // change from true to false => detach interupt
        detachInterrupt(pinMotionSensor);
      }
    }

    /*__________________________________________Feed Back ecran or web FUNCTIONS______________________________________________*/

    void gestEcran(actionEcranWeb action){
        char heure[20];
        char actionStr[][12] = {"none", "entete", "full", "horloge", "wifi", "values", "mForcee", "mfValues", "modeManu", "httpError"};
        bool updateEcran =false;
        char rounded[5];   

      if(action != horloge) {
        Serial.printf("   gestEcran with action=%s\n",actionStr[action]);      
      }
            // do it in good order
      if((action == mfValues) && (typeUpdateEcran!=mForcee)) 
        action = mForcee;
      if((action==values) && (typeUpdateEcran!=full))
        action = full;
      if((action==modeManuValues) && (typeUpdateEcran!=modeManu))
        action = modeManu;
          
            // store action for oled wake up if needed
      switch(action){             
        case values:
        case full:
            typeUpdateEcran = full;       
          break;
        case mfValues:
        case mForcee:
            typeUpdateEcran = mForcee;     
          break;
        case modeManuValues:
        case modeManu:
            typeUpdateEcran = modeManu;
          break;
      }
                // out of httpError redraw
      if(httpErreur && (action != horloge)) {
        httpErreur = false;
        if(action == values) action = full;       // redraw all if previous http error
        if(action == mfValues) action = mForcee;  // redraw all if previous http error
        if(action == modeManuValues) action = modeManu;
      }
            /* ---- entete ---- */
      if(action == entete){
        Serial.println("    ==> doing entete");
        u8g2.clearBuffer(); // on efface ce qui se trouve déjà dans le buffer
        u8g2.sendBuffer();
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
        u8g2.print("v1   Web:");         
        u8g2.print(byte4);               // affichage dernier byte adresse ip //
        updateEcran = true;              // l'image qu'on vient de construire est affichée à l'écran
        delay(500);
      }
            /* ---- wifi sign icon ---- */
      if( (WiFi.status() == WL_CONNECTED) && (OldWifiStatus == WL_DISCONNECTED) ){
        u8g2.setDrawColor(0);
        u8g2.drawBox(118, 0, 8, 8);   // erase manu symbol
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2.drawGlyph(118, 8, 0x51);   // wifi symbol
        OldWifiStatus = WL_CONNECTED;
        updateEcran = true;             
      } 
      if( (WiFi.status() != WL_CONNECTED) && (OldWifiStatus == WL_CONNECTED) ){
        u8g2.setDrawColor(0);
        u8g2.drawBox(118, 0,8,8);   // erase wifi symbol
        u8g2.setDrawColor(1);
        OldWifiStatus = WL_DISCONNECTED;
        updateEcran = true;             
      }
            /* ---- chauffe eau icon ---- */
      if(mfChauffeEau && !oldChauffeEau){
        u8g2.setFont(u8g2_font_open_iconic_thing_2x_t);
        u8g2.drawGlyph(97, 15, 0x48);   // chauffeEau symbol
        oldChauffeEau = mfChauffeEau;
        updateEcran = true;             
      } 
      if(!mfChauffeEau && oldChauffeEau){
        u8g2.setDrawColor(0);
        u8g2.drawBox(97, 0, 16, 15);   // erase chauffEau symbol
        u8g2.setDrawColor(1);
        oldChauffeEau = mfChauffeEau;
        updateEcran = true;             
      }
            /* ---- PAC icon ---- */
      if(mfPAC && !oldPAC){
        u8g2.setFont(u8g2_font_open_iconic_app_2x_t);
        u8g2.drawGlyph(76, 15, 0x40);   // pac symbol
        oldPAC = mfPAC;
        updateEcran = true;             
      } 
      if(!mfPAC && oldPAC){
        u8g2.setDrawColor(0);
        u8g2.drawBox(76, 0, 16, 15);   // erase pac symbol
        u8g2.setDrawColor(1);
        oldPAC = mfPAC;
        updateEcran = true;             
      }
                // espnow icon
      /*      if((action == espnow) || (action == full) || (action == mForcee)){
        u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
        if(routeurEspnow.hasManager()){
          u8g2.drawGlyph(118, 18, 0x46);   // espnow symbol (courrant)
        } else {
          u8g2.setDrawColor(0);
          u8g2.drawBox(118, 10, 8, 8);   // erase espnow symbol
          u8g2.setDrawColor(1);
        }
      }
      */
            
            /* ---- normal mode ---- */
      if(action == full){
        Serial.println("    ==> doing full");
                // erase drawing zone
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 16, 128,48);   // erase zone x,y haut gauche,longueur,hauteur
        u8g2.setDrawColor(1);
                // Frame
        u8g2.drawRFrame(25,17,50,17,4); 	  // rectangle x et y haut gauche / longueur / hauteur / arrondi //
        updateEcran = true;             
      }
            /* ---- Marche Forcee ---- */
      if(action == mForcee){
        Serial.println("    ==> doing mForcee");
                // erase drawing zone
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 16, 128,48);   // erase zone x,y haut gauche,longueur,hauteur
        u8g2.setDrawColor(1);
                // Frame
        u8g2.drawRFrame(23,19,99,20,5); 	      // rectangle x et y haut gauche / longueur / hauteur / arrondi //
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(28, 33);
        u8g2.print("Marche forcée");
        u8g2.setFont(u8g2_font_streamline_all_t);
        u8g2.drawGlyph(5, 40, 0x00d9);          // radioactiv
        u8g2.setFont(u8g2_font_6x10_tf);
        updateEcran = true;             
      }
            /* ---- Mode Manu ---- */
      if(action == modeManu){
        Serial.println("    ==> doing modeManu");
                // erase drawing zone
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 16, 128,48);   // erase zone x,y haut gauche,longueur,hauteur
        u8g2.setDrawColor(1);
                // Frame
        u8g2.drawRFrame(16,19,85,20,5); 	// rectangle x et y haut gauche / longueur / hauteur / arrondi //
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(21, 33);
        u8g2.print("Mode Manuel");
        u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
        u8g2.drawGlyph(1, 40, 0x48);   // manu symbol (clé)
        u8g2.setFont(u8g2_font_6x10_tf);
        updateEcran = true;             
      }
            /* ---- Marche Forcee ---- */
      if(action == httpError){
        Serial.println("    ==> doing httpError");
        httpErreur = true;
                // erase drawing zone
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 16, 128,48);   // erase zone x,y haut gauche,longueur,hauteur
        u8g2.setDrawColor(1);

        u8g2.drawRFrame(23,19,99,20,5); 	      // rectangle x et y haut gauche / longueur / hauteur / arrondi //
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(28, 33);
        u8g2.print("Erreur HTTP!");
        u8g2.setFont(u8g2_font_streamline_internet_network_t);
        u8g2.drawGlyph(5, 40, 0x0035);                        // download portable
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.setCursor(21, 52);
        u8g2.print("Error Code : ");   
        if(httpResponseCode<0){
          dtostrf(httpResponseCode,3,0,rounded);      // negative to str
          u8g2.print(rounded);
        } else {
          u8g2.print(httpResponseCode);                         //  400 par ex
        }
        u8g2.setCursor(1, 64);
        u8g2.print(http_status_to_string(httpResponseCode));  //  "INTERNAL SERVER ERROR" par ex
        u8g2.setFont(u8g2_font_6x10_tf);

        updateEcran = true;             
      }
                  /* ---- values ---- */
      if((action == values) || (action == full) || (action==modeManu) || (action == modeManuValues)){
        Serial.println("    ==> doing global values");
                // icones edf,maison,pv
        u8g2.setFont(u8g2_font_streamline_interface_essential_wifi_t);
        u8g2.drawGlyph(1, 62, 0x0031);    // EDF (wifi)
        u8g2.setFont(u8g2_font_streamline_building_real_estate_t);
        u8g2.drawGlyph(54, 62, 0x0032);    // Maison
        u8g2.setFont(u8g2_font_streamline_ecology_t);
        u8g2.drawGlyph(109, 62, 0x003E);    // solar plant
                // grid power
      //          u8g2.setFont(u8g2_font_squeezed_b6_tr);     
        u8g2.setFont(u8g2_font_6x10_tf);
        if(GRIDCurrentPower != 0){
          if((int)(GRIDCurrentPower/10) != 0){
            u8g2.setCursor(21, 64);
          } else {
            u8g2.setCursor(25, 64);
          }
          u8g2.print(GRIDCurrentPower,2);   // print edf/maison  
        }
                // PV power
        if(PVCurrentPower != 0){
          if((int)(PVCurrentPower/10) != 0){
            u8g2.setCursor(78, 64);
          } else {
            u8g2.setCursor(85, 64);
          }
          u8g2.print(PVCurrentPower,2);   // print pv:maison
        }
                // fleches 
        u8g2.setFont(u8g2_font_open_iconic_arrow_2x_t);
        if (strcmp(from,"LOAD") == 0) {               
          u8g2.drawGlyph(32, 56, 0x0041);    // over prod donc fleche gauche 
        } else {                              
          u8g2.drawGlyph(30, 56, 0x0042);    // on consomme donc fleche droite
        }
        if(PVCurrentPower != 0){
            u8g2.drawGlyph(84, 56, 0x0041);  // fleche gauche 
        }
        updateEcran = true;             
      }
      if((action == values) || (action == full)){
        Serial.println("    ==> doing Normal values");
            // conso/prod
        u8g2.setFont(u8g2_font_7x13B_mn);
        if((int)(LOADCurrentPower/10) != 0){
          u8g2.setCursor(35,30);
        } else {
          u8g2.setCursor(44,30);
        }
        u8g2.print(LOADCurrentPower,2);       // print dans le cadre
                
            // Face happy/sad
        if (strcmp(from,"LOAD") == 0) {
          if(GRIDCurrentPower > 1.5F){            // over production
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
        updateEcran = true;             
      }
      if((action==modeManu) || (action == modeManuValues)){
        Serial.println("    ==> doing Mode Manu values");
        // conso/prod
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(105,33);
        dtostrf(LOADCurrentPower,3,1,rounded);      // to 1 decimal
        u8g2.print(rounded);       
        u8g2.print(' ');       
        updateEcran = true;             
      }

            /* ---- Marche Forcee values ---- */
      if((action == mfValues) || (action == mForcee)){
        Serial.println("    ==> doing mfValues");
        if ( temperatureEau == -127 ){       // marche Forcee Volume
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.setCursor(30, 64);
          sprintf(heure,"Fin %02d:%02d,%02d", hour(finMarcheForcee),minute(finMarcheForcee),second(finMarcheForcee));
          u8g2.print(heure);
        } else {                             // marche Forcee Temperature
          u8g2.setFont(u8g2_font_6x10_tf);
          u8g2.setCursor(68, 52);
          sprintf(heure,"Fin %02d:%02d", hour(finMarcheForcee),minute(finMarcheForcee));
          u8g2.print(heure);
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.setCursor(8, 64);
          u8g2.print("Temp Eau: ");
          dtostrf(temperatureEau,4,1,rounded);      // to 1 decimal
          u8g2.print(rounded);       
          u8g2.print("°C");
        }
        updateEcran = true;             
      }
            /* ---- horloge ---- */
      if(action == horloge){
        if(!httpErreur){
          u8g2.setFont(u8g2_font_6x10_tf);
          if(!(modeManuEau || modeManuPAC)){
            if(marcheForcee) {   
              if ( temperatureEau == -127 )       // marche Forcee Volume
                u8g2.setCursor(41, 52);
              else
                u8g2.setCursor(11, 52);
            } else {
              u8g2.setCursor(81,29);
            }
          }
          sprintf(heure,"%02d:%02d:%02d",hour(),minute(),second());
          u8g2.print(heure);
          updateEcran = true;
        }             
      }
            /* ---- Affichage Ecran ---- */
      if(updateEcran){             
        u8g2.sendBuffer();  // l'image qu'on vient de construire est affichée à l'écran
      }  
    }

    void stopEcran(){
      u8g2.setPowerSave(1);
      oled = false;
    }

    void gestWeb(){
            // update main web site
          routeurWeb.OnUpdate();        
        }



