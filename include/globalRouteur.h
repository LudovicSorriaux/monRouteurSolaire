/*******************************************************************************
 * @file    globalRouteur.h
 * @brief   Constantes globales et structures routeur solaire
 * @details Définitions MAX_USERS, MAX_WIFI, structures configuration (users,
 *          wifi, SolarEdge API, paramètres ballon). Instances globales
 *          routeurWeb, server, SSE events.
 * 
 * Usage   : Configuration système, structures partagées
 * Référencé par : TOUS les modules
 * Référence     : Arduino.h, TimeLib
 * 
 * @author  Ludovic Sorriaux
 * @date    2024
 *******************************************************************************/

#include <Arduino.h>
#include <TimeLib.h>

#define MAX_USERS 5                      // max nb of users
#define MAX_WIFI 3                       // max nb of wifi configs
#define MAX_NAME_SIZE 32
#define MAX_USERNAME_SIZE 11

// telecom
#define ESP8266_BAUD 115200

// NTP
#define MaxNTPRetries 15

// Classes
   class Adafruit_SSD1306;
   class RouteurSolWebClass;
   class AsyncWebServer;
   class AsyncEventSource;


// Instances
   extern RouteurSolWebClass routeurWeb;                           // gestion sur serveur web              
   extern AsyncWebServer server; 
   extern AsyncEventSource routeurEvents;
   extern AsyncEventSource routeurParamsEvents;
 
    // global structures definitions 
#ifndef monRouteurStructures
#define monRouteurStructures
  typedef struct users_t {
    char user[MAX_USERNAME_SIZE];
    char user_passwd[MAX_USERNAME_SIZE];
  } struct_users;

  typedef struct wifi_t {
    char ssid[MAX_NAME_SIZE];
    char ssid_passwd[64];
  } struct_wifi;

  typedef struct configuartion_t {
    char adminPassword[MAX_USERNAME_SIZE];
    users_t users[MAX_USERS];
    wifi_t wifi[MAX_WIFI];
    char solarEdge[50];   //  8INR9G7TVYP03QAMRMNKJYRNN0MTVJSQ",   ; get api key from solaredge portal in solarEdgeApiKey var
    int volumeBallon;     //  150,         ; volume du ballon en litres in volumeBallon var
    int puissanceBallon;  //  1500,      ; puissance de la resistance du chauffe-eau in resistance var
    char heureBackup[10];  //  "20:00",      ; heure:minute début marche forcée in heureOnMarcheForcee;minuteOnMarcheForcee vars
    int tempEauMin;       //   50,            ; réglage de la température minimale de l'eau en marche forcée in temperatureEauMin var
    boolean secondBackup; //   false,       ; si marche forcee secondaire dans la journee in marcheForceeSec var
    char heureSecondBackup[10];  //  20:00", ; heure début marche forcée sec in heureOnMarcheForceeSec:minuteOnMarcheForceeSec var
    boolean sondeTemp;    //  false,          ; si sonde DS18B20 in sondeTemp var
    boolean pacPresent;   //  true,          ; si relay pour la PAC in relayPAC var
    int puissancePac;     //  1000,         ; Puissance de la PAC in puissancePAC var
    int puissPacOn;       //  1000,          ; puissance du surplus pour déclencher le relay in relayPACOn var
    int puissPacOff;      //  800,          ; puissance du surplus pour stopper le relay in relayPACOff var
    int tempsOverProd;    //  900,        ; 900=15*60 ou DEBUG 1*60 : temps en sec d'overproduction avant de passer en PAC on si assez de puissance (relayOn) in tempsOverP var
    int tempsMinPac;      //  7200,         ; 7200=2*3600 ou DEBUG 2*60 : si PAC en Marche au moins 2h de marche même si pas assez de puissance in tempsMinPAC var
    boolean afficheur;    //  true,           ; on-off de l'ecran in oled var
    boolean motionSensor; //  true         ; si module de detection in motionSensor var
  } struct_configuration;

  typedef struct marcheForcee_t{
    bool marcheForceeDone = true;
    uint8_t heureOnMarcheForcee;
    uint8_t minuteOnMarcheForcee;
  } structMarcheForcee;

#endif      // end structures monRouteurStructures

// functions called outside from main
    extern void saveConfiguration();
    extern void marcheForceePACSwitch(boolean value);
    extern void marcheForceeSwitch(boolean value);
    extern void changePIRmode(bool val);
    extern void stopEcran();

// variables defined in main 
   extern const bool debug;
   extern struct_configuration config;
   extern structMarcheForcee plageMarcheForcee[2];

   extern bool modeManuEau;
   extern bool modeManuPAC;

   extern float GRIDCurrentPower;
   extern float LOADCurrentPower;
   extern float PVCurrentPower;
   extern char from[];
   extern char to[];

   extern float currentPower;                           // currentPower = (PVCurrentPower - LOADCurrentPower)*1000; //Kw en W si > 0 on produit trop, si < 0 on ne produit pas assez
   extern float valTriac;
   extern time_t debutRelayPAC;
   extern boolean marcheForcee;
   extern float temperatureEau;
   extern time_t finMarcheForcee;                     // temps de fin de marche forcee ballon (now + temps chauffe)





