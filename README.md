# monRouteurSolaire v1.0

## 📄 Description

Routeur solaire intelligent ESP32 pour optimisation autoconsommation photovoltaïque. Route le surplus de production vers un chauffe-eau résistif avec modulation TRIAC 0-100%. Interface web temps réel, écran OLED, 4 capteurs température DS18B20, récupération production SolarEdge API.

**Plateforme :** ESP32 DEVKIT V1  
**Framework :** Arduino  
**Version :** 1.0.0

---

## 🎯 Fonctionnalités

- **Routage intelligent** surplus solaire → chauffe-eau résistif
- **Dimmer TRIAC 24A-600V** modulation puissance 0-100% (découpage d'onde)
- **Serveur web asynchrone** (ESPAsyncWebServer, mDNS `routeurSol.local`)
- **API REST complète** (`/api/status`, `/api/command`, `/api/config`)
- **Server-Sent Events (SSE)** mises à jour temps réel
- **SolarEdge API** récupération production PV HTTPS
- **Écran OLED** SSD1306 128x64 (U8g2) - affichage status
- **4x DS18B20** températures eau/air/PAC/interne
- **Multi-threading** FreeRTOS 2 cores ESP32 (dimmer core0, web core1)
- **WiFi Manager** configuration sans fil (ESPAsyncWiFiManager)
- **NTP** synchronisation temps automatique
- **Authentification** sessions utilisateurs avec cookies
- **Marche forcée** programmable (heures creuses)
- **ESP-NOW** optionnel communication PAC sans fil

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                  Navigateur Web / Mobile                         │
│           (http://routeurSol.local ou IP ESP32)                  │
└────────────────┬────────────────────────────────────────────────┘
                 │ HTTP/REST API + SSE
                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                      ESP32 DEVKIT V1                             │
│              (Dual-Core 240MHz, 320KB RAM, 4MB Flash)            │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┤
│  │  Core 1 (loop principal)                                    │
│  │  - AsyncWebServer (port 80)                                 │
│  │  - Routes API REST (/api/*)                                 │
│  │  - SSE (/routeurEvents, /routeurParamsEvents)               │
│  │  - NTPClient synchronisation                                │
│  │  - SolarEdge API polling (5 min)                            │
│  │  - Écran OLED update (1 sec)                                │
│  └─────────────────────────────────────────────────────────────┤
│  │  Core 0 (coreTask - temps réel)                             │
│  │  - TRIAC dimmer control (interruptions 100µs)               │
│  │  - Zero-crossing detection (10ms)                           │
│  │  - DS18B20 acquisition (1 sec)                              │
│  │  - Calcul puissance à router                                │
│  └─────────────────────────────────────────────────────────────┘
│                  │                         │                     │
└──────────────────┼─────────────────────────┼─────────────────────┘
         GPIO 18/19│              OneWire    │ I2C (21/22)
                   ▼                         ▼                     ▼
    ┌────────────────────┐    ┌───────────────────┐   ┌──────────────┐
    │  Dimmer TRIAC      │    │  4x DS18B20       │   │  OLED SSD1306│
    │  24A - 600V        │    │  - Eau ballon     │   │  128x64 I2C  │
    │  Zero-cross detect │    │  - Air extérieur  │   │              │
    │  (pin 18)          │    │  - PAC            │   │  Status +    │
    │  Gate pulse        │    │  - Interne ESP32  │   │  Puissance   │
    │  (pin 19)          │    │                   │   │              │
    └─────────┬──────────┘    └───────────────────┘   └──────────────┘
              │
              ▼
    ┌─────────────────────┐
    │  Chauffe-eau        │
    │  Résistif           │
    │  (1500W - 3000W)    │
    │                     │
    │  Routage surplus    │
    │  production solaire │
    └─────────────────────┘

              ▲
              │ Production (HTTPS API)
    ┌─────────────────────┐
    │  SolarEdge API      │
    │  monitoring.        │
    │  solaredge.com      │
    │                     │
    │  currentPowerFlow   │
    └─────────────────────┘
```

---

## 📂 Structure du Projet

```
routeur solaire/
├── src/
│   ├── main.cpp                     # Point d'entrée (2003 lignes)
│   │                                # Setup: WiFi, NTP, OLED, TRIAC, API
│   │                                # Loop: core1 (web), core0 (dimmer)
│   ├── routeurWeb.cpp               # Serveur web AsyncWebServer (1349 lignes)
│   │                                # Routes API, SSE, authentification
│   └── espnow.cpp                   # Communication ESP-NOW (284 lignes)
│
├── include/
│   ├── globalRouteur.h              # Constantes, structures config
│   ├── routeurWeb.h                 # Classe RouteurSolWebClass
│   ├── espnow.h                     # Classe EspNowClass (optionnel)
│   ├── solaredge.h                  # Certificat SSL API SolarEdge
│   └── Triac.h                      # Classe Triac (dimmer control)
│
├── html/                            # Sources HTML/CSS/JS
│   ├── index.html
│   ├── css/
│   └── js/
│
├── data/                            # Fichiers compilés Gulp (LittleFS)
│   ├── 404.html.lgz
│   ├── index.html.lgz
│   └── ...
│
├── scripts/
│   └── add_comments_src.py          # Script auto commentaires fonctions
│
├── platformio.ini                   # Configuration PlatformIO
├── gulpfile.js                      # Build frontend (minify, gzip)
├── package.json                     # Dépendances Node.js
├── copilote-instructions.md         # Guide développeur complet
└── README.md                        # Ce fichier
```

---

## 🔌 Matériel Requis

### Composants Essentiels

| Composant          | Référence           | Usage                           |
|--------------------|---------------------|---------------------------------|
| ESP32 DEVKIT V1    | Dual-core 240MHz    | Microcontrôleur principal       |
| Dimmer TRIAC       | 24A 600V AC         | Modulation puissance            |
| Zero-cross Detector| Optocoupleur        | Détection passage par zéro      |
| OLED SSD1306       | 128x64 I2C (0x3C)   | Affichage status                |
| DS18B20 (x4)       | OneWire             | Capteurs température            |
| Résistances 4.7kΩ  | Pull-up OneWire     | Bus OneWire                     |

### Pinout ESP32

| Pin GPIO | Fonction          | Description                          |
|----------|-------------------|--------------------------------------|
| 18       | TRIAC_ZC          | Zero-crossing detector (INPUT)       |
| 19       | TRIAC_GATE        | Gate TRIAC pulse (OUTPUT)            |
| 4        | DS18B20_DATA      | Bus OneWire (4x capteurs)            |
| 21       | I2C_SDA           | OLED SSD1306                         |
| 22       | I2C_SCL           | OLED SSD1306                         |

**⚠️ ATTENTION SÉCURITÉ :** Le dimmer TRIAC manipule du 230V AC. **Isolation galvanique obligatoire** entre ESP32 et TRIAC (optocoupleur). Ne jamais modifier le câblage sous tension.

---

## 🔧 Configuration

### `platformio.ini`

```ini
[env:esp32doit-devkit-v1]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200

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

### `globalRouteur.h` - Paramètres Clés

```cpp
// Configuration système
#define MAX_USERS 5                    // Nombre max utilisateurs
#define MAX_WIFI 3                     // Nombre max réseaux WiFi
#define TZ_OFFSET 1                    // Fuseau horaire (GMT+1)

// Configuration routeur (dans struct configuartion_t)
char solarEdge[50];                    // API key SolarEdge
int volumeBallon;                      // Volume ballon (litres)
int puissanceBallon;                   // Puissance résistance (W)
char heureBackup[10];                  // HH:MM marche forcée
int tempEauMin;                        // Température min (°C)
boolean secondBackup;                  // Marche forcée secondaire
char heureSecondBackup[10];            // HH:MM seconde marche forcée
```

### Endpoints API REST

| Endpoint              | Méthode | Description                          |
|-----------------------|---------|--------------------------------------|
| `/api/status`         | GET     | État routeur (JSON)                  |
| `/api/command`        | POST    | Commande dimmer (power, mode)        |
| `/api/config`         | GET/POST| Configuration système                |
| `/api/wifi`           | POST    | Configuration WiFi                   |
| `/routeurEvents`      | SSE     | Mises à jour temps réel              |
| `/routeurParamsEvents`| SSE     | Événements paramètres                |

---

## 🚀 Utilisation

### 1️⃣ Installation Dépendances Frontend

```bash
cd "/Users/ludovic1/Documents/PlatformIO/Projects/routeur solaire"
npm install
```

### 2️⃣ Build Frontend (Gulp)

```bash
# Minify et gzip des fichiers HTML/CSS/JS
npx gulp

# Les fichiers sont générés dans data/ (.lgz)
```

### 3️⃣ Compilation Firmware

```bash
~/.platformio/penv/bin/platformio run --environment esp32doit-devkit-v1
```

**Résultats attendus :**
- RAM usage ~17% (55KB / 320KB)
- Flash usage ~67% (1.4MB / 2MB)

### 4️⃣ Upload Firmware

```bash
~/.platformio/penv/bin/platformio run --target upload --environment esp32doit-devkit-v1
```

### 5️⃣ Upload Filesystem (LittleFS)

```bash
~/.platformio/penv/bin/platformio run --target uploadfs --environment esp32doit-devkit-v1
```

**Contenu uploadé :** fichiers `data/*.lgz` (HTML/CSS/JS compressés)

### 6️⃣ Moniteur Série

```bash
~/.platformio/penv/bin/platformio device monitor --environment esp32doit-devkit-v1
```

---

## 🌐 Premier Démarrage

### Configuration WiFi (WiFiManager)

1. **Brancher l'ESP32** via USB
2. **Uploader firmware + filesystem** (voir ci-dessus)
3. **Redémarrer l'ESP32**
4. **L'ESP32 crée un point d'accès :** `RouteurSolaire-XXXXXX`
5. **Se connecter depuis smartphone/PC**
6. **Naviguer vers** `http://192.168.4.1`
7. **Sélectionner réseau WiFi** et entrer mot de passe
8. **Valider** → ESP32 redémarre et se connecte au réseau

### Configuration SolarEdge API

1. **Obtenir API key :**
   - Connexion https://monitoring.solaredge.com
   - Admin → API Access → Generate API Key
   - Copier la clé (ex: `8INR9G7TVYP03QAMRMNKJYRNN0MTVJSQ`)

2. **Configurer dans l'interface web :**
   - `/api/config` → `solarEdge: "VOTRE_CLE_API"`
   - Ou directement dans `globalRouteur.h` avant compilation

### Configuration Ballon

Via interface web `/api/config` :

```json
{
  "volumeBallon": 150,              // Volume en litres
  "puissanceBallon": 1500,          // Puissance résistance en W
  "tempEauMin": 50,                 // Température minimale °C
  "heureBackup": "20:00",           // Marche forcée heures creuses
  "secondBackup": false,            // Marche forcée secondaire
  "heureSecondBackup": "00:00"
}
```

### Accès Interface Web

- **Via mDNS :** `http://routeurSol.local` (macOS/Linux/iOS)
- **Via IP :** Affichée sur OLED ou moniteur série (ex: `http://192.168.1.42`)

---

## 📊 Fonctionnement Routeur

### Algorithme Principal (main.cpp core0)

```cpp
// Récupération production solaire (API SolarEdge toutes les 5 min)
int productionPV = getSolarEdgeProduction();

// Récupération consommation maison (compteur ou estimation)
int consommationMaison = getConsommation();

// Calcul surplus disponible
int surplus = productionPV - consommationMaison;

// Si surplus > 0 → router vers chauffe-eau
if (surplus > 0) {
    // Calcul puissance à injecter (0-100%)
    int puissancePourcent = (surplus * 100) / puissanceBallon;
    puissancePourcent = constrain(puissancePourcent, 0, 100);
    
    // Commande dimmer TRIAC
    triac.setPower(puissancePourcent);
}

// Si température eau < tempEauMin ET heure = heureBackup
// → Marche forcée 100%
if (tempEau < config.tempEauMin && now() == heureBackup) {
    triac.setPower(100);
}
```

### Modulation TRIAC (Triac.cpp)

Le dimmer TRIAC utilise la technique de **découpage d'onde sinusoïdale** :

1. **Détection zero-crossing :** Interruption GPIO 18 toutes les 10ms (50Hz)
2. **Timer hardware :** Interruption 100µs pour pulse précis
3. **Pulse gate :** Impulsion courte GPIO 19 pour amorcer TRIAC
4. **Puissance 0% :** Pas de pulse (TRIAC bloqué)
5. **Puissance 50% :** Pulse à 5ms (milieu demi-onde)
6. **Puissance 100% :** Pulse immédiat après zero-crossing

**Avantages :**
- Modulation précise 0-100%
- Pas de harmoniques basses fréquences
- Compatible charges résistives
- Rendement ~99%

---

## 🔍 Monitoring

### Écran OLED (SSD1306)

Affichage temps réel :

```
┌──────────────────┐
│ RouteurSol v1.0  │
│ IP: 192.168.1.42 │
├──────────────────┤
│ Prod:  2500 W    │
│ Conso: 1200 W    │
│ Route: 1300 W    │
│ Dimmer:  87%     │
├──────────────────┤
│ Eau:   55.2°C    │
│ Air:   18.5°C    │
│ PAC:   22.1°C    │
└──────────────────┘
```

### API REST `/api/status`

```json
{
  "production": 2500,
  "consommation": 1200,
  "surplus": 1300,
  "dimmerPower": 87,
  "temperatures": {
    "eau": 55.2,
    "air": 18.5,
    "pac": 22.1,
    "interne": 45.3
  },
  "wifi": {
    "ssid": "MaisonWiFi",
    "rssi": -45
  },
  "uptime": 86400
}
```

### Server-Sent Events (SSE)

Connexion temps réel pour mises à jour instantanées :

```javascript
// JavaScript côté client
const evtSource = new EventSource('/routeurEvents');

evtSource.addEventListener('power', (event) => {
    const data = JSON.parse(event.data);
    console.log('Puissance routée:', data.power, 'W');
});
```

---

## 🛠️ Troubleshooting

### Problème : WiFi ne se connecte pas

**Solution :**
1. Vérifier LED ESP32 clignote (mode WiFiManager)
2. Chercher réseau `RouteurSolaire-XXXXXX`
3. Si bloqué : presser bouton RESET physique
4. Moniteur série : `[WiFi] Connecting...`

### Problème : Dimmer ne module pas

**Diagnostics :**
1. Zero-crossing détecté ?
   - Moniteur série : `[TRIAC] ZC detected`
   - Si non : vérifier pin 18 → optocoupleur ZC
2. Gate pulse envoyé ?
   - Oscilloscope sur pin 19 → pulse ~100µs
3. TRIAC amorcé ?
   - Multimètre sur charge : tension modulée
4. Charge résistive ?
   - Charges inductives/capacitives non supportées

### Problème : OLED ne s'affiche pas

**Vérifications :**
1. Adresse I2C correcte :
   ```bash
   # Scanner I2C
   ~/.platformio/penv/bin/platformio device monitor
   # Doit afficher: "I2C device found at 0x3C"
   ```
2. Pins SDA/SCL (GPIO 21/22) connectés
3. Alimentation 3.3V stable (> 100mA)
4. Contraste OLED réglé

### Problème : API SolarEdge timeout

**Checks :**
1. API key valide (`config.solarEdge`)
2. Certificat SSL à jour (`solaredge.h`)
3. WiFi stable (RSSI > -70 dBm)
4. Firewall autorise HTTPS sortant (port 443)
5. Moniteur série :
   ```
   [API] GET https://monitoringapi.solaredge.com/...
   [API] Response: 200 OK
   ```

### Problème : Température DS18B20 -127°C

**Causes :**
1. Sonde déconnectée
2. Résistance pull-up 4.7kΩ manquante
3. Bus OneWire trop long (> 10m)
4. Alimentation insuffisante (parasite mode)

**Solution :**
```cpp
// Vérifier adresses sondes
sensors.begin();
Serial.printf("Found %d DS18B20 devices\n", sensors.getDeviceCount());
```

### Problème : ESP32 reboot aléatoire

**Causes fréquentes :**
1. **Watchdog timeout :**
   ```cpp
   // Dans loop core0
   vTaskDelay(pdMS_TO_TICKS(10)); // Éviter boucle infinie
   ```
2. **Stack overflow :**
   ```cpp
   // Augmenter taille stack
   xTaskCreatePinnedToCore(coreTask, "core0", 20000, NULL, 1, &Task1, 0);
   //                                          ^^^^^ 10000 → 20000
   ```
3. **Alimentation insuffisante :** 500mA min (1A recommandé)
4. **Température interne > 80°C :** Améliorer ventilation

---

## 📈 Performances

### Mémoire

| Ressource       | Usage Typique | Max Sécurisé |
|-----------------|---------------|--------------|
| RAM             | ~17% (55KB)   | 70%          |
| Flash           | ~67% (1.4MB)  | 90%          |
| Heap libre      | ~200KB        | > 100KB      |

### Réseau

- **WiFi :** 802.11 b/g/n (2.4 GHz)
- **Connexions simultanées :** 4-6 (AsyncWebServer)
- **SSE clients max :** 4
- **Latence API :** < 100ms

### Dimmer TRIAC

- **Précision modulation :** ±1% (100 steps)
- **Fréquence zero-crossing :** 100Hz (détection chaque demi-onde)
- **Temps réponse :** < 10ms (demi-onde)
- **Puissance max :** 24A @ 230V = 5.5kW

### Capteurs

- **Précision DS18B20 :** ±0.5°C (-10°C à +85°C)
- **Fréquence acquisition :** 1 mesure/sec (750ms conversion)
- **Résolution :** 12 bits (0.0625°C)

---

## 🔒 Sécurité

### Électrique

⚠️ **DANGER HAUTE TENSION 230V AC**

1. **Isolation galvanique obligatoire** ESP32 ↔ TRIAC
2. **Optocoupleur** zero-crossing et gate pulse
3. **Boîtier isolé** ESP32 (IP44 min)
4. **Disjoncteur différentiel** 30mA sur circuit
5. **Câblage normalisé** (section 2.5mm² pour 16A)

### Informatique

```cpp
// Authentification sessions
struct sessions {
    char sessID[16];
    time_t ttl;
    time_t timecreated;
};

// Hash password (à implémenter)
// TODO: bcrypt ou SHA256 pour stockage passwords
```

**Recommandations :**
- Changer mot de passe admin par défaut
- Limiter accès WiFi (réseau local isolé)
- Activer HTTPS si exposition internet (certificat Let's Encrypt)
- Mettre à jour firmware régulièrement

---

## 📦 Dépendances

### Libraries PlatformIO

```ini
ESPAsyncWiFiManager @ ^0.31       # WiFi Manager
AsyncTCP @ ^3.3.5                 # TCP asynchrone ESP32
ESPAsyncWebServer @ ^3.7.1        # Serveur web async
NTPClient @ ^3.2.1                # Synchronisation temps
Dimmable Light for Arduino @ ^2.0.1  # Dimmer TRIAC
DallasTemperature @ ^4.0.4        # DS18B20
U8g2 @ ^2.36.5                    # OLED SSD1306
ArduinoJson @ ^7.3.0              # Parsing JSON
AceButton @ ^1.10.1               # Gestion boutons
```

### Node.js (Frontend Build)

```json
{
  "devDependencies": {
    "gulp": "^4.0.2",
    "gulp-htmlmin": "^5.0.1",
    "gulp-gzip": "^1.4.2"
  }
}
```

---

## 📝 Historique Versions

| Version | Date    | Modifications                                |
|---------|---------|----------------------------------------------|
| 1.0.0   | 11/2024 | Documentation complète, headers, commentaires|
| 0.9     | 10/2024 | Ajout ESP-NOW, refonte multi-threading       |
| 0.8     | 09/2024 | API SolarEdge HTTPS, certificat SSL          |
| 0.7     | 08/2024 | Dimmer TRIAC, zero-crossing, interruptions   |
| 0.6     | 07/2024 | AsyncWebServer, SSE, interface web           |
| 0.5     | 06/2024 | DS18B20 x4, OLED SSD1306                     |

---

## 👤 Auteur

**Ludovic Sorriaux**  
Projet : Routeur Solaire Intelligent (ESP32 + TRIAC)

---

## 📜 Licence

Projet privé - Tous droits réservés

---

## 🔗 Liens Utiles

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [U8g2 OLED](https://github.com/olikraus/u8g2/wiki)
- [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [SolarEdge API](https://www.solaredge.com/sites/default/files/se_monitoring_api.pdf)
- [Dimmable Light for Arduino](https://github.com/fabianoriccardi/dimmable-light)

---

## 📞 Support

En cas de problème :
1. Consulter section Troubleshooting ci-dessus
2. Vérifier logs moniteur série
3. Tester avec `debug = true` dans `globalRouteur.h`
4. Vérifier isolation TRIAC (sécurité électrique)

**⚡ RAPPEL SÉCURITÉ : Ne jamais intervenir sur le circuit TRIAC sous tension. Couper le disjoncteur avant toute manipulation.**
