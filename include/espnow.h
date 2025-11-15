/*******************************************************************************
 * @file    espnow.h
 * @brief   Communication ESP-NOW (expérimental)
 * @details Protocole ESP-NOW pour communication sans fil entre ESP8266/ESP32.
 *          Codes messages (DATA_MSG, CLIENT_HELLO, etc.), états transmission,
 *          gestion canal WiFi (channel 1 obligatoire).
 * 
 * Usage   : Communication sans fil optionnelle (alternative/complément web)
 * Référencé par : main.cpp (si espnowPAC activé)
 * Référence     : espnow.h (ESP8266), esp_now.h (ESP32)
 * 
 * @author  Ludovic Sorriaux
 * @date    2024
 *******************************************************************************/

#include <Arduino.h>
#include <TimeLib.h>

// Different Wifi and ESP-Now implementations for ESP8266 and ESP32
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#else
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#endif

#define WIFI_CHANNEL        1     // Must be 1. (!IMPORTANT)
                                  // ESP-Now can work on other channels, but the receiving ESP Gateway must operate on
                                  // channel 1 in order to also work for TCP/IP WiFi.
                                  // It has been reported to work also for other Channels, but I have only succeeded on ch 1.



#define ESP_NOW_ETH_ALEN 6              /* Length of ESPNOW peer MAC address */
// #define ESP_NOW_MAX_DATA_LEN 250        /* Maximum length of ESPNOW data which is sent very time */


#define DATA_MSG 0
#define CLIENT_HELLO 1
#define SERVER_HELLO 2
#define SYNCH_TIME_REQ 3
#define SYNCH_TIME 4
#define ERROR_MSG 5
#define PROGS_REQ_MSG 6
#define PROGS_DATA_MSG 7
#define SENSOR_DATA_MSG 8
#define ROUTEUR_REQ_MSG 9
#define ROUTEUR_DATA_MSG 10
#define CLIENT_SONDE_HELLO 20
#define CLIENT_ROUTEUR_HELLO 21
#define NO_MSG 255

#define NOSENT 0             // not yet sent do nothing;
#define NOSENTACK 10         // not yet received do nothing;
#define SENTNOACK 20         // sent but not delivered
#define SENTOK 30            // sent and delivered : process next step (wait for datareceive);
#define DATARECEIVEDOK 40    // process next step
#define SENTABORTED 50       // max retries can't find manager gotosleep or re-initialise;


    extern const bool debug;

 /* -------------   Structures  -------------*/
    //Datas structures
    typedef struct struct_routeur_Data{
      float GRIDCurrentPower = 0.0;
      float LOADCurrentPower = 0.0;
      float PVCurrentPower = 0.0;
      time_t debutRelayPAC = 0; 
      boolean relayPAC = false;
      boolean routeurPresent = true;
    } routeur_Data;

    //Messages structures
    typedef struct message {
        uint8_t messID = DATA_MSG;
        uint8_t macOfPeer[ESP_NOW_ETH_ALEN];
        uint8_t boardID;
        char jsonBuff[ESP_NOW_MAX_DATA_LEN-(3+ESP_NOW_ETH_ALEN)];
        uint8_t sendingId = 0;
    } struct_message;
    typedef struct initNode {
      uint8_t messID = CLIENT_ROUTEUR_HELLO;
      uint8_t sendingId = 0;
    } initdataMsg;
    typedef struct initManagerMsg {
      uint8_t messID = SERVER_HELLO;
      uint8_t macOfPeer[ESP_NOW_ETH_ALEN];
      uint8_t sendingId = 0;
      int8_t channel = 1;
    } initManagerData;
    typedef struct synchTimeReqMsg {
      uint8_t messID = SYNCH_TIME_REQ;
      int8_t boardID;
      uint8_t sendingId = 0;
    } synchTimeReqData;
    typedef struct synchTimeMsg {
      uint8_t messID = SYNCH_TIME;
      time_t now;
      uint8_t sendingId = 0;
    } synchTimeData;
    typedef struct routeurReqMsg {
      uint8_t messID = ROUTEUR_REQ_MSG;
      uint8_t sendingId = 0;
    } routeurReqData;
    typedef struct routeurMsg {
      uint8_t messID = ROUTEUR_DATA_MSG;
      bool newDatas;
      routeur_Data routeurDatas;
      uint8_t sendingId = 0;
    } routeurDataMsg;

    typedef struct errorMsg {
      uint8_t messID = ERROR_MSG;
      uint8_t macOfPeer[ESP_NOW_ETH_ALEN];
      char message[100];
      uint8_t sendingId;
    } errorData;



template <typename T>
struct Callback;

template <typename Ret, typename... Params>
struct Callback<Ret(Params...)> {
   template <typename... Args> 
   static Ret callback(Args... args) {                    
//      return func(args...);  
   }
   static std::function<Ret(Params...)> func; 
};

template <typename Ret, typename... Params>
std::function<Ret(Params...)> Callback<Ret(Params...)>::func;

class EspNowClass {
    public :

        ~EspNowClass(void);
        EspNowClass(void);         
        
        void initESPNOW();
        void sendClientHello();
        void sendRouteurData();
        void registerPeer(const uint8_t peerMac[]);
        void setRouteurDatas(float GRIDCurrentPower, 
                                    float LOADCurrentPower,
                                    float PVCurrentPower,
                                    boolean relayPAC,
                                    time_t debutRelayPAC 
                                   );
        bool hasManager();

    private :
      uint8_t managerMac[ESP_NOW_ETH_ALEN] = {0x02, 0x10, 0x11, 0x12, 0x13, 0x14};
                                  // MAC Address of the remote ESP Gateway we send to.
                                  // This is the "system address" all Sensors send to and the Gateway listens on.
                                  // (The ESP Gateway will set this "soft" MAC address in its HW. See Gateway code for info.)
      uint8_t broadcastAddress[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};    //MAC Address to broadcast 
      uint8_t myOwnMac[ESP_NOW_ETH_ALEN];                     // my mac address
      int8_t channel = WIFI_CHANNEL;                          // WiFi Channel

      bool foundManager = false;
      bool espnowNotInitialized = true;
      volatile boolean callbackCalled = false;

      message dataMsg;
      initdataMsg initNodeMessage;
      initManagerData initManagerMessage;
      synchTimeData synchTimeMessage;
      synchTimeReqData synchTimeReqMessage;
      errorData errorMessage;
      routeurReqData routeurReqMessage;
      routeurDataMsg routeurMessage;

      routeur_Data routeurData;

      uint8_t messageStatus = NOSENT;      // callback by espnow sent and received callbacks
      uint8_t nbSendReties = 0;            // control nb send failures to detect problem 
      const uint8_t maxSendRetries = 3;    // can't reach manager any more restart init prcedure
      uint8_t lastMessageSent = NO_MSG;    // to ack by sent callback 

        void sentCallback(const uint8_t* macAddr, esp_now_send_status_t sendStatus); // callback for message sent out
        void receiveCallback(const uint8_t *mac_addr, const uint8_t *data, int data_len);// callback for message recieved

        void getMessageType(uint8_t messId, char *messagetypeStr);
        uint8_t getMessageStatus();
        template <typename T> void sendData(uint8_t *peerAddress, const T message);
        void formatMacAddressToStr(const uint8_t *macAddr, char *buffer, int maxLength);
        bool compareMacAdd(uint8_t *mac1, uint8_t *mac2);
    
};

