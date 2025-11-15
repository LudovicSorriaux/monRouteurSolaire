/*******************************************************************************
 * @file    routeurWeb.cpp
 * @brief   Implémentation classe RouteurSolWebClass - Serveur web
 * @details Gestion AsyncWebServer, routes API (/api/status, /api/command, etc.),
 *          SSE (Server-Sent Events), authentification sessions, interface
 *          contrôle dimmer TRIAC et paramètres routeur.
 * 
 * @author  Ludovic Sorriaux
 * @date    2024
 *******************************************************************************/

#include "routeurWeb.h"

    RouteurSolWebClass::~RouteurSolWebClass(void)
      {};

  /*
   * RouteurSolWebClass::RouteurSolWebClass
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    RouteurSolWebClass::RouteurSolWebClass(){
        Serial.println(F("init monRouteurWeb"));
    }


// PUBLIC functions 

  /*
   * void RouteurSolWebClass::startup
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::startup(){
      Serial.println("monRouteurWeb Startup ... ");
      if(!serverSettedUp){
        startServer();               // Start a HTTP server with a file read handler and an upload handler
      } else {
        Serial.println("Wifi back again but server on bad socket/port need to restart");
/*
        server.reset();
        startServer();
        server.end();
        server.begin();
*/
      }
      startMDNS();                 // Start the mDNS responder
    }

  /*
   * void RouteurSolWebClass::OnUpdate
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::OnUpdate(){
        #if defined(ESP8266)
            MDNS.update();
        #endif
        updateRouteurData();
        updateRouteurParamsData();
    }

/*__________________________________________________________  update status FUNCTIONS __________________________________________________________*/


// PRIVATE functions 

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

  /*
   * void RouteurSolWebClass::startMDNS
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::startMDNS() { // Start the mDNS responder
      #if defined(ESP8266)
        if (!MDNS.begin(mdnsName,WiFi.localIP())) {
      #else
        if (!MDNS.begin(mdnsName)) {
      #endif
            Serial.println(F("Error setting up MDNS responder!"));
        } else {
            Serial.println(F("mDNS responder started"));
            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);
            Serial.printf("mDNS responder started: http://%s.local\n",mdnsName);
        }
    }

  /*
   * void RouteurSolWebClass::startServer
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::startServer() { // Start a HTTP server with a file read handler and an upload handler

        server.on("/", HTTP_ANY, std::bind(&RouteurSolWebClass::handleRoot, this, std::placeholders::_1));                         // Call the 'handleRoot' function when a client requests URI "/"                         // Call the 'handleRoot' function when a client requests URI "/"
                    // -------- call backs for debug ------------
        server.on("/jsonConfig", HTTP_ANY, std::bind(&RouteurSolWebClass::showJsonConfig, this, std::placeholders::_1));
        
                    // -------- call backs from javascripts ------------
        server.on("/logon", HTTP_POST, std::bind(&RouteurSolWebClass::handleLogin, this, std::placeholders::_1)); 			  
        server.on("/register", HTTP_POST, std::bind(&RouteurSolWebClass::handleRegister, this, std::placeholders::_1)); 	
        server.on("/adminPasswd", HTTP_POST, std::bind(&RouteurSolWebClass::handleChangAdminPW, this, std::placeholders::_1)); 	
        server.on("/getUsers", HTTP_POST, std::bind(&RouteurSolWebClass::handleGetUsers, this, std::placeholders::_1));  
        server.on("/deleteUsers", HTTP_POST, std::bind(&RouteurSolWebClass::handleDeleteUsers, this, std::placeholders::_1));  

            /* -------- call backs from restapi ------------*/

        server.on("/setpw", HTTP_GET, std::bind(&RouteurSolWebClass::handleChangePassword, this, std::placeholders::_1)); 			  
        server.on("/getrouteurstatus", HTTP_GET, std::bind(&RouteurSolWebClass::handleGetRouteurStatus, this, std::placeholders::_1)); 			
        server.on("/setrouteurswitch", HTTP_GET, std::bind(&RouteurSolWebClass::handleSetRouteurSwitch, this, std::placeholders::_1)); 			
        
        // ---------- Routeur ------
        server.on("/setHomeSSEData", HTTP_POST, std::bind(&RouteurSolWebClass::handleSetRouteurSSEData, this, std::placeholders::_1)); 
        server.on("/setSwitches", HTTP_POST, std::bind(&RouteurSolWebClass::handleSetSwitches, this, std::placeholders::_1));      
        server.on("/setParamsSSEData", HTTP_POST, std::bind(&RouteurSolWebClass::handleSetParamsSSEData, this, std::placeholders::_1)); 
        server.on("/setRouteurParams", HTTP_POST, std::bind(&RouteurSolWebClass::handleSetParams, this, std::placeholders::_1));      

        
          // -------- Routeur SSE event management ------------
        routeurEvents.onConnect([this](AsyncEventSourceClient *client){       
          if(client->lastId()){
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
          }
          client->send("hello! routeurEvents Ready", NULL, millis(), 10000);  // send message "hello!", id current millis and set reconnect delay to 1 second
        });

        routeurParamsEvents.onConnect([this](AsyncEventSourceClient *client){
          if(client->lastId()){
              Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
          }
          client->send("hello! routeurParamsEvents Ready", NULL, millis(), 10000);
        });

          // -------- call statics files not html ------------
        server.serveStatic("/", LittleFS, "/");
        server.serveStatic("/img", LittleFS, "/img/");
        server.serveStatic("/icons", LittleFS, "/icons/");
        server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");
        server.serveStatic("/manifest.json", LittleFS, "/manifest.json");
/*
        server.serveStatic("/css", LittleFS, "/css/");
        server.serveStatic("/js", LittleFS, "/js/");
*/
        server.onNotFound(std::bind(&RouteurSolWebClass::handleOtherFiles, this, std::placeholders::_1));           			  // When a client requests an unknown URI (i.e. something other than "/"), call function handleNotFound"

        server.addHandler(&routeurEvents);
        server.addHandler(&routeurParamsEvents);

        serverSettedUp = true;
        server.begin();                             			  // start the HTTP server
        Serial.println(F("HTTP server started, IP address: "));
        Serial.println(WiFi.localIP());

    }

