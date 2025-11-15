# Instructions Copilot - monRouteurSolaire

## Contexte du Projet

**monRouteurSolaire** est un routeur solaire intelligent basé sur ESP32 qui optimise l'autoconsommation en routant le surplus de production photovoltaïque vers un chauffe-eau résistif. Le système utilise un dimmer TRIAC 24A-600V pour moduler la puissance (0-100%) et intègre un écran OLED, 4 sondes DS18B20, et une interface web AsyncWebServer.

## Architecture Technique

### Hardware
- **MCU :** ESP32 DEVKIT V1 (dual-core 240MHz, 320KB RAM, 4MB Flash)
- **Dimmer :** TRIAC 24A-600V avec détection zero-crossing (pins 18/19)
- **Affichage :** Écran OLED SSD1306 128x64 I2C (U8g2)
- **Capteurs :** 4x DS18B20 OneWire (températures eau/air/PAC/interne)
- **Communication :** WiFi (AsyncWebServer), ESP-NOW optionnel (PAC)

### Software
- **Plateforme :** ESP32 Arduino framework
- **Serveur Web :** ESPAsyncWebServer avec mDNS (`routeurSol.local`)
- **API :** SolarEdge API HTTPS pour récupération production temps réel
- **Multi-threading :** FreeRTOS 2 cores (core0: dimmer/sensors, core1: web/API)

## Structure des Fichiers

### Code Principal
```
src/
├── main.cpp                    # Point d'entrée (2003 lignes)
│                               # - Setup: WiFi, NTP, OLED, DS18B20, TRIAC, API
│                               # - Loop: core0 (dimmer), core1 (web/sensors)
├── routeurWeb.cpp              # Serveur web AsyncWebServer (1349 lignes)
│                               # - Routes API (/api/status, /api/command)
│                               # - SSE (routeurEvents, routeurParamsEvents)
│                               # - Authentification sessions
└── espnow.cpp                  # Communication ESP-NOW (284 lignes)
                                # - Protocole sans fil (optionnel)
```

### Headers
```
include/
├── globalRouteur.h             # Constantes, structures config
├── routeurWeb.h                # Classe RouteurSolWebClass
├── espnow.h                    # Classe EspNowClass, codes messages
├── solaredge.h                 # Certificat SSL API SolarEdge
└── Triac.h                     # Classe Triac (dimmer control)
```

### Frontend
```
html/                           # Sources HTML/CSS/JS
data/                           # Fichiers compilés Gulp (LittleFS)
gulpfile.js                     # Build frontend (minify, gzip)
package.json                    # Dépendances Node.js
```

## Commandes Essentielles

### Build & Upload
```bash
# Compilation (ESP32)
~/.platformio/penv/bin/platformio run --environment esp32doit-devkit-v1

# Upload firmware
~/.platformio/penv/bin/platformio run --target upload --environment esp32doit-devkit-v1

# Upload filesystem (LittleFS)
~/.platformio/penv/bin/platformio run --target uploadfs --environment esp32doit-devkit-v1

# Moniteur série
~/.platformio/penv/bin/platformio device monitor --environment esp32doit-devkit-v1
```

### Frontend
```bash
# Build HTML/CSS/JS (Gulp)
npx gulp

# Install dépendances
npm install
```

### Tests
```bash
# Validation build
~/.platformio/penv/bin/platformio run --environment esp32doit-devkit-v1

# Test upload port
~/.platformio/penv/bin/platformio device list
```

## Points Clés du Code

### 1. Dimmer TRIAC (Triac.h/cpp)
- **Zero-crossing :** Détection passage par zéro (pin 18) toutes les 10ms (50Hz)
- **Timer hardware :** Interruption 100µs pour pulse TRIAC précis
- **Puissance :** 0-100% par découpage d'onde sinusoïdale
- **Méthodes IRAM :** `currentNull()`, `onTimer()` pour performance temps réel

