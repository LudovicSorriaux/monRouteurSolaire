#include "routeurWeb.h"
#include "routeurSolManager.h"

    RouteurSolWebClass::~RouteurSolWebClass(void)
      {};

    RouteurSolWebClass::RouteurSolWebClass(){
        Serial.println(F("init RouteurSolWeb"));
    }


// PUBLIC functions 

    void RouteurSolWebClass::startup(){
      Serial.println(F("RouteurSolWeb Startup ... "));
      startServer();               // Start a HTTP server with a file read handler and an upload handler
      startMDNS();                 // Start the mDNS responder
    }

    void RouteurSolWebClass::OnUpdate(){
        #if defined(ESP8266)
            MDNS.update();
        #endif
        updateRouteurSolData();
    }

/*__________________________________________________________  update status FUNCTIONS __________________________________________________________*/


// PRIVATE functions 

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

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
        server.on("/getrouteurSolstatus", HTTP_GET, std::bind(&RouteurSolWebClass::handleGetRouteurSolStatus, this, std::placeholders::_1)); 			

        // ---------- RouteurSol ------
        server.on("/setRouteurSolSSEData", HTTP_POST, std::bind(&RouteurSolWebClass::handleSetRouteurSolSSEData, this, std::placeholders::_1)); 
        server.on("/setSwitches", HTTP_POST, std::bind(&RouteurSolWebClass::handleSetSwitches, this, std::placeholders::_1));      

          // -------- RouteurSol SSE event management ------------
        routeurSolEvents.onConnect([](AsyncEventSourceClient *client){                 // OK     
            if(client->lastId()){
                Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
            }
            client->send("hello! routeurSolEvents Ready", NULL, millis(), 10000);  // send message "hello!", id current millis and set reconnect delay to 1 second
        });

          // -------- call statics files not html ------------
        server.serveStatic("/", LittleFS, "/");
/*        server.serveStatic("/css", LittleFS, "/css/");
        server.serveStatic("/js", LittleFS, "/js/");
        server.serveStatic("/img", LittleFS, "/img/");
        server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");
        server.serveStatic("/manifest.json", LittleFS, "/manifest.json");
*/
        server.onNotFound(std::bind(&RouteurSolWebClass::handleOtherFiles, this, std::placeholders::_1));           			  // When a client requests an unknown URI (i.e. something other than "/"), call function handleNotFound"


        server.addHandler(&routeurSolEvents);

        server.begin();                             			  // start the HTTP server
        Serial.println(F("HTTP server started, IP address: "));
        Serial.println(WiFi.localIP());

    }

