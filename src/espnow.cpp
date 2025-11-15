/*******************************************************************************
 * @file    espnow.cpp
 * @brief   Implémentation communication ESP-NOW routeur
 * @details Gestion protocole ESP-NOW pour communication sans fil, callbacks
 *          réception (data, hello, sync), détection manager, retries.
 *          Compatible ESP8266 et ESP32.
 * 
 * @author  Ludovic Sorriaux
 * @date    2024
 *******************************************************************************/

// -----------------------------------------------------------------------------------------
//  ESPNOW for routeur
// -----------------------------------------------------------------------------------------


#include <espnow.h>

  /*
   * EspNowClass::EspNowClass
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  EspNowClass::EspNowClass(){};
  /*
   * EspNowClass::~EspNowClass
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  EspNowClass::~EspNowClass(){};

// public

  /*
   * void EspNowClass::initESPNOW
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void EspNowClass::initESPNOW(){
        char macStr[18];
  
    if(foundManager){                        // if already found manager just need to reinit variables
      if(debug){ 
          formatMacAddressToStr(managerMac, macStr, 18);
          Serial.printf("Reinit espnow Manager Mac is now : %s\n", macStr);
      }
      uint8_t primary=0;
      wifi_second_chan_t secondary;
      esp_wifi_set_promiscuous(true);
      esp_wifi_get_channel(&primary,&secondary);
      if((channel != primary) || (channel != secondary)){
          esp_wifi_set_channel(channel,secondary);
      }           
      esp_wifi_set_promiscuous(false);
      if(debug){
        Serial.printf("Reinit espnow Channel is now:%d\n", channel);
        WiFi.printDiag(Serial);
      } 
    }   //else {                                 // do stuff only if not already done            
                      // Set up ESP-Now link 
      WiFi.mode(WIFI_STA);          // Station mode for esp-now sensor node
//      WiFi.disconnect();
      WiFi.macAddress(myOwnMac);    // set mac add in myOwnMac

      if(debug) {
        Serial.printf("My HW mac: %s", WiFi.macAddress().c_str());
        Serial.println("");
        Serial.printf("Sending to MAC: %02x:%02x:%02x:%02x:%02x:%02x", managerMac[0], managerMac[1], managerMac[2], managerMac[3], managerMac[4], managerMac[5]);
        Serial.printf(", on channel: %i\n", channel);
      }

      // Initialize ESP-now ----------------------------

      if (esp_now_init() == ESP_OK) {
        Callback<void(const uint8_t*,esp_now_send_status_t)>::func = std::bind(&EspNowClass::sentCallback, this, std::placeholders::_1, std::placeholders::_2);
        esp_now_send_cb_t funcSend = static_cast<esp_now_send_cb_t>(Callback<void(const uint8_t*,esp_now_send_status_t)>::callback);      
        esp_now_register_send_cb(funcSend);     // Once ESPNow is successfully Init, we will register for Send CB to get the status of Trasnmitted packet     

        Callback<void(const uint8_t*, const uint8_t*, int)>::func = std::bind(&EspNowClass::receiveCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        esp_now_recv_cb_t funcRecv = static_cast<esp_now_recv_cb_t>(Callback<void(const uint8_t*, const uint8_t*, int)>::callback);      
        esp_now_register_recv_cb(funcRecv);     // Once ESPNow is successfully Init, we will register for Send CB to get the status of Trasnmitted packet     

        registerPeer(broadcastAddress);                                         // register broadcast as a valid peer
        if(foundManager) registerPeer(managerMac);               // register manager
        espnowNotInitialized = false;
        if(debug) Serial.println("*** ESP_Now has now init sucessfully");

      } else {      // espnow init failed
        if(debug) Serial.println("*** ESP_Now init failed. Going to sleep");
      }
  }

  /*
   * void EspNowClass::sendClientHello
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void EspNowClass::sendClientHello(){
    if(!foundManager){                          // in initialisation phase need to send Client Hello msg
        initNodeMessage.sendingId = nbSendReties;
        sendData(broadcastAddress,initNodeMessage);
    } 
  }

  /*
   * void EspNowClass::sendRouteurData
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void EspNowClass::sendRouteurData(){
    if(foundManager){
      sendData(managerMac,routeurData);
    }  
  }

  /*
   * void EspNowClass::registerPeer
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void EspNowClass::registerPeer(const uint8_t peerMac[]){           //Register peer (manager for slave)
    if ( !esp_now_is_peer_exist(peerMac)) {     // Manager not paired, attempt pair       
        if(debug) Serial.print("Register Manager with status : ");       
        esp_now_peer_info_t gateway;
        memcpy(gateway.peer_addr, peerMac, 6);
        gateway.channel = channel;
        gateway.encrypt = false;            // no encryption
        if(esp_now_add_peer(&gateway) == 0) {  
          if(debug) Serial.println("Pair success");         
        } else {         
            if(debug) Serial.println("Could Not peer");         
        }     
    } else {
        if(debug) Serial.println("Peer Already registred");         
    }
  }

  void EspNowClass::setRouteurDatas(float GRIDCurrentPower, 
                            float LOADCurrentPower,
                            float PVCurrentPower,
                            boolean relayPAC,
                            time_t debutRelayPAC 
                            ){
    routeurData.GRIDCurrentPower = GRIDCurrentPower;
    routeurData.LOADCurrentPower = LOADCurrentPower;
    routeurData.PVCurrentPower = PVCurrentPower;
    routeurData.relayPAC = relayPAC;
    routeurData.debutRelayPAC = debutRelayPAC;
  }

  /*
   * bool EspNowClass::hasManager
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  bool EspNowClass::hasManager(){
    return foundManager;
  }

// private


  // callback for message sent out
  /*
   * void EspNowClass::sentCallback
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void EspNowClass::sentCallback(const uint8_t* macAddr, esp_now_send_status_t sendStatus) {
    char macStr[18];
    formatMacAddressToStr(macAddr, macStr, 18); // to get a printable string

    if (sendStatus == 0) {
      if(debug) Serial.println("Message recieved with status: Delivery Success" );
      nbSendReties = 0;
      messageStatus = SENTOK;
    } else {
      if(debug) Serial.printf("Last Message could not been sent to: %s, Delivery Fail\n", macStr );
      if (nbSendReties == maxSendRetries){    // can't reach manager any more restart init prcedure
        foundManager = false;
        messageStatus = SENTABORTED;
      } else {
        nbSendReties++;
        messageStatus = SENTNOACK;
      }
    }
  }

  // callback for message recieved
  /*
   * void EspNowClass::receiveCallback
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
  void EspNowClass::receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen){
    // only allow a maximum of 250 characters in the message + a null terminating byte

    int msgLen = ((ESP_NOW_MAX_DATA_LEN)<(dataLen) ? (ESP_NOW_MAX_DATA_LEN) : (dataLen));       // min(ESP_NOW_MAX_DATA_LEN, dataLen);
    char buffer[msgLen + 1];
    char macStr[18];        // format the mac address
    
    strncpy(buffer, (const char *)data, msgLen);
    buffer[msgLen] = 0;     // make sure we are null terminated
    if(debug) {
      formatMacAddressToStr(macAddr, macStr, 18);
      Serial.printf("Received message from: %s\n", macStr);
    }
    // what are our instructions
    switch(buffer[0]) {       // message type
      case SERVER_HELLO:
          initNodeMessage.sendingId = 0; // clear retries on initMessage
          if(compareMacAdd(initManagerMessage.macOfPeer,myOwnMac)){ // || compareMacAdd(dataMsg.macOfPeer,broadcastAddress)){     // got an error message for me need to reinitialize the process send Client_Hello. 
              memcpy(managerMac, macAddr, ESP_NOW_ETH_ALEN);     
              if(debug){ 
                Serial.printf("Received SERVER_HELLO Message addressed to me with channel :%d; current channel is %d\n", initManagerMessage.channel, channel);
                formatMacAddressToStr(managerMac, macStr, 18);
                Serial.printf("Manager Mac is now : %s\n", macStr);
              }
              if(channel != initManagerMessage.channel){
                channel = initManagerMessage.channel;
                uint8_t primary=0;
                wifi_second_chan_t secondary;
                esp_wifi_set_promiscuous(true);
                esp_wifi_get_channel(&primary,&secondary);
                if((channel != primary) || (channel != secondary)){
                    esp_wifi_set_channel(channel,secondary);
                }           
                esp_wifi_set_promiscuous(false);
              }
              registerPeer(macAddr);                          // register manager
              memcpy(&initManagerMessage, data, dataLen);     
              if(debug) Serial.println(F("ESP NOW connected to Manager server"));
              foundManager = true;
              messageStatus = DATARECEIVEDOK;
          }    
        break;
      case SYNCH_TIME: 
        synchTimeReqMessage.sendingId = 0; // clear retries on synchTimeReqMessage
        memcpy(&synchTimeMessage, data, dataLen);     
        setTime(synchTimeMessage.now);                                         // makeTime(&tm);         // return time_t from elements stored in tm struct
        Serial.printf("New time is now : %d/%d/%d %d:%d:%d \n", day(), month(), year(), hour(), minute(), second() );
        messageStatus = DATARECEIVEDOK;
        break;
      case ROUTEUR_REQ_MSG: 
        Serial.printf("Received routeur request from manager\n"); 
        sendRouteurData();
        messageStatus = DATARECEIVEDOK;
        break;

      case ERROR_MSG:
        memcpy(&errorMessage, data, dataLen);     
        if(compareMacAdd(errorMessage.macOfPeer,myOwnMac) || compareMacAdd(errorMessage.macOfPeer,broadcastAddress)){     // got an error message for me need to reinitialize the process send Client_Hello. 
          Serial.printf("Received an ERROR_MSG broadcasted or addressed to me with value : %s\n", errorMessage.message);
          esp_now_del_peer(managerMac);
          foundManager=false;
          messageStatus = SENTABORTED;    // will generate a NO MANAGER status
        }  
        break;
    }
  }






  /*
   * void EspNowClass::getMessageType
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void EspNowClass::getMessageType(uint8_t messId, char *messagetypeStr){
      switch (messId) {
        case DATA_MSG:
            strcpy(messagetypeStr, "DATA_MSG");
          break;
        case CLIENT_HELLO:
            strcpy(messagetypeStr, "CLIENT_HELLO");
          break;
        case SERVER_HELLO:
            strcpy(messagetypeStr, "SERVER_HELLO");
          break;
        case SYNCH_TIME_REQ:
            strcpy(messagetypeStr, "SYNCH_TIME_REQ");
          break;
        case SYNCH_TIME:
            strcpy(messagetypeStr, "SYNCH_TIME");
          break;
        case ERROR_MSG:
            strcpy(messagetypeStr, "ERROR_MSG");
          break;
        case PROGS_REQ_MSG:
            strcpy(messagetypeStr, "PROGS_REQ_MSG");
          break;
        case PROGS_DATA_MSG:
            strcpy(messagetypeStr, "PROGS_DATA_MSG");
          break;
        case SENSOR_DATA_MSG:
            strcpy(messagetypeStr, "SENSOR_DATA_MSG");
          break;
        
        default:
            strcpy(messagetypeStr, "Unknown_MSG");
          break;
      }
    }

  /*
   * uint8_t EspNowClass::getMessageStatus
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    uint8_t EspNowClass::getMessageStatus(){
      return messageStatus;
    }

  /*
   * template <typename T> void EspNowClass::sendData
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    template <typename T> void EspNowClass::sendData(uint8_t *peerAddress, const T message){
      uint8_t s_data[sizeof(message)]; 
      char msgTypStr[20];
      char macStr[18];

      formatMacAddressToStr(peerAddress, macStr, 18);
      memcpy(s_data, &message, sizeof(message));     
      if (esp_now_send(peerAddress, s_data, sizeof(s_data)) == 0) {
        getMessageType(*s_data,msgTypStr);
        Serial.printf("Message type %s send successfully to %s\n", msgTypStr,macStr);
      } else {
        Serial.println("Unknown error");
      }
    }

  /*
   * void EspNowClass::formatMacAddressToStr
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    void EspNowClass::formatMacAddressToStr(const uint8_t *macAddr, char *buffer, int maxLength) {
      snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    }

  /*
   * bool EspNowClass::compareMacAdd
   * But : (description automatique) — expliquer brièvement l'objectif de la fonction
   * Entrées : voir la signature de la fonction (paramètres)
   * Sortie : valeur de retour ou effet sur l'état interne
   */
    bool EspNowClass::compareMacAdd(uint8_t *mac1, uint8_t *mac2){
        bool isSame = true;
      for (int i=0; i<ESP_NOW_ETH_ALEN; i++){
        if(mac1[i] != mac2[i]){
          isSame = false;
          break;
        }
      }
      return isSame;
    }