### 2. Multi-threading ESP32 (main.cpp)
```cpp
// Core 0 : Traitement dimmer et acquisition capteurs (priorité temps réel)
void setup() {
    xTaskCreatePinnedToCore(coreTask, "coreTask", 10000, NULL, 1, &Task1, 0);
}

// Core 1 : Web server, API, NTP (loop principal)
void loop() {
    // Gestion web, SSE, API SolarEdge
}
```

### 3. API SolarEdge (main.cpp ~ligne 800-900)
- **Endpoint :** `https://monitoringapi.solaredge.com/site/{siteId}/currentPowerFlow.json`
- **Authentification :** API key dans `config.solarEdge`
- **Certificat :** `solarEdgeCertificate` (solaredge.h)
- **Parsing JSON :** ArduinoJson v7 pour extraction `PV.currentPower`

### 4. Configuration Persistante (globalRouteur.h)
```cpp
struct configuartion_t {
    char adminPassword[11];
    users_t users[5];
    wifi_t wifi[3];
    char solarEdge[50];         // API key
    int volumeBallon;           // Volume ballon (litres)
    int puissanceBallon;        // Puissance résistance (W)
    char heureBackup[10];       // HH:MM marche forcée
    int tempEauMin;             // Temp min marche forcée (°C)
    boolean secondBackup;       // Marche forcée secondaire
    char heureSecondBackup[10];
};
```

### 5. Routes API REST (routeurWeb.cpp ~ligne 200-800)
| Endpoint | Méthode | Description |
|----------|---------|-------------|
| `/api/status` | GET | État routeur (puissance, températures, production) |
| `/api/command` | POST | Commande dimmer (power, mode) |
| `/api/config` | GET/POST | Configuration système |
| `/api/wifi` | POST | Configuration WiFi |
| `/routeurEvents` | SSE | Événements temps réel (puissance, temps) |
| `/routeurParamsEvents` | SSE | Mises à jour paramètres |

### 6. Capteurs DS18B20 (main.cpp ~ligne 400-500)
```cpp
OneWire oneWire(pinDS18B20);
DallasTemperature sensors(&oneWire);

// 4 sondes avec adresses DeviceAddress
DeviceAddress tempDeviceAddressCuve;    // Eau ballon
DeviceAddress tempDeviceAddressExt;     // Air extérieur
DeviceAddress tempDeviceAddressPAC;     // PAC
DeviceAddress tempDeviceAddressInt;     // Interne ESP32
```

## Configuration WiFi

### Premier Démarrage (WiFiManager)
1. ESP32 crée AP : `RouteurSolaire-XXXXXX`
2. Connexion depuis smartphone → `http://192.168.4.1`
3. Sélection réseau WiFi + mot de passe
4. ESP32 redémarre et se connecte

### Accès Interface Web
- **mDNS :** `http://routeurSol.local`
- **IP directe :** Affichée sur OLED ou moniteur série

## Dépendances Importantes

### PlatformIO (platformio.ini)
```ini
lib_deps = 
    ESPAsyncWiFiManager @ ^0.31
    AsyncTCP @ ^3.3.5
    ESPAsyncWebServer @ ^3.7.1
    NTPClient @ ^3.2.1
    Dimmable Light for Arduino @ ^2.0.1
    DallasTemperature @ ^4.0.4
    U8g2 @ ^2.36.5
    ArduinoJson @ ^7.3.0
    AceButton @ ^1.10.1
```

### Node.js (package.json)
```json
{
  "devDependencies": {
    "gulp": "^4.0.2",
    "gulp-htmlmin": "^5.0.1",
    "gulp-gzip": "^1.4.2"
  }
}
```

## Troubleshooting

### Problème : Build failed (TRIAC)
**Cause :** Library `Dimmable Light for Arduino` manquante  
**Solution :** `~/.platformio/penv/bin/platformio lib install "Dimmable Light for Arduino@^2.0.1"`

### Problème : OLED ne s'affiche pas
**Vérifications :**
1. Adresse I2C correcte (0x3C par défaut) → Scanner I2C
2. Pins SDA/SCL connectés (GPIO 21/22)
3. Alimentation 3.3V stable

