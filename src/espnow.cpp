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

  /**
 * @brief Constructeur vide classe EspNowClass (ESP-NOW communication)
 * 
 * @note EspNowClass::EspNowClass
 */
  EspNowClass::EspNowClass(){};
  /**
 * @brief Destructeur vide classe EspNowClass
 * 
 * @note EspNowClass::~EspNowClass
 */
  EspNowClass::~EspNowClass(){};

// public

  /**
 * @brief Initialise ESP-NOW : WiFi.mode(WIFI_STA), esp_now_init(), register callbacks send/recv, register peers
 * 
 * @note void EspNowClass::initESPNOW
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

  /**
 * @brief Envoie message CLIENT_HELLO en broadcast si !foundManager (init phase discovery)
 * 
 * @note void EspNowClass::sendClientHello
 */
  void EspNowClass::sendClientHello(){
    if(!foundManager){                          // in initialisation phase need to send Client Hello msg
        initNodeMessage.sendingId = nbSendReties;
        sendData(broadcastAddress,initNodeMessage);
    } 
  }

  /**
 * @brief Envoie structure routeurData (GRID/LOAD/PV power, relayPAC status) au manager si foundManager
 * 
 * @note void EspNowClass::sendRouteurData
 */
  void EspNowClass::sendRouteurData(){
    if(foundManager){
      sendData(managerMac,routeurData);
    }  
  }

  /**
 * @brief Ajoute peer ESP-NOW (manager ou broadcast) si pas déjà existant via esp_now_add_peer()
 * 
 * @note void EspNowClass::registerPeer
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

  /**
 * @brief Retourne bool foundManager (true si SERVER_HELLO reçu et peer enregistré)
 * 
 * @note bool EspNowClass::hasManager
 */
  bool EspNowClass::hasManager(){
    return foundManager;
  }

// private


  // callback for message sent out
  /**
 * @brief Callback ESP-NOW envoi : gère ACK (sendStatus=0) ou retry (max maxSendRetries, sinon foundManager=false)
 * 
 * @note void EspNowClass::sentCallback
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
  /**
 * @brief Callback ESP-NOW réception : parse buffer[0] (message type) et traite SERVER_HELLO, SYNCH_TIME, ROUTEUR_REQ_MSG, ERROR_MSG
 * 
 * @note void EspNowClass::receiveCallback
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






  /**
 * @brief Convertit uint8_t message ID en string descriptif (DATA_MSG, CLIENT_HELLO, SERVER_HELLO, etc.)
 * 
 * @note void EspNowClass::getMessageType
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

  /**
 * @brief Retourne uint8_t messageStatus (SENTOK, SENTNOACK, SENTABORTED, DATARECEIVEDOK)
 * 
 * @note uint8_t EspNowClass::getMessageStatus
 */
    uint8_t EspNowClass::getMessageStatus(){
      return messageStatus;
    }

  /**
 * @brief Template générique envoi struct via ESP-NOW : sérialise message en uint8_t[] et appelle esp_now_send()
 * 
 * @note template <typename T> void EspNowClass::sendData
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

  /**
 * @brief Convertit MAC address uint8_t[6] en string formaté 'XX:XX:XX:XX:XX:XX' via snprintf
 * 
 * @note void EspNowClass::formatMacAddressToStr
 */
    void EspNowClass::formatMacAddressToStr(const uint8_t *macAddr, char *buffer, int maxLength) {
      snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    }

  /**
 * @brief Compare deux adresses MAC uint8_t[6] byte par byte - retourne true si identiques
 * 
 * @note bool EspNowClass::compareMacAdd
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