/*__________________________________________________________AUTHENTIFY_FUNCTIONS__________________________________________________________*/

  /*
   * void RouteurSolWebClass::printActiveSessions
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::printActiveSessions(char *sessID){   // to help debug
        Serial.printf("Dump of Active session tab, now is: %lld\n",now());
        for (uint8_t i=0; i<10;i++){
            Serial.printf("sessionID: %s, ttl: %lld, timecreated: %lld\n", activeSessions[i].sessID,activeSessions[i].ttl,activeSessions[i].timecreated);
            if(activeSessions[i].timecreated == 0) break; // no more to show
        }  
    }

  /*
   * bool RouteurSolWebClass::isSessionValid
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    bool RouteurSolWebClass::isSessionValid(char *sessID){
        uint8_t i = 0;
        bool flagOK=false;

        // first manage activeSessions struct
        for (i=0; i<10;i++){
            if(activeSessions[i].timecreated+activeSessions[i].ttl < now() ){ // time exhasted : delete sessionID
            activeSessions[i].sessID[0]=0;
            activeSessions[i].ttl=0;
            activeSessions[i].timecreated=0;
            }
        }
        printActiveSessions(sessID);
        Serial.printf("Looking for session: %s\n",sessID);
        // search for sessID
        for (i=0; i<10;i++){
            if(activeSessions[i].ttl == 0) continue;  // found an empty slot
            if(strcmp(activeSessions[i].sessID, sessID) == 0){ 
            Serial.printf("Found right session, time to live is :%lld\n",now() - (activeSessions[i].timecreated+activeSessions[i].ttl));
            flagOK = true;
            break; 
            }
        }
        return flagOK;   
    }

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

  /*
   * void RouteurSolWebClass::handleRoot
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleRoot(AsyncWebServerRequest *request) {                         // When URI / is requested, send a login web page
        Serial.println(F("Enter handleRoot"));
        if(!handleFileRead("/index.html",request)){
            handleFileError("/index.html",request);                 // file not found
        } 
    }

  /*
   * void RouteurSolWebClass::handleOtherFiles
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleOtherFiles(AsyncWebServerRequest *request){ 	// if the requested file or page doesn't exist, return a 404 not found error
        Serial.println(F("Enter handleOtherFiles"));
        Serial.printf(" http://%s %s\n", request->host().c_str(), request->url().c_str());
        if(!handleFileRead(request->url(),request)){
            handleFileError(request->url(),request);                 // file not found
        } 
    }

  /*
   * void RouteurSolWebClass::handleNotFound
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleNotFound(AsyncWebServerRequest *request){ 	// if the requested file or page doesn't exist, return a 404 not found error
        Serial.println(F("Enter handleNotFound"));

        request->send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
        Serial.print(F("NOT_FOUND: "));
        switch (request->method()) { 
            case HTTP_GET: Serial.print(F("GET"));
            break;    
            case HTTP_POST: Serial.print(F("POST"));
            break;    
            case HTTP_DELETE : Serial.print(F("DELETE"));
            break;    
            case HTTP_PUT : Serial.print(F("PUT"));
            break;    
            default : Serial.print(F("UNKNOWN"));
        }
        Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

        if(request->contentLength()){
            Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
            Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
        }

        int headers = request->headers();
        int i;
        for(i=0;i<headers;i++){
            const AsyncWebHeader* h = request->getHeader(i);
            Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for(i=0;i<params;i++){
            const AsyncWebParameter* p = request->getParam(i);
            if(p->isFile()){
            Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
            } else if(p->isPost()){
            Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            } else {
            Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
    }

  /*
   * void RouteurSolWebClass::handleNotFound2
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleNotFound2(AsyncWebServerRequest *request){ 	// if the requested file or page doesn't exist, return a 404 not found error
    String path = request->url();
    //  bool authentified = false;

    Serial.println(F("Enter handleNotFound"));
    /*  authentified = is_authentifiedByCookie(request);
    if(!authentified){    // if not by cookie, test by parameter
        authentified = is_authentifiedByParameter(request);
    }

    if (!authentified){                     // not authentified then serve login.html from the SPIFFS  
        if (!handleFileRead("/login.html",request)){              // if found then sent by the function      
        handleFileError("/login.html",request);                 // file not found
        }
    }  else {                                           // authentified then serve it from the SPIFFS if exists
        if (!handleFileRead(path,request)){                             // if found then sent by the function
        handleFileError(path, request);                                  // file not found 
        } 
    }
    */
        if (!handleFileRead(path,request)){                             // if found then sent by the function
        handleFileError(path, request);                               // file not found 
        } 

    }