/*__________________________________________________________AUTHENTIFY_FUNCTIONS__________________________________________________________*/

    void RouteurSolWebClass::printActiveSessions(char *sessID){   // to help debug
        Serial.printf("Dump of Active session tab, now is: %lld\n",now());
        for (uint8_t i=0; i<10;i++){
            Serial.printf("sessionID: %s, ttl: %lld, timecreated: %lld\n", activeSessions[i].sessID,activeSessions[i].ttl,activeSessions[i].timecreated);
            if(activeSessions[i].timecreated == 0) break; // no more to show
        }  
    }

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

    void RouteurSolWebClass::handleRoot(AsyncWebServerRequest *request) {                         // When URI / is requested, send a login web page
        Serial.println(F("Enter handleRoot"));
        if(!handleFileRead("/main.html",request)){
            handleFileError("/main.html",request);                 // file not found
        } 
    }

    void RouteurSolWebClass::handleOtherFiles(AsyncWebServerRequest *request){ 	// if the requested file or page doesn't exist, return a 404 not found error
        Serial.println(F("Enter handleOtherFiles"));
        Serial.printf(" http://%s %s\n", request->host().c_str(), request->url().c_str());
        if(!handleFileRead(request->url(),request)){
            handleFileError(request->url(),request);                 // file not found
        } 
    }

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

  void RouteurSolWebClass::handleLogin(AsyncWebServerRequest *request) {                         // If a POST request is made to URI /login
      bool flgVerified = false;
      char newusername[MAX_USERNAME_SIZE], newuserpassword[MAX_USERNAME_SIZE];
      uint8_t indUser = 0;
      String jsonBuff;
      DynamicJsonDocument  jsonRoot(256);
      char sessionID[16];                       // calculated at each login set in the cookie maPiscine (15 chars)
      long ttl = 1*60*60;                       // 1 hours by default in sec
      //  long ttl = 2*60;                          // 2 min by default in sec for debug
      bool keepAlive = false;
      int value;
      
    if( ! request->hasParam("username",true) || ! request->hasParam("password",true) 
      || request->getParam("username",true)->value() == NULL || request->getParam("password",true)->value() == NULL) { // If the POST request doesn't have username and password data
      request->send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    } else {  // check the credentials
      request->getParam("username",true)->value().toCharArray(newusername,MAX_USERNAME_SIZE);
      request->getParam("password",true)->value().toCharArray(newuserpassword,MAX_USERNAME_SIZE);
      (request->hasParam("keepAlive",true)) ? keepAlive=true : keepAlive = false;
    }

    for(indUser=0;indUser<MAX_USERS;indUser++){   // find user in config
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

  void RouteurSolWebClass::handleRegister(AsyncWebServerRequest *request){ 							// If a POST request is made to URI /register
      bool flgfound = false;
      int8_t flgFoundEmpty = -1;
      char newusername[MAX_USERNAME_SIZE], newuserpassword[MAX_USERNAME_SIZE], theadminpassword[MAX_USERNAME_SIZE];
      uint8_t indUser = 0;
      String jsonBuff;
      DynamicJsonDocument  jsonRoot(256);
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

  void RouteurSolWebClass::handleChangAdminPW(AsyncWebServerRequest *request) {   
  // /setpw?pw=x & npw=y & cpw=y
      char password[MAX_USERNAME_SIZE], newPassword[MAX_USERNAME_SIZE], chkPassword[MAX_USERNAME_SIZE];
      String jsonBuff;
      DynamicJsonDocument  jsonRoot(256);

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

  void RouteurSolWebClass::handleGetUsers(AsyncWebServerRequest *request){
      String jsonBuff;
      DynamicJsonDocument  jsonRoot(256);
      uint8_t indU=0, indUser=0;

    jsonRoot["status"] = "User(s) Listed";
    JsonArray rtnUsers = jsonRoot.createNestedArray("users");

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

  void RouteurSolWebClass::handleDeleteUsers(AsyncWebServerRequest *request){
      bool flgfound = false;
      char theadminpassword[MAX_USERNAME_SIZE];
      char currentUser[MAX_USERNAME_SIZE];
      char username[MAX_USERNAME_SIZE];
      String jsonBuff;
      DynamicJsonDocument jsonRoot(100);


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

    bool RouteurSolWebClass::handleFileError(String path, AsyncWebServerRequest *request) {         // send file not found to the client

        if (!handleFileRead("/404.html", request)){      // try sending 404.html file from SPIFFS before static one
                                                        // if not found then go for local one
            const char html404[] = R"(            
                <!DOCTYPE html>
                <html>
                <head>
                    <title>My mobile page!</title>
                    <meta charset="UTF-8">
                    <meta name="author" content="Ludovic Sorriaux">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
                    <link href="//netdna.bootstrapcdn.com/twitter-bootstrap/2.3.2/css/bootstrap-combined.min.css" rel="stylesheet" id="bootstrap-css">
                    <script src="//netdna.bootstrapcdn.com/twitter-bootstrap/2.3.2/js/bootstrap.min.js"></script>
                </head>
                <body>
                    <div class="container">
                    <div class="row">
                        <div class="span12">
                        <div class="hero-unit center">
                            <h1>Page Not Found <small><font face="Tahoma" color="red">Error 404</font></small></h1>
                            <br />
                            <p>The page you requested could not be found, either contact your webmaster</p>
                            <p>Or try again. Use your browsers <b>Back</b> button to navigate to the page you have prevously come from</p>
                        </div>
                        <br />
                        </div>
                    </div>
                    </div>
                </body>
                </html>
                )";
            request->send(404, "text/plain", html404);
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

    bool RouteurSolWebClass::checkSessionParam(AsyncWebServerRequest *request){
        char sessionID[16];
        bool rtn = false;

    if(request->hasParam("sess",true)){                                     // check the session credentials
        request->getParam("sess",true)->value().toCharArray(sessionID,16);      
        rtn = isSessionValid(sessionID);
    }
    return rtn;
    }

/*__________________________________________________________CUVE_HANDLERS__________________________________________________________*/

    void RouteurSolWebClass::handleSetRouteurSolSSEData(AsyncWebServerRequest *request){          // setRouteurSolPagePrincip
          String jsonBuff;
          DynamicJsonDocument jsonRoot(30);
        updateRouteurSolData();          
        if(request != nullptr){
          jsonRoot["status"] = "Processed";
          serializeJson(jsonRoot, jsonBuff);
          AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
          response->addHeader("Cache-Control","no-cache");
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);
        }
        Serial.println(F("OK SetRouteurSolSSEData done"));
    }

    void RouteurSolWebClass::handleSetSwitches(AsyncWebServerRequest *request){              // setRouteurSolParamPP?sess=sessID&cmd=startProgram&pid=pid&en=en
          char theSwitch[15];
          char jsonMess[100];
          int8_t value;
          bool val;
          String jsonBuff;
          DynamicJsonDocument jsonRoot(30);


        Serial.println(F("Enter handleSetSwitches :/setSwitches"));             // sess='+sessID+'&switch=manuSwitch'+'&value=true'
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
                value = p->value().toInt();     // if 0 then convert to false
                }
            }
            if(!isnan(value)) {  
                (value==0) ? val=false : val=true;

                if(strcmp(theSwitch, "manuSwitch") == 0){       
                    flgButtonManu = val;
                } else if (strcmp(theSwitch, "moteurSwitch") == 0){           
                    flgButtonPompe = val;
                } else if (strcmp(theSwitch, "troppleinSwitch") == 0){          
                    flgButtonTroplein = val;
                } else if (strcmp(theSwitch, "eauvilleSwitch") == 0){          
                    flgButtonEauville = val;
                }
                jsonRoot["status"] = "Processed";
                serializeJson(jsonRoot, jsonBuff);
                AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuff.c_str());
                response->addHeader("Cache-Control","no-cache");
                response->addHeader("Access-Control-Allow-Origin","*");
                request->send(response);
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
        Serial.printf("OK setRouteurSolParam done");
    }

    void RouteurSolWebClass::updateRouteurSolData(){          
        String jsonBuffPP;
        DynamicJsonDocument routeurSolData(50);
        /*    {
                "water" : 2000,
                "pct" : 10,
                "manuSW" : true/false,
                "tropPleinSW" : true/false,
                "eauvilleSW" : true/false,
                "moteurSW" : true/false,
                "flgPBTropPlein" : true/false,
              }
        */
      if(routeurSolEvents.count() != 0 ){                // au moins un client sur la page principale    
        routeurSolData["water"] = routeurSolManager.getWater();
        routeurSolData["pct"] = routeurSolManager.getPct();
        routeurSolData["manuSW"] = routeurSolManager.getSwitches(0);
        routeurSolData["tropPleinSW"] = routeurSolManager.getSwitches(1);
        routeurSolData["eauvilleSW"] = routeurSolManager.getSwitches(2);
        routeurSolData["moteurSW"] = routeurSolManager.getSwitches(3);
        routeurSolData["flgPBTropPlein"] = routeurSolManager.getFlgPBTropPlein();
        
        serializeJson(routeurSolData, jsonBuffPP);
        Serial.println(jsonBuffPP);
        routeurSolEvents.send(jsonBuffPP.c_str(), "routeurSolData", millis());
      }
      Serial.println("OK updateRouteurSolData done");
    }

/*__________________________________________________________REST_HANDLERS__________________________________________________________*/

  void RouteurSolWebClass::handleChangePassword(AsyncWebServerRequest *request){ // /setpw ? adminpassword = oldAdminPW & password = newAdminPW

    char theadminpassword[MAX_USERNAME_SIZE];
    char newadminpassword[MAX_USERNAME_SIZE];
    String jsonBuff;
    DynamicJsonDocument  jsonRoot(100);

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

  void RouteurSolWebClass::handleGetRouteurSolStatus(AsyncWebServerRequest *request) {  
        /*  /getrouteurSolstatus ? pw = adminPW return :
            {
              "water" : 2000,
              "pct" : 10,
              "manuSW" : true/false,
              "tropPleinSW" : true/false,
              "eauvilleSW" : true/false,
              "moteurSW" : true/false,
              "flgPBTropPlein" : true/false,
            }
      */

      String jsonBuffPP;
      DynamicJsonDocument routeurSolData(50);
      char theadminpassword[MAX_USERNAME_SIZE];


    Serial.println(F("Enter handleGetRouteurSolStatus"));

    if(request->hasParam("pw",true)){
      request->getParam("pw",true)->value().toCharArray(theadminpassword,MAX_USERNAME_SIZE);      
      if(strcmp(theadminpassword, config.adminPassword) == 0 ){                                    // good admin password allowed to process
        routeurSolData["water"] = routeurSolManager.getWater();
        routeurSolData["pct"] = routeurSolManager.getPct();
        routeurSolData["manuSW"] = routeurSolManager.getSwitches(0);
        routeurSolData["tropPleinSW"] = routeurSolManager.getSwitches(1);
        routeurSolData["eauvilleSW"] = routeurSolManager.getSwitches(2);
        routeurSolData["moteurSW"] = routeurSolManager.getSwitches(3);
        routeurSolData["flgPBTropPlein"] = routeurSolManager.getFlgPBTropPlein();
        
      } else {  // bad adminPassword
          routeurSolData["status"] = "Bad Admin Password";
          routeurSolData["message"] = "You entered an invalid admin password, please try again !";
          Serial.println(F("Bad AdminPassword")); 
      }
      serializeJson(routeurSolData, jsonBuffPP);
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonBuffPP);
      response->addHeader("Cache-Control","no-cache");
      response->addHeader("Access-Control-Allow-Origin","*");
      request->send(response);

      Serial.println("OK handleGetRouteurSolStatus done");
      Serial.println(jsonBuffPP);

    }
  }

  void RouteurSolWebClass::showJsonConfig(AsyncWebServerRequest *request){
    char jsonPrintConfig[3072];
    DynamicJsonDocument jsonConfig(3072);
    JsonArray table, table2;
    JsonObject obj;
    int i,j;


    Serial.println(F("Asked for print json config file"));
    jsonConfig["adminPassword"] = config.adminPassword;
    table = jsonConfig.createNestedArray("users");
    for(i=0;i<MAX_USERS;i++){
        obj = table.createNestedObject();
        obj["name"] = config.users[i].user;
        obj["password"] = config.users[i].user_passwd;
    }  
    table = jsonConfig.createNestedArray("wifi");
    for(i=0;i<MAX_WIFI;i++){
        obj = table.createNestedObject();
        obj["ssid"] = config.wifi[i].ssid;
        obj["password"] = config.wifi[i].ssid_passwd;
    }  
    table = jsonConfig.createNestedArray("stations");
    for(i=0;i<MAX_STATIONS;i++){
        obj = table.createNestedObject();
        obj["name"] = config.stations[i].name;
        obj["sid"] = i;
        obj["enable"] = (config.stations[i].enable) ? true : false;
    }  
    table = jsonConfig.createNestedArray("programs");
    for(i=0;i<MAX_PROGRAMS;i++){
        obj = table.createNestedObject();
        obj["name"] = config.programs[i].name;
        obj["pid"] = i;
        obj["enable"] = (config.programs[i].enable) ? true : false;
        obj["start"] = config.programs[i].start;
        obj["useWeather"] = config.programs[i].useWeather;
        table2 = obj.createNestedArray("days");
        for(j=0;j<7;j++){
            table2[j] = config.programs[i].days[j];
        }
        table2 = obj.createNestedArray("days");
        for(j=0;j<MAX_STATIONS;j++){
            table2[j] = config.programs[i].durations[j];
        }
    }  
    serializeJsonPretty(jsonConfig, Serial);
    Serial.println();
    serializeJsonPretty(jsonConfig, jsonPrintConfig);
    request->send(200, "text/plain",jsonPrintConfig );
  }


/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

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

    String RouteurSolWebClass::getContentType(String filename) { // determine the filetype of a given filename, based on the extension
        if (filename.endsWith(".html")) return "text/html";
        else if (filename.endsWith(".css")) return "text/css";
        else if (filename.endsWith(".js")) return "application/javascript";
        else if (filename.endsWith(".ico")) return "image/x-icon";
        else if (filename.endsWith(".gz")) return "application/x-gzip";
        return "text/plain";
    }

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

    void RouteurSolWebClass::printActiveSessions(){   // to help debug
        Serial.printf("Dump of Active session tab, now is: %lld\n",now());
        for (uint8_t i=0; i<10;i++){
            if(activeSessions[i].timecreated == 0) break; // no more to show
                Serial.printf("sessionID: %s, ttl: %lld, timecreated: %lld\n", activeSessions[i].sessID,activeSessions[i].ttl,activeSessions[i].timecreated);
        }  
    }