### Problème : API SolarEdge timeout
**Checks :**
1. API key valide dans `config.solarEdge`
2. Certificat SSL à jour (`solaredge.h`)
3. Connexion WiFi stable
4. Firewall autorise HTTPS sortant

### Problème : Dimmer ne module pas
**Diagnostics :**
1. Zero-crossing détecté ? → Moniteur série `[TRIAC] ZC detected`
2. Pin 18 connecté au ZC detector
3. Pin 19 connecté au gate TRIAC
4. Alimentation TRIAC isolée

### Problème : ESP32 reboot intempestif
**Causes fréquentes :**
1. Watchdog timeout → Augmenter délais dans loop
2. Stack overflow → Augmenter taille stack tasks
3. Alimentation insuffisante → 500mA min

## Bonnes Pratiques

### 1. Modifications Code
- **Interruptions IRAM :** Fonctions `currentNull()` et `onTimer()` doivent rester dans IRAM
- **Zero-crossing :** Ne pas ajouter de `delay()` ou `Serial.print()` dans ISR
- **Multi-core :** Variables partagées core0/core1 → utiliser `mutex` ou `volatile`

### 2. Sécurité
- **Isolation galvanique :** TRIAC isolé de l'ESP32 (optocoupleur)
- **Température :** Surveiller temp interne ESP32 (> 80°C = throttling)
- **Puissance max :** Dimmer 24A @ 230V = 5.5kW max

### 3. Performance
- **SSE :** Limiter fréquence envoi events (> 1 event/sec = lag web)
- **API SolarEdge :** Polling max 1x/5min (rate limit API)
- **OLED :** Rafraîchir écran max 1x/sec (évite flickering)

### 4. Débogage
```cpp
// Activer debug dans globalRouteur.h
const bool debug = true;

// Logs séries
Serial.printf("[TRIAC] Power: %d%%\n", power);
Serial.printf("[SOLAREDGE] Production: %d W\n", pvPower);
Serial.printf("[TEMP] Eau: %.1f°C, Air: %.1f°C\n", tempEau, tempAir);
```

## Mise à Jour Firmware

### OTA (Over-The-Air)
```cpp
// TODO: Implémenter ArduinoOTA
#include <ArduinoOTA.h>

void setup() {
    ArduinoOTA.setHostname("routeurSol");
    ArduinoOTA.begin();
}

void loop() {
    ArduinoOTA.handle();
}
```

### Upload Série
```bash
# Via USB (port /dev/cu.SLAB_USBtoUART)
~/.platformio/penv/bin/platformio run --target upload --upload-port /dev/cu.SLAB_USBtoUART
```

## Ressources Externes

### Documentation Libraries
- **ESPAsyncWebServer :** https://github.com/me-no-dev/ESPAsyncWebServer
- **U8g2 OLED :** https://github.com/olikraus/u8g2/wiki
- **DallasTemperature :** https://github.com/milesburton/Arduino-Temperature-Control-Library
- **ArduinoJson v7 :** https://arduinojson.org/

### API SolarEdge
- **API Docs :** https://www.solaredge.com/sites/default/files/se_monitoring_api.pdf
- **Portal :** https://monitoring.solaredge.com

### PlatformIO
- **Docs :** https://docs.platformio.org/
- **ESP32 Platform :** https://github.com/pioarduino/platform-espressif32

## Notes Importantes

1. **Sécurité Électrique :** Ne jamais modifier le circuit TRIAC sous tension
2. **Calibration Dimmer :** Ajuster `dimPulseBegin` / `dimPulseEnd` selon charge
3. **Backup Config :** Exporter config via `/api/config` avant reflash
4. **Version Arduino :** Framework 3.1.3 (compatible ESP-IDF 5.3.0)

---

**Auteur :** Ludovic Sorriaux  
**Projet :** Routeur Solaire Intelligent  
**Repository :** https://github.com/LudovicSorriaux/monRouteurSolaire