/*__________________________________________________________USER_MANAGEMENT__________________________________________________________*/

  /*
   * void RouteurSolWebClass::handleLogin
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleLogin(AsyncWebServerRequest *request) {                         // If a POST request is made to URI /login
      bool flgVerified = false;
      char newusername[MAX_USERNAME_SIZE], newuserpassword[MAX_USERNAME_SIZE];
      uint8_t indUser = 0;
      String jsonBuff;
      JsonDocument  jsonRoot;
      char sessionID[16];                       // calculated at each login set in the cookie maPiscine (15 chars)
      long ttl = 1*60*60;                       // 1 hours by default in sec
      //  long ttl = 2*60;                          // 2 min by default in sec for debug
      bool keepAlive = false;
      int value;
      
    if( ! request->hasParam("username",true) || ! request->hasParam("password",true) 
      || request->getParam("username",true)->value() == NULL || request->getParam("password",true)->value() == NULL) { // If the POST request doesn't have username and password data
      Serial.println("Invalid Request, bad params !");  
      request->send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    } else {  // check the credentials
      request->getParam("username",true)->value().toCharArray(newusername,MAX_USERNAME_SIZE);
      request->getParam("password",true)->value().toCharArray(newuserpassword,MAX_USERNAME_SIZE);
      if(request->hasParam("keepAlive",true)){
        (strcmp((request->getParam("keepAlive",true)->value().c_str()),"false") == 0) ? keepAlive=false : keepAlive = true;
      } 
      Serial.printf("user : %s, passwd: %s, keepAlive: %s\n",newusername, newuserpassword, (keepAlive)?"true":"false");
    }

    for(indUser=0;indUser<MAX_USERS;indUser++){   // find user in config
      Serial.printf("config user: %s, passwd: %s\n",config.users[indUser].user,config.users[indUser].user_passwd);
        if(strcmp(config.users[indUser].user, newusername) == 0){       // found existing user check password
            if(strcmp(config.users[indUser].user_passwd, newuserpassword) == 0 ){     // good password
                flgVerified = true;
            }
            break;
        }  
    }
    if(flgVerified) {                     // If both the username and the password are correct
        if(keepAlive) ttl=ttl*24;           // one day if keepAlive one hour else
        generateKey(sessionID, ttl);
        jsonRoot["status"] = "Log in Successful";
        jsonRoot["username"] = String(newusername);
        jsonRoot["password"] = String(newuserpassword);
        jsonRoot["sessionID"] = sessionID;   
        jsonRoot["ttl"] = ttl;   
        jsonRoot["message"] = String("Welcome, ") + newusername + "!";
        Serial.println(F("Log in Successful"));
    } else {              // bad password or user not found
        jsonRoot["status"] = "Log in Failed";
        jsonRoot["message"] = "Wrong username/password! try again.";
        Serial.println(F("Log in Failed")); 
    }
    serializeJson(jsonRoot, jsonBuff);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff);
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
    Serial.print(F("Json is : "));
    Serial.println(jsonBuff);
  }

  /*
   * void RouteurSolWebClass::handleRegister
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleRegister(AsyncWebServerRequest *request){ 							// If a POST request is made to URI /register
      bool flgfound = false;
      int8_t flgFoundEmpty = -1;
      char newusername[MAX_USERNAME_SIZE], newuserpassword[MAX_USERNAME_SIZE], theadminpassword[MAX_USERNAME_SIZE];
      uint8_t indUser = 0;
      String jsonBuff;
      JsonDocument  jsonRoot;
      char sessionID[16];                       // calculated at each login set in the cookie maPiscine (15 chars)
      long ttl = 12*60*60*1000;                 // 12 hours by default
      String flgLogin;

    if( ! request->hasParam("newname",true) || ! request->hasParam("newpassword",true) || ! request->hasParam("adminpassword",true) 
        || request->getParam("newname",true)->value() == NULL || request->getParam("newpassword",true)->value() == NULL || request->getParam("adminpassword",true)->value() == NULL) { // If the POST request doesn't have username and password data
        request->send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
        return;
    }

    request->getParam("adminpassword",true)->value().toCharArray(theadminpassword,MAX_USERNAME_SIZE);      
    if(strcmp(theadminpassword, config.adminPassword) == 0){                                       // good admin password register new user or update it
        request->getParam("newname",true)->value().toCharArray(newusername,MAX_USERNAME_SIZE);
        request->getParam("newpassword",true)->value().toCharArray(newuserpassword,MAX_USERNAME_SIZE);
        Serial.printf("the new user is : %s, passwd is : %s\n",newusername,newuserpassword);

        for(indUser=0;indUser<MAX_USERS;indUser++){   // find user in config
            if(strcmp(config.users[indUser].user, newusername) == 0){       // found existing user check password
                flgfound = true;
                break;
            }
            if(config.users[indUser].user[0]==0){       // found an empty user space
                flgFoundEmpty = indUser;
            }
        }  
        if(flgfound) {            // Found username and updated the password
            jsonRoot["status"] = "User Already Exist";
            jsonRoot["username"] = String(newusername);
            jsonRoot["message"] = String("User ")+newusername+" already exist, try another one.";
            Serial.println(F("User already Exist"));
        } else {                        // not found so new user
          if(flgFoundEmpty != -1 ){     // flgFoundEmpty is the index breaked so still have room for a new user create a new entry
            strlcpy(config.users[flgFoundEmpty].user, newusername,11); 
            strlcpy(config.users[flgFoundEmpty].user_passwd, newuserpassword,11); 
            saveConfiguration();      // save the config to file
            generateKey(sessionID,ttl);
            jsonRoot["status"] = "New User Created Succesfully";
            jsonRoot["username"] = newusername;
            jsonRoot["password"] = newuserpassword;
            jsonRoot["sessionID"] = sessionID;   
            jsonRoot["ttl"] = ttl;
            if (request->hasParam("flgLogin",true)) {
                Serial.println("request has flgLogin");
                flgLogin = request->getParam("flgLogin",true)->value();
                Serial.printf("FlagLogin is %s\n",flgLogin.c_str());
            if(flgLogin == "true"){
                jsonRoot["message"] = String("Welcome, ") + newusername + "!";
            } else {
                jsonRoot["message"] = String("User ") + newusername + " created successfully";
            }
            jsonRoot["flgLogin"] = flgLogin;      
            } else {      // case where flgLogin not there no auto log then
                Serial.println("request has not flgLogin : return false");
                jsonRoot["flgLogin"] = "false";
                jsonRoot["message"] = String("User ") + newusername + " created successfully";
            }  
            Serial.println(F("New User Created Succesfully")); 
          } else {  // no room for new user
            jsonRoot["status"] = "No room for new user";
            jsonRoot["username"] = String(newusername);
            jsonRoot["message"] = "There is no room for a new user, use or delete an existing one !";
            Serial.println(F("No room for new user")); 
          } 
        }
    } else {  // bad adminPassword
        jsonRoot["status"] = "Bad AdminPassword";
        jsonRoot["message"] = "You entered an invalid admin password, please try again !";
        Serial.println(F("Bad AdminPassword")); 
    }
    serializeJson(jsonRoot, jsonBuff);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff);
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
    Serial.print(F("Json is : "));
    Serial.println(jsonBuff);
  }

  /*
   * void RouteurSolWebClass::handleChangAdminPW
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleChangAdminPW(AsyncWebServerRequest *request) {   
  // /setpw?pw=x & npw=y & cpw=y
      char password[MAX_USERNAME_SIZE], newPassword[MAX_USERNAME_SIZE], chkPassword[MAX_USERNAME_SIZE];
      String jsonBuff;
      JsonDocument  jsonRoot;

    Serial.println(F("Enter handleChangAdminPW"));
    if( ! request->hasParam("oldadminpasswd",true) || ! request->hasParam("newadminpassword",true) || ! request->hasParam("adminpasswordchk",true) 
        || request->getParam("oldadminpasswd",true)->value() == NULL || request->getParam("newadminpassword",true)->value() == NULL || request->getParam("adminpasswordchk",true)->value() == NULL) {       // If the POST request doesn't have username and password data
        request->send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
        return;
    }

    int params = request->params();
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      Serial.printf("[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if (strcmp(p->name().c_str(),"oldadminpasswd")==0){
        p->value().toCharArray(password,MAX_USERNAME_SIZE);
      } else if (strcmp(p->name().c_str(),"newadminpassword")==0){
        p->value().toCharArray(newPassword,MAX_USERNAME_SIZE);
      } else if (strcmp(p->name().c_str(),"adminpasswordchk")==0){
        p->value().toCharArray(chkPassword,MAX_USERNAME_SIZE);
      }
    }
    // check the admin credentials
    if(strcmp(password, config.adminPassword) != 0){            // check password
      jsonRoot["status"] = "Bad AdminPassword";
      jsonRoot["message"] = "You entered an invalid admin password, please try again !";
      Serial.println(F("Bad AdminPassword")); 
    } else {  // good adminPW, then do things
      strlcpy(config.adminPassword,newPassword,MAX_USERNAME_SIZE);
      saveConfiguration();                                                          // save changes to the config file
      jsonRoot["status"] = "Admin Password Updated";
      jsonRoot["message"] = "Change Admin Password OK";
      Serial.println(F("Change Admin Password OK")); 
    }  
    serializeJson(jsonRoot, jsonBuff);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff);
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
  }

  /*
   * void RouteurSolWebClass::handleGetUsers
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleGetUsers(AsyncWebServerRequest *request){
      String jsonBuff;
      JsonDocument  jsonRoot;
      uint8_t indU=0, indUser=0;

    jsonRoot["status"] = "User(s) Listed";
    JsonArray rtnUsers = jsonRoot["users"].to<JsonArray>();

     for(indUser=0;indUser<MAX_USERS;indUser++){   // find user in config
        rtnUsers[indU]["username"] = config.users[indUser].user;
        indU++;  
    }
    serializeJson(jsonRoot, jsonBuff);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff);
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
    Serial.print(F("Json is : "));
    Serial.println(jsonBuff);
  }

  /*
   * void RouteurSolWebClass::handleDeleteUsers
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleDeleteUsers(AsyncWebServerRequest *request){
      bool flgfound = false;
      char theadminpassword[MAX_USERNAME_SIZE];
      char currentUser[MAX_USERNAME_SIZE];
      char username[MAX_USERNAME_SIZE];
      String jsonBuff;
      JsonDocument jsonRoot;


    if(request->hasParam("adminpassword",true)){
      request->getParam("adminpassword",true)->value().toCharArray(theadminpassword,MAX_USERNAME_SIZE);      
      if(strcmp(theadminpassword, config.adminPassword) == 0 ){                                    // good admin password allowed to process
        for (int i=0; i<MAX_USERS;i++){
          sprintf(currentUser, "user%d",i);
          Serial.printf("currentuser is %s\n",currentUser);
          if(request->hasParam(currentUser,true)){     // checkbox is checked => delete the user (value of checkbox)
            request->getParam(currentUser,true)->value().toCharArray(username,MAX_USERNAME_SIZE);
            Serial.printf("User to delete is %s\n",username);
            for (int j=0; j<MAX_USERS; j++) {
              if(strcmp(config.users[j].user, username) == 0){            // found existing user 
                flgfound = true;
                strcpy(config.users[j].user,"");
                strcpy(config.users[j].user_passwd,"");
                Serial.println(F("user deleted"));
                break;
              }
            }
          }
        }
        if(flgfound){               // did deleted user 
          saveConfiguration();      // save the config to file
          jsonRoot["status"] = "User(s) Deleted";
          jsonRoot["message"] = "User(s) Deleted in the config file";
          Serial.println(F("User(s) deleted"));
        } else {
          jsonRoot["status"] = "No User(s) to Delete";
          jsonRoot["message"] = "Can not find existing user to delete !";
          Serial.println(F("Can not find existing user to delete !")); 
        }
      } else {  // bad adminPassword
        jsonRoot["status"] = "Bad Admin Password";
        jsonRoot["message"] = "You entered an invalid admin password, please try again !";
        Serial.println(F("Bad AdminPassword")); 
      }
    } else {  // no admin passord so => bad adminPassword
      jsonRoot["status"] = "Bad Admin Password";
      jsonRoot["message"] = "You entered an invalid admin password, please try again !";
      Serial.println(F("Bad AdminPassword")); 
    }
    serializeJson(jsonRoot, jsonBuff);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff);
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
    Serial.print(F("Json is : "));
    Serial.println(jsonBuff);
  }

/*__________________________________________________________PAGE_HANDLERS__________________________________________________________*/

  /*
   * bool RouteurSolWebClass::handleFileRead
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    bool RouteurSolWebClass::handleFileRead(String path, AsyncWebServerRequest *request) { // send the right file to the client (if it exists)
        String contentType, pathWithGz;
        File file;
        bool rtn = false;
        bool gzip = false;

        Serial.printf(" Asking for file : %s\n", path.c_str());
        int headers = request->headers();
        int i;
        for(i=0;i<headers;i++){
            const AsyncWebHeader* h = request->getHeader(i);
            Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        }

        contentType = getContentType(path);              // Get the MIME type
        pathWithGz = path + ".lgz";
        if (LittleFS.exists(pathWithGz)) {
            path += ".lgz";         // If there's a compressed version available use it
            gzip = true;
        }
        if (LittleFS.exists(path)) { 
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType);
            if(gzip)   response->addHeader("Content-Encoding", "gzip");
            response->addHeader("Access-Control-Allow-Origin","*");
            response->setCode(200);
            request->send(response);
            rtn = true;
        } else {
            Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
            rtn = false;
        }
        return rtn;
    }

  /*
   * bool RouteurSolWebClass::handleFileError
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    bool RouteurSolWebClass::handleFileError(String path, AsyncWebServerRequest *request) {         // send file not found to the client

        if (!handleFileRead("pages/Full404.html", request)){      // try sending 404.html file from SPIFFS before static one
                                                        // if not found then go for local one
            const char html404[] = R"(            
                <!doctype html>
                <html lang="en">
                <head>
                    <meta charset="utf-8">
                    <meta name="author" content="Ludovic Sorriaux">
                    <meta http-equiv="X-UA-Compatible" content="IE=edge">
                    <meta name="viewport" content="width=device-width, initial-scale=1">
<!--
                    <link href="https://cdn.tailwindcss.com" rel="stylesheet">
-->
                    <script src="https://cdn.tailwindcss.com"></script>

                </head>
                <body class="h-screen overflow-hidden flex items-center justify-center" style="background: #edf2f7;">
                  <div class="bg-gray-200 w-full px-16 md:px-0 h-screen flex items-center justify-center">
                    <div class="bg-white border border-gray-200 flex flex-col items-center justify-center px-4 md:px-8 lg:px-24 py-8 rounded-lg shadow-2xl">
                        <p class="text-6xl md:text-7xl lg:text-9xl font-bold tracking-wider text-gray-300">404</p>
                        <p class="text-2xl md:text-3xl lg:text-5xl font-bold tracking-wider text-gray-500 mt-4">Page Not Found</p>
                        <p class="text-gray-500 mt-4 pb-4 border-b-2 text-center">Sorry, the page you are looking for could not be found.</p>
                      <!--  <a href="#" class="flex items-center space-x-2 bg-blue-600 hover:bg-blue-700 text-gray-100 px-4 py-2 mt-6 rounded transition duration-150" title="Return">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" viewBox="0 0 20 20" fill="currentColor">
                                <path fill-rule="evenodd" d="M9.707 14.707a1 1 0 01-1.414 0l-4-4a1 1 0 010-1.414l4-4a1 1 0 011.414 1.414L7.414 9H15a1 1 0 110 2H7.414l2.293 2.293a1 1 0 010 1.414z" clip-rule="evenodd"></path>
                            </svg>
                            <span>Return</span>
                        </a> 
                      -->  
                    </div>
                  </div>
                </body>
                </html>
               )";
            request->send(404, "text/html", html404);
        } 

        Serial.print(F("NOT_FOUND: "));
        switch (request->method()) { 
            case HTTP_GET: Serial.print(F("GET"));
            break;    
            case HTTP_POST: Serial.print(F("POST"));
            break;    
            case HTTP_DELETE : Serial.print(F("DELETE"));
            break;    
            case HTTP_PUT : Serial.print(F("PUT"));
            break;    
            default : Serial.print(F("UNKNOWN"));
        }
        Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

        if(request->contentLength()){
            Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
            Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
        }

        int headers = request->headers();
        int i;
        for(i=0;i<headers;i++){
            const AsyncWebHeader* h = request->getHeader(i);
            Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for(i=0;i<params;i++){
            const AsyncWebParameter* p = request->getParam(i);
            if(p->isFile()){
            Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
            } else if(p->isPost()){
            Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            } else {
            Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
        return false;
    }

  /*
   * void RouteurSolWebClass::handleFileList
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleFileList( AsyncWebServerRequest *request) {
    String output = "[";
    
//        File root = SPIFFS.open("/","r");
        File root = LittleFS.open("/","r");
        if(root.isDirectory()){
            File file = root.openNextFile();
            while(file){
                if (output != "[") {
                    output += ',';
                }
                output += "{\"type\":\"";
                output += (file.isDirectory()) ? "dir" : "file";
                output += "\",\"name\":\"";
                output += String(file.name()).substring(1);
                output += "\",\"size\":\"";
                output += formatBytes(file.size()).c_str();
                output += "\"}";
                file = root.openNextFile();
            }
        }
    }

  /*
   * bool RouteurSolWebClass::checkSessionParam
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    bool RouteurSolWebClass::checkSessionParam(AsyncWebServerRequest *request){
        char sessionID[16];
        bool rtn = false;

    if(request->hasParam("sess",true)){                                     // check the session credentials
        request->getParam("sess",true)->value().toCharArray(sessionID,16);      
        rtn = isSessionValid(sessionID);
    }
    return rtn;
    }

/*__________________________________________________________ROUTEUR_HANDLERS__________________________________________________________*/

  /*
   * void RouteurSolWebClass::handleSetRouteurSSEData
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleSetRouteurSSEData(AsyncWebServerRequest *request){          // setRouteurPagePrincip
          String jsonBuff;
          JsonDocument jsonRoot;
        updateRouteurData();          
        if(request != nullptr){
          jsonRoot["status"] = "Processed";
          serializeJson(jsonRoot, jsonBuff);
          AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);
        }
        Serial.println(F("OK SetRouteurSSEData done"));
    }

  /*
   * void RouteurSolWebClass::updateRouteurData
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::updateRouteurData(){          
        String jsonBuffPP;
        JsonDocument routeurData;
        float ballonW = 0.0;
      /*    {
              "maisonW" : 2000,
              "edfW" : 1000,
              "pnxW" : 5000,
              "ballonW" : 200,
              "pacW" : 1000,
              "marcheForcee" : true/false,
              "modeManu" : true/false,
              "mfChauffeEau" : true/false,
              "mfPAC" : true/false,
              "temperatureEau" : 55,
              "pacSwitch" : true/false
            }


      */
     if(routeurEvents.count() != 0 ){                // au moins un client sur la page principale    
        routeurData["maisonW"] = LOADCurrentPower;
        routeurData["edfW"] = GRIDCurrentPower;
        routeurData["from"] = from;
        routeurData["pnxW"] = PVCurrentPower;
        ballonW = (valTriac/100)*config.puissanceBallon;
        Serial.printf("valtriac = %f, ballonW : %f\n",valTriac,ballonW);
        routeurData["ballonW"] = ballonW;
        routeurData["pacW"] = (debutRelayPAC!=0) ? config.puissancePac : 0;
        routeurData["marcheForcee"] = marcheForcee;
        routeurData["modeManu"] = modeManuEau || modeManuPAC;
        routeurData["mfChauffeEau"] = (valTriac != 0.0);
        routeurData["mfPAC"] = (debutRelayPAC!=0);
        routeurData["temperatureEau"] = temperatureEau;
        routeurData["pacSwitch"] = config.pacPresent;
        
        serializeJson(routeurData, jsonBuffPP);
        Serial.println(jsonBuffPP);
        AsyncEventSource::SendStatus status = routeurEvents.send(jsonBuffPP.c_str(), "routeurData", millis());
        Serial.println("OK updateRouteurData done");
      }
    }

  /*
   * void RouteurSolWebClass::handleSetSwitches
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleSetSwitches(AsyncWebServerRequest *request){              // setRouteurParamPP?sess=sessID&cmd=startProgram&pid=pid&en=en
        char theSwitch[20];
        char jsonMess[100];
        String value;
        bool val;
        String jsonBuff;
        JsonDocument jsonRoot;


      Serial.println(F("Enter handleSetSwitches :/setSwitches"));             // setSwitches?sess=sessID & switch=[pacSwitch|chauffeEauSwitch] & value=[1:true|0:false]
      if(!checkSessionParam(request)){                                        // check session
        Serial.println(F("Error : Invalid Session"));
        request->send(400, "text/plain", "400: Invalid Session");        
      } else {                                                                // good sessid, then do things

        int params = request->params();
        for(int i=0;i<params;i++){
            const AsyncWebParameter* p = request->getParam(i);
            Serial.printf("[%s]: %s\n", p->name().c_str(), p->value().c_str());
            if (strcmp(p->name().c_str(),"switch")==0){
            p->value().toCharArray(theSwitch,sizeof(theSwitch));
            } else if(strcmp(p->name().c_str(),"value")==0){
            value = p->value();     // if 0 then convert to false
            }
        }
        value.equals("true")?val=true:val=false;
        Serial.printf("switch:%s, value:%s, val:%s\n",theSwitch,value,(val)?"true":"false");       
        if(strcmp(theSwitch, "chauffeEauSwitch") == 0){       
          Serial.printf("chauffeEauSwitch, val:%s\n",(val)?"true":"false");
          marcheForceeSwitch(val);
          jsonRoot["status"] = "Processed";
          serializeJson(jsonRoot, jsonBuff);
          AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);

        } else if (strcmp(theSwitch, "pacSwitch") == 0){           
          Serial.printf("pacSwitch, val:%s\n",(val)?"true":"false");
          marcheForceePACSwitch(val);
          jsonRoot["status"] = "Processed";
          serializeJson(jsonRoot, jsonBuff);
          AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);

        } else {    // bad param swithch
          Serial.println(F("Error : Invalid Switch or value"));
          /*    {
                  "exception" : 'parsererror','timeout','abort'
                  "ErrorStatus" : 'The Status',
                  "Correction" : 'This what to do ..',
                  }
          */
          jsonRoot["ErrorStatus"] = "Invalid Switch or value";
          serializeJson(jsonRoot, jsonBuff);
          strcpy(jsonMess,jsonBuff.c_str());
          AsyncWebServerResponse *response = request->beginResponse(500, "application/json", jsonMess);
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);
        }
      }
      Serial.println("OK setRouteurSwitches done");
  }

  /*
   * void RouteurSolWebClass::handleSetParamsSSEData
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleSetParamsSSEData(AsyncWebServerRequest *request){          // setRouteurPageParams
        String jsonBuff;
        JsonDocument jsonRoot;
      updateRouteurParamsData();          
      if(request != nullptr){
        jsonRoot["status"] = "Processed";
        serializeJson(jsonRoot, jsonBuff);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
        response->addHeader("Cache-Control","no-cache");
        response->addHeader("Access-Control-Allow-Origin","*");
        request->send(response);
      }
      Serial.println(F("OK SetRouteurSSEData done"));
    }  

  /*
   * void RouteurSolWebClass::updateRouteurParamsData
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::updateRouteurParamsData(){          
        String jsonBuffPP;
        JsonDocument routeurParamsData;
        char theTime[10];

        /*    {
                  "heureBackup" : "20:00",
                  "secondBackup" : true/false,
                  "heureSecondBackup" : "20:00",
                  "sondeTemp" : true/false,
                  "tempEauMin" : 50,
                  "pacPresent" : true/false,
                  "puissPacOn" : 1000,
                  "puissPacOff" : 800,
                  "tempsOverProd" : 1,
                  "tempsMinPac" : 2,
                  "afficheur" : true/false,
                  "motionSensor" : true/false,
                  "volumeBallon" : 150,
                  "puissanceBallon" : 1500,
                  }
            */

      if(routeurParamsEvents.count() != 0 ){                                // au moins un client sur la page principale    
        sprintf(theTime,"%02d:%02d",plageMarcheForcee[0].heureOnMarcheForcee,plageMarcheForcee[0].minuteOnMarcheForcee);
        routeurParamsData["heureBackup"] = theTime;                         // heure début marche forcée
        routeurParamsData["secondBackup"] = config.secondBackup;            // si marche forcee secondaire dans la journee
        sprintf(theTime,"%02d:%02d",plageMarcheForcee[1].heureOnMarcheForcee,plageMarcheForcee[1].minuteOnMarcheForcee);
        routeurParamsData["heureSecondBackup"] = theTime;                   // heure début marche forcée sec
        routeurParamsData["sondeTemp"] = config.sondeTemp;                  // si sonde DS18B20
        routeurParamsData["tempEauMin"] = config.tempEauMin;                // réglage de la température minimale de l'eau en marche forcée
        routeurParamsData["pacPresent"] = config.pacPresent;                // si relay pour la PAC
        routeurParamsData["puissPacOn"] = config.puissPacOn;                // puissance du surplus pour déclencher le relay
        routeurParamsData["puissPacOff"] = config.puissPacOff;              // puissance du surplus pour stopper le relay //
        routeurParamsData["tempsOverProd"] = config.tempsOverProd;          // temps en sec d'overproduction avant de passer en PAC on si assez de puissance (relayOn)
        routeurParamsData["tempsMinPac"] = config.tempsMinPac;              // si PAC en Marche au moins 2h de marche même si pas assez de puissance
        routeurParamsData["afficheur"] = config.afficheur;                  // on-off de l'ecran
        routeurParamsData["motionSensor"] = config.motionSensor;            // si module de detection
        routeurParamsData["volumeBallon"] = config.volumeBallon;            // volume du ballon en litres
        routeurParamsData["puissanceBallon"] = config.puissanceBallon;      // puissance de la resistance du chauffe-eau
        
        serializeJson(routeurParamsData, jsonBuffPP);
        Serial.println(jsonBuffPP);
        routeurParamsEvents.send(jsonBuffPP.c_str(), "routeurParamsData", millis());
        Serial.println("OK updateRouteurParamsData done");
      }
    }

  /*
   * void RouteurSolWebClass::handleSetParams
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::handleSetParams(AsyncWebServerRequest *request){              
        String theTime;
        char jsonMess[100];
        uint16_t value;
        bool val, flgParamOk = false;
        String jsonBuff;
        JsonDocument jsonRoot;
        uint8_t nbCanges=0;

    //    /setRouteurParams?sess=sessID & heureMarcheForce=20:30 & marcheForceSuppSW=[1:true|0:false] & heureMarcheForceSec=20:30 & 
    //                      sondeTempSW=[1:true|0:false] & tempEauMin=60 & pacPresentSW=[1:true|0:false] & puissPacOn=1000 & puissPacOff=800 & tempsOverProd=60 & &
    //                      tempsPacMin=120 & afficheurSW=[1:true|0:false] & motionSensorSW=[1:true|0:false] & volBallon=150 & puissanceBallon=1500

      Serial.println(F("Enter handleSetRouteurParams :/setRouteurParams"));             
      if(!checkSessionParam(request)){                                        // check session
        Serial.println(F("Error : Invalid Session"));
        request->send(400, "text/plain", "400: Invalid Session");        
      } else {                                                                // good sessid, then do things
        int params = request->params();
        for(int i=0;i<params;i++){
          const AsyncWebParameter* p = request->getParam(i);
          Serial.printf("[%s]: %s\n", p->name().c_str(), p->value().c_str());

          if (strcmp(p->name().c_str(),"heureMarcheForce")==0){                 // heureMarcheForce
            theTime = p->value();
            if(strcmp(config.heureBackup,theTime.c_str())!=0){
              Serial.printf("   ... Changing heureBackup from %s to %s\n",config.heureBackup,theTime.c_str());
              strncpy(config.heureBackup,theTime.c_str(),sizeof(config.heureBackup));
              uint8_t heure = atoi(theTime.substring(0, 2).c_str());
              uint8_t minute = atoi(theTime.substring(3).c_str());
              plageMarcheForcee[0].heureOnMarcheForcee = heure;                      
              plageMarcheForcee[0].minuteOnMarcheForcee = minute;
              if(plageMarcheForcee[0].marcheForceeDone){        // if already done, check if reset if new date is in future
                tmElements_t tm;
                breakTime(now(), tm);  // break time_t into elements stored in tm struct
                tm.Hour = heure;
                tm.Minute = minute;
                size_t newTimeMarcheForce = makeTime(tm);
                if(now()<newTimeMarcheForce){ // in future
                  plageMarcheForcee[0].marcheForceeDone = false;
                }   
              }
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"marcheForceSuppSW")==0){         // marcheForceSuppSW
            (strcmp(p->value().c_str(),"true")==0) ? val=true : val=false;      // convert to boolean
            if(config.secondBackup != val){
              Serial.printf("   ... Changing marcheForceSuppSW from %s to %s\n",(config.secondBackup)?"true":"false",(val)?"true":"false");
              config.secondBackup = val;
              if( marcheForcee && !val &&                                         // marche forcee sec and started => need to stop
                ((hour()>plageMarcheForcee[1].heureOnMarcheForcee) || 
                (hour()==plageMarcheForcee[1].heureOnMarcheForcee) && (minute()>=plageMarcheForcee[1].minuteOnMarcheForcee)
                )
              ){
                finMarcheForcee = now();
              }
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"heureMarcheForceSec")==0){       // heureMarcheForceSec
            theTime = p->value();
            if(strcmp(config.heureSecondBackup,theTime.c_str())!=0){
              Serial.printf("   ... Changing heureMarcheForceSec from %s to %s\n",config.heureSecondBackup,theTime.c_str());
              strncpy(config.heureSecondBackup,theTime.c_str(),sizeof(config.heureSecondBackup));
              uint8_t heure = atoi(theTime.substring(0, 2).c_str());
              uint8_t minute = atoi(theTime.substring(3).c_str());
              plageMarcheForcee[1].heureOnMarcheForcee = heure;                      
              plageMarcheForcee[1].minuteOnMarcheForcee = minute;
              if(plageMarcheForcee[1].marcheForceeDone){        // if already done, check if reset if new date is in future
                tmElements_t tm;
                breakTime(now(), tm);  // break time_t into elements stored in tm struct
                tm.Hour = heure;
                tm.Minute = minute;
                size_t newTimeMarcheForce = makeTime(tm);
                if(now()<newTimeMarcheForce){ // in future
                  plageMarcheForcee[1].marcheForceeDone = false;
                }   
              }
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"sondeTempSW")==0){               // sondeTempSW
            (strcmp(p->value().c_str(),"true")==0) ? val=true : val=false;      // convert to boolean
            if(config.sondeTemp != val){
              Serial.printf("   ... Changing sondeTempSW from %s to %s\n",(config.sondeTemp)?"true":"false",(val)?"true":"false");
              config.sondeTemp = val;
              if(config.sondeTemp){           // ds18b20.begin() done in setup
                temperatureEau = 0.0;
              } else {
                temperatureEau = -127.0;            // valeur pour sonde non presente
              }
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"tempEauMin")==0){                // tempEauMin
            value = p->value().toInt();
            if(config.tempEauMin != value){
              Serial.printf("   ... Changing tempEauMin from %d to %d\n",config.tempEauMin,value);
              config.tempEauMin = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"pacPresentSW")==0){              // pacPresentSW
            (strcmp(p->value().c_str(),"true")==0) ? val=true : val=false;      // convert to boolean
            if(config.pacPresent != val){
              Serial.printf("   ... Changing pacPresentSW from %s to %s\n",(config.pacPresent)?"true":"false",(val)?"true":"false");
              config.pacPresent = val;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"puissPacOn")==0){                // puissPacOn
            value = p->value().toInt();
            if(config.puissPacOn != value){
              Serial.printf("   ... Changing puissPacOn from %d to %d\n",config.puissPacOn,value);
              config.puissPacOn = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"puissPacOff")==0){               // puissPacOff
            value = p->value().toInt();
            if(config.puissPacOff != value){
              Serial.printf("   ... Changing puissPacOff from %d to %d\n",config.puissPacOff,value);
              config.puissPacOff = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"tempsOverProd")==0){             // tempsOverProd
            value = p->value().toInt();
            if(config.tempsOverProd != value){
              Serial.printf("   ... Changing tempsOverProd from %d to %d\n",config.tempsOverProd,value);
              config.tempsOverProd = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"tempsPacMin")==0){               // tempsPacMin
            value = p->value().toInt();
            if(config.tempsMinPac != value){
              Serial.printf("   ... Changing tempsPacMin from %d to %d\n",config.tempsMinPac,value);
              config.tempsMinPac = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"afficheurSW")==0){               // afficheurSW
            (strcmp(p->value().c_str(),"true")==0) ? val=true : val=false;      // convert to boolean
            if(config.afficheur != val){
              Serial.printf("   ... Changing afficheurSW from %s to %s\n",(config.afficheur)?"true":"false",(val)?"true":"false");
              config.afficheur = val;
              if(!val){
                stopEcran();      // turn display off 
              } 
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"motionSensorSW")==0){            // motionSensorSW
            (strcmp(p->value().c_str(),"true")==0) ? val=true : val=false;      // convert to boolean
            if(config.motionSensor != val){
              Serial.printf("   ... Changing motionSensorSW from %s to %s\n",(config.motionSensor)?"true":"false",(val)?"true":"false");
              config.motionSensor = val;
              changePIRmode(val);
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"volBallon")==0){                 // volBallon
            value = p->value().toInt();
            if(config.volumeBallon != value){
              Serial.printf("   ... Changing volBallon from %d to %d\n",config.volumeBallon,value);
              config.volumeBallon = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else if (strcmp(p->name().c_str(),"puissanceBallon")==0){           // puissanceBallon
            value = p->value().toInt();
            if(config.puissanceBallon != value){
              Serial.printf("   ... Changing puissanceBallon from %d to %d\n",config.puissanceBallon,value);
              config.puissanceBallon = value;
              nbCanges++;
            }
            flgParamOk = true;
          } else {                                                              // Error invalid param
            flgParamOk = false;
          }  
        }
        if(flgParamOk) {
          if(nbCanges!=0){
            jsonRoot["status"] = "New Parameters Saved";
          } else {        // No parameters saved
            jsonRoot["status"] = "Processed";
          }
          serializeJson(jsonRoot, jsonBuff);
          AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);
          Serial.printf("%d changements de parametres faits \n",nbCanges);
          updateRouteurParamsData();    // set it back to web
          saveConfiguration();
        } else {                                                    // bad command
          Serial.println(F("Error : Invalid Switch or value"));
                  /*    {
                          "exception" : 'parsererror','timeout','abort'
                          "ErrorStatus" : 'The Status',
                          "Correction" : 'This what to do ..',
                          }
                  */
          jsonRoot["ErrorStatus"] = "Invalid Switch or value";
          serializeJson(jsonRoot, jsonBuff);
          strcpy(jsonMess,jsonBuff.c_str());
          AsyncWebServerResponse *response = request->beginResponse(500, "application/json", jsonMess);
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);
        }
      }
  }

