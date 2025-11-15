#include "globalRouteur.h"

//#include <FS.h>                 //this needs to be first, or it all crashes and burns...
#include <LittleFS.h>
#include <TimeLib.h>
#include <EEPROM.h>   
#include <SPI.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <SDFS.h>
#else
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <SD.h>
#endif
#include <ESPAsyncWebServer.h>
#include <set>
#include <ArduinoJson.h> 

    
class RouteurSolWebClass {
    public :

        ~RouteurSolWebClass(void);
        RouteurSolWebClass();

        void startup();
        void OnUpdate();

        // --- update status FUNCTIONS ---
           
    private :

        bool serverSettedUp = false;
        const char* mdnsName = "routeurSol"; 			// Domain name for the mDNS responder

        typedef struct sessions {
            char sessID[16]="\0";
            time_t ttl;
            time_t timecreated;
        } actSessions;
        
        actSessions activeSessions[10];


        // --- SETUP_FUNCTIONS ---
        void startMDNS();                      // Start the mDNS responder
        void startServer();                    // Start a HTTP server with a file read handler and an upload handler

        // --- AUTHENTIFY_FUNCTIONS ---
        void printActiveSessions(char *sessID);
        bool isSessionValid(char *sessID);

        // --- SERVER HANDLERS ---
        void handleRoot(AsyncWebServerRequest *request);                        // When URI / is requested, send a login web page
        void handleOtherFiles(AsyncWebServerRequest *request);
        void handleNotFound(AsyncWebServerRequest *request); 	                // if the requested file or page doesn't exist, return a 404 not found error
        void handleNotFound2(AsyncWebServerRequest *request); 	                // if the requested file or page doesn't exist, return a 404 not found error

        // -------- User management ------------
        void handleLogin(AsyncWebServerRequest *request);                       // If a POST request is made to URI /login
        void handleRegister(AsyncWebServerRequest *request); 					// If a POST request is made to URI /register
        void handleChangAdminPW(AsyncWebServerRequest *request);
        void handleGetUsers(AsyncWebServerRequest *request);
        void handleDeleteUsers(AsyncWebServerRequest *request);

        // --- PAGES HANDLERS ---
        bool handleFileRead(String path, AsyncWebServerRequest *request);
        bool handleFileError(String path, AsyncWebServerRequest *request);     // send file not found to the client
        void handleFileList( AsyncWebServerRequest *request);
        bool checkSessionParam(AsyncWebServerRequest *request);

        // --- ROUTEUR_HANDLERS ---
        void handleSetRouteurSSEData(AsyncWebServerRequest *request); // /setHomeSSEData
        void handleSetSwitches(AsyncWebServerRequest *request);       // /setSwitches?sess=sessID & switch=[pacSwitch|chauffeEauSwitch] & value=[1:true|0:false]
        void handleSetParamsSSEData(AsyncWebServerRequest *request);  // /setParamsSSEData
        void handleSetParams(AsyncWebServerRequest *request);         // /setRouteurParams?sess=sessID & heureMarcheForce=20:30 & marcheForceSuppSW=[1:true|0:false] & heureMarcheForceSec=20:30 & 
                                                                      //                   sondeTempSW=[1:true|0:false] & tempEauMin=60 & pacPresentSW=[1:true|0:false] & puissPacOn=1000 & 
                                                                      //                   puissPacOff=800 & tempsOverProd=60 & tempsPacMin=120 & afficheurSW=[1:true|0:false] & 
                                                                      //                   motionSensorSW=[1:true|0:false] & volBallon=150 & puissanceBallon=1500
    
        
        // SSE Updates  
        void updateRouteurData();                                                 // update page web pricipale
        void updateRouteurParamsData();                                           // update page web parametres

        // --- REST_HANDLERS ---

        void handleChangePassword(AsyncWebServerRequest *request);   

        void handleGetRouteurStatus(AsyncWebServerRequest *request); 
        void handleSetRouteurSwitch(AsyncWebServerRequest *request); 

        void showJsonConfig(AsyncWebServerRequest *request);

        // --- HELPER_FUNCTIONS ---

        String getContentType(String filename);
        String formatBytes(size_t bytes);
        bool generateKey(char *sessID,long ttl);
        void printActiveSessions();




};