/*__________________________________________________________REST_HANDLERS__________________________________________________________*/

  /*
   * void RouteurSolWebClass::handleChangePassword
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleChangePassword(AsyncWebServerRequest *request){ // /setpw ? adminpassword = oldAdminPW & password = newAdminPW

    char theadminpassword[MAX_USERNAME_SIZE];
    char newadminpassword[MAX_USERNAME_SIZE];
    String jsonBuff;
    JsonDocument  jsonRoot;

    if( ! request->hasParam("password",true) || ! request->hasParam("adminpassword",true) 
        || request->getParam("password",true)->value() == NULL || request->getParam("adminpassword",true)->value() == NULL) {       // If the POST request doesn't have username and password data
        request->send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
        return;
    }

    request->getParam("adminpassword",true)->value().toCharArray(theadminpassword,MAX_USERNAME_SIZE);      
    if(strcmp(theadminpassword, config.adminPassword) == 0 ){                                      // good admin password allowed to update it
        request->getParam("password",true)->value().toCharArray(newadminpassword,MAX_USERNAME_SIZE);
        strlcpy( config.adminPassword, newadminpassword, MAX_USERNAME_SIZE);                                        // set the new password
        saveConfiguration();                                                                  // save changes to the config file
        jsonRoot["status"] = "Admin Password Updated";
        jsonRoot["message"] = "Succesfully updated the admin passord !";
        Serial.println(F("Admin Password Updated")); 
    } else {  // bad adminPassword
        jsonRoot["status"] = "Bad Admin Password";
        jsonRoot["message"] = "You entered an invalid admin password, please try again !";
        Serial.println(F("Bad AdminPassword")); 
    }
    serializeJson(jsonRoot, jsonBuff);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff);
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
    Serial.print(F("Json is : "));
    Serial.println(jsonBuff);
    }

  /*
   * void RouteurSolWebClass::handleGetRouteurStatus
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleGetRouteurStatus(AsyncWebServerRequest *request) {  
        /*  /getrouteurstatus ? pw = adminPW return :
            {
              "maisonW" : 2000,
              "edfW" : 1000,
              "pnxW" : 5000,
              "ballonW" : 200,
              "pacW" : 1000,
              "marcheForcee" : true/false,
              "modeManuEau" : true/false,
              "modeManuPAC" : true/false,
              "mfChauffeEau" : true/false,
              "mfPAC" : true/false,
              "temperatureEau" : 50,
              ""
            }
      */

      String jsonBuffPP;
      JsonDocument routeurData;
      char theadminpassword[MAX_USERNAME_SIZE];


    Serial.println("\nEnter handleGetRouteurStatus");
    if(request->hasParam("pw")){
      request->getParam("pw")->value().toCharArray(theadminpassword,MAX_USERNAME_SIZE);      
      if(strcmp(theadminpassword, config.adminPassword) == 0 ){                                    // good admin password allowed to process
        routeurData["maisonW"] = LOADCurrentPower;
        routeurData["edfW"] = GRIDCurrentPower;
        routeurData["from"] = from;
        routeurData["pnxW"] = PVCurrentPower;
        routeurData["ballonW"] = (config.puissanceBallon*valTriac/100);
        routeurData["pacW"] = (debutRelayPAC!=0) ? config.puissancePac : 0;
        routeurData["marcheForcee"] = marcheForcee;
        routeurData["modeManuEau"] = modeManuEau;
        routeurData["modeManuPAC"] = modeManuPAC;
        routeurData["mfChauffeEau"] = (valTriac != 0.0);
        routeurData["mfPAC"] = (debutRelayPAC!=0);
        routeurData["temperatureEau"] = temperatureEau;
        routeurData["finMarcheForcee"] = finMarcheForcee;
      } else {  // bad adminPassword
          routeurData["status"] = "Bad Admin Password";
          routeurData["message"] = "You entered an invalid admin password, please try again !";
          Serial.println(F("Bad AdminPassword")); 
      }
      serializeJson(routeurData, jsonBuffPP);
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuffPP);
      response->addHeader("Cache-Control","no-cache");
      response->addHeader("Access-Control-Allow-Origin","*");
      request->send(response);
    } else {
      Serial.println("Error : Invalid Parameter");
      request->send(400, "text/plain", "400: Invalid Parameter");        
    }
    Serial.println(jsonBuffPP);
    Serial.println("OK handleGetRouteurStatus done\n");
  }

  /*
   * void RouteurSolWebClass::handleSetRouteurSwitch
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::handleSetRouteurSwitch(AsyncWebServerRequest *request) {  // /setrouteurswitch ? pw=adminPWD & [chauffeEau|pac]=[true|false]
      String jsonBuffPP;
      JsonDocument routeurData;
      char theadminpassword[MAX_USERNAME_SIZE];
      char modeStr[8];   // "true" or "false"
      bool mode = false;


    Serial.println("\nEnter handleSetRouteurSwitch");
    if(request->hasParam("pw") && (request->hasParam("chauffeEau") || request->hasParam("pac"))){
      request->getParam("pw")->value().toCharArray(theadminpassword,MAX_USERNAME_SIZE);      
      if(strcmp(theadminpassword, config.adminPassword) == 0 ){                                    // good admin password allowed to process
        if(request->hasParam("chauffeEau")){
          request->getParam("chauffeEau")->value().toCharArray(modeStr,sizeof(modeStr)); 
          mode = (strcmp(modeStr,"true") == 0);  
          marcheForceeSwitch(mode);
          Serial.printf("OK Set switch chauffeEau to %s\n",modeStr);
          request->send(200, "text/plain", "200: OK Set switch done");        
        } else if(request->hasParam("pac")){
          request->getParam("pac")->value().toCharArray(modeStr,sizeof(modeStr));   
          mode = (strcmp(modeStr,"true") == 0);  
          marcheForceePACSwitch(mode);
          Serial.printf("OK Set switch pac to %s\n",modeStr);
          request->send(200, "text/plain", "200: OK Set switch done");        
        } else {
          Serial.println("Error : Invalid Parameter");
          request->send(400, "text/plain", "400: Invalid Parameter");        
        }
      } else {  // bad adminPassword
        Serial.println("Error : Bad Admin Password");
        request->send(400, "text/plain", "400: Bad Admin Password");        
      }
    } else {
      Serial.println("Error : Invalid Parameter");
      request->send(400, "text/plain", "400: Invalid Parameter");        
    }
    Serial.println("OK handleSetRouteurSwitch done\n");
  }

  /*
   * void RouteurSolWebClass::showJsonConfig
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void RouteurSolWebClass::showJsonConfig(AsyncWebServerRequest *request){
    char jsonPrintConfig[3072];
    JsonDocument jsonConfig;
    JsonArray table, table2;
    JsonObject obj;
    int i,j;

/*
    {
      "adminPassword": "manager",
      "users":[		
          { "name": "ludovic", "password": "Ludovic1"}
      ],
      "wifi":[		
          { "ssid": "SFR_2E48", "password": "rsionardishoodbe2rmo"},      
          { "ssid": "SFR_2E48_EXT", "password": "rsionardishoodbe2rmo"}
      ],
      "solarEdge":"8INR9G7TVYP03QAMRMNKJYRNN0MTVJSQ",  
      "parametres": {
          "volumeBallon" : 150,        
          "puissanceBallon": 1500,     
          "heureBackup" : "23:00",      
          "tempEauMin" : 50,           
          "secondBackup" : false,       
          "heureSecondBackup": "13:00", 
          "sondeTemp" : true,          
          "pacPresent" : true,          
          "puissancePac": 1000,         
          "puissPacOn" : 1000,          
          "puissPacOff" : 800,          
          "tempsOverProd" : 900,        
          "tempsMinPac" : 7200,         
          "afficheur" : true,           
          "motionSensor" : false         
        }
    }
*/
    
    Serial.println(F("Asked for print json config file"));
    jsonConfig["adminPassword"] = config.adminPassword;
    table = jsonConfig["users"].to<JsonArray>();

    for(i=0;i<MAX_USERS;i++){
        obj = table.add<JsonObject>();
        obj["name"] = config.users[i].user;
        obj["password"] = config.users[i].user_passwd;
    }  
    table = jsonConfig["wifi"].to<JsonArray>();
    for(i=0;i<MAX_WIFI;i++){
        obj = table.add<JsonObject>();
        obj["ssid"] = config.wifi[i].ssid;
        obj["password"] = config.wifi[i].ssid_passwd;
    }  
    jsonConfig["solarEdge"] = config.solarEdge;
    obj = jsonConfig["parametres"].to<JsonObject>();
    obj["volumeBallon"] = config.volumeBallon;      
    obj["puissanceBallon"] = config.puissanceBallon;    
    obj["heureBackup"] = config.heureBackup;     
    obj["tempEauMin"] = config.tempEauMin;           
    obj["secondBackup"] = config.secondBackup;       
    obj["heureSecondBackup"] = config.heureSecondBackup; 
    obj["sondeTemp"] = config.sondeTemp;          
    obj["pacPresent"] = config.pacPresent;          
    obj["puissancePac"] = config.puissancePac;         
    obj["puissPacOn"] = config.puissPacOn;          
    obj["puissPacOff"] = config.puissPacOff;          
    obj["tempsOverProd"] = config.tempsOverProd;        
    obj["tempsMinPac"] = config.tempsMinPac;         
    obj["afficheur"] = config.afficheur;           
    obj["motionSensor"] = config.motionSensor;         

    serializeJsonPretty(jsonConfig, Serial);
    Serial.println();
    serializeJsonPretty(jsonConfig, jsonPrintConfig);
    request->send(200, "text/plain",jsonPrintConfig );
  }


/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

  /*
   * String RouteurSolWebClass::formatBytes
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    String RouteurSolWebClass::formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
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

  /*
   * String RouteurSolWebClass::getContentType
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    String RouteurSolWebClass::getContentType(String filename) { // determine the filetype of a given filename, based on the extension
        if (filename.endsWith(".html")) return "text/html";
        else if (filename.endsWith(".css")) return "text/css";
        else if (filename.endsWith(".js")) return "application/javascript";
        else if (filename.endsWith(".ico")) return "image/x-icon";
        else if (filename.endsWith(".gz")) return "application/x-gzip";
        return "text/plain";
    }

  /*
   * bool RouteurSolWebClass::generateKey
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    bool RouteurSolWebClass::generateKey(char *sessID,long ttl){
            char strSess[16];
            char alphabeth[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; 
            uint8_t alphabethLength = 62;
            int i = 0;
            bool flagOK=false;

        for (uint8_t n = 0; n < 15 ; n++) {
            *(strSess+n) = alphabeth[random(0, alphabethLength-1)];
        }
        *(strSess+15) = '\0';
        Serial.printf("new key strSess is : %s\n", strSess);  
        strlcpy( sessID, strSess, 16);  
        Serial.printf("new key is : %s\n", sessID);  

        // manage activeSessions struct
        for (i=0; i<10;i++){
            if(activeSessions[i].timecreated+activeSessions[i].ttl < now() ){ // time exhasted : delete sessionID
            activeSessions[i].sessID[0]=0;
            activeSessions[i].ttl=0;
            activeSessions[i].timecreated=0;
            }
        }
        // store new infos
        for (i=0; i<10;i++){
            if(activeSessions[i].ttl == 0) { // found an empty slot
            strlcpy(activeSessions[i].sessID, sessID, 16);
            activeSessions[i].timecreated=now();
            activeSessions[i].ttl=ttl;
            flagOK=true;
            break;
            }
        }
        printActiveSessions();
            
        return flagOK;   //  if (!flagOK){   // couldn't store no room left.
    }

  /*
   * void RouteurSolWebClass::printActiveSessions
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void RouteurSolWebClass::printActiveSessions(){   // to help debug
        Serial.printf("Dump of Active session tab, now is: %lld\n",now());
        for (uint8_t i=0; i<10;i++){
            if(activeSessions[i].timecreated == 0) break; // no more to show
                Serial.printf("sessionID: %s, ttl: %lld, timecreated: %lld\n", activeSessions[i].sessID,activeSessions[i].ttl,activeSessions[i].timecreated);
        }  
    }


