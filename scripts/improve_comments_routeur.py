#!/usr/bin/env python3
"""
Script d'amélioration des commentaires génériques dans monRouteurSolaire
Remplace "But : (description automatique)" par descriptions explicites
"""

import re
from pathlib import Path

# Mapping fonction -> description explicite
DESCRIPTIONS = {
    # main.cpp
    "detectsMovementRising": "ISR détection front montant du capteur PIR - enregistre candidat HIGH pour détection mouvement",
    "detectsMovementFalling": "ISR détection front descendant du capteur PIR - enregistre candidat LOW pour fin mouvement",
    "decodeWiFiEvent": "Convertit enum WiFiEvent_t (ESP32) en chaîne descriptive pour logging",
    "WiFiDebugEvent": "Callback debug WiFi ESP32 - affiche code événement et description sur Serial",
    "WiFiStationConnected": "Callback ESP32 connexion WiFi réussie - affiche IP/MAC/channel et active auto-reconnect",
    "WiFiGotIP": "Callback ESP32 attribution IP - log adresse obtenue depuis info.got_ip.ip_info.ip.addr",
    "WiFiStationDisconnected": "Callback ESP32 déconnexion WiFi - log raison (info.wifi_sta_disconnected.reason) et met à jour WifiStatus",
    "wl_status_to_string": "Convertit enum wl_status_t en string (WL_CONNECTED, WL_NO_SSID_AVAIL, etc.)",
    "findPassword": "Recherche mot de passe WiFi dans config.wifi[] par SSID - retourne password ou nullptr",
    "WiFiConnect": "Tente connexion WiFi avec SSID/password - timeout 10s - retourne true si WL_CONNECTED",
    "ConnectWithStoredCredentials": "Scanne réseaux disponibles et tente connexion avec chaque SSID trouvé dans config.wifi[] - 4 retries",
    "ConnectWithConfigFileCredentials": "Teste connexion séquentielle avec tous les SSID de config.wifi[] sans scanner - ordre déclaration",
    "startWiFi": "Gère séquence connexion WiFi complète : reconnect si déjà connecté une fois, sinon cascade config/scan/stored",
    "getSolarEdgeValues": "Parse JSON SolarEdge (solarEdgePayload) pour extraire GRID/LOAD/PV power et direction flux (from/to)",
    "getSolarEdgeInfos": "Appel HTTPS GET vers API SolarEdge currentPowerFlow - stocke réponse JSON dans solarEdgePayload",
    "http_status_to_string": "Convertit code HTTP (200-503 + codes erreur HTTPClient -1 à -13) en string descriptif",
    "startSPIFFS": "Monte système fichiers LittleFS et liste récursif de tous les fichiers via listAllFilesInDir('/',0)",
    "listAllFilesInDir": "Liste récursive dossiers/fichiers LittleFS avec indentation (deep) et taille formatée",
    "printConfiguration": "Affiche struct config complète sur Serial : adminPassword, users[], wifi[], tous les params routeur",
    "loadConfiguration": "Charge /cfg/routeur.cfg (JSON) vers struct config - parse users[], wifi[], solarEdge, parametres{}",
    "saveConfiguration": "Sérialise struct config vers JSON et écrit dans /cfg/routeur.cfg (LittleFS)",
    "saveNewConfiguration": "Met à jour config avec nouveaux adminPassword/user/wifi puis appelle saveConfiguration()",
    "resetWifiSettingsInConfig": "Efface tous les SSID/passwords dans config.wifi[] (strcpy vide) puis sauvegarde",
    "formatBytes": "Convertit taille (bytes) en String formaté : B/KB/MB selon seuil 1024",
    "setup": "Init ESP32 : Serial, OLED, LittleFS, config, DS18B20, TRIAC, WiFi, NTP, WebServer, 2 tasks (cores 0/1)",
    "Task1code": "Core 0 : gestion TRIAC dimmer, lecture SolarEdge, régulation surplus PV, contrôle PAC, marche forcée",
    "Task2code": "Core 1 : gestion écran OLED, PIR calibration, WiFi reconnect, NTP updates, timers secondes/minutes/heures",
    "loop": "Fonction loop() Arduino vide - tout géré par Task1code (core0) et Task2code (core1)",
    "dstOffset": "Calcule offset DST (heure d'été France) : +3600s si entre dernier dimanche mars 3h et dernier dimanche octobre 3h",
    "getNTPTime": "Synchronise horloge ESP32 via NTP (europe.pool.ntp.org) + ajuste TZ_OFFSET (GMT+1) et DST",
    "calculDureeJour": "Calcule timestamps jour (lever) et nuit (coucher) selon table journee[12][4] indexée par mois",
    "marcheForceeSwitch": "Active/désactive marche forcée chauffe-eau manuel : TRIAC ON 100% si true, OFF si false",
    "marcheForceePACSwitch": "Active/désactive PAC manuel : relay ON si true, OFF si false, met à jour debutRelayPAC",
    "setRelayPac": "Contrôle GPIO RelayPAC (HIGH/LOW) si config.pacPresent=true",
    "handleButtonEvent": "Callback AceButton pour bouton chauffe-eau : kEventPressed allume écran, kEventReleased toggle marcheForcee",
    "changePIRmode": "Active/désactive capteur PIR : si true -> initPIRphase (calibration 60s), si false -> detachInterrupt",
    "gestEcran": "Met à jour écran OLED U8g2 selon action : entete, full, mForcee, modeManu, horloge, values, httpError",
    "stopEcran": "Éteint écran OLED via u8g2.setPowerSave(1) et positionne oled=false",
    "gestWeb": "Appelle routeurWeb.OnUpdate() pour envoyer events SSE (routeurEvents/routeurParamsEvents)",
    
    # routeurWeb.cpp
    "RouteurSolWebClass::RouteurSolWebClass": "Constructeur classe RouteurSolWebClass - affiche 'init monRouteurWeb' sur Serial",
    "RouteurSolWebClass::startup": "Démarre AsyncWebServer (port 80) via startServer() + mDNS responder via startMDNS()",
    "RouteurSolWebClass::OnUpdate": "Wrapper appelé depuis gestWeb() : updateRouteurData() + updateRouteurParamsData() pour SSE",
    "RouteurSolWebClass::startMDNS": "Démarre mDNS responder (nom 'routeur.local') et enregistre service http/_tcp/80",
    "RouteurSolWebClass::startServer": "Configure toutes les routes AsyncWebServer : HTML, API, SSE, serveStatic, onNotFound",
    "RouteurSolWebClass::printActiveSessions": "Debug : affiche tableau activeSessions[] (sessID, ttl, timecreated) avec sessID fourni en param",
    "RouteurSolWebClass::isSessionValid": "Vérifie validité session (sessID) : expire sessions (now>timecreated+ttl), cherche sessID dans activeSessions[]",
    "RouteurSolWebClass::handleRoot": "Route GET '/' : tente servir /index.html depuis LittleFS via handleFileRead(), sinon 404",
    "RouteurSolWebClass::handleOtherFiles": "Handler onNotFound : tente servir request->url() depuis LittleFS, sinon 404",
    "RouteurSolWebClass::handleNotFound": "Handler 404 complet : log méthode HTTP, headers, params, envoie '400: Invalid Request'",
    "RouteurSolWebClass::handleNotFound2": "Handler 404 alternatif (legacy commenté) : même logique que handleNotFound mais avec auth commentée",
    "RouteurSolWebClass::handleLogin": "Route POST /logon : vérifie user/password dans config.users[], génère sessionID (15 chars) + ttl 1h/24h",
    "RouteurSolWebClass::handleRegister": "Route POST '/register' : crée nouvel utilisateur si adminPassword OK et slot libre dans config.users[]",
    "RouteurSolWebClass::handleChangAdminPW": "Route POST '/adminPasswd' : change config.adminPassword si oldadminpasswd valide",
    "RouteurSolWebClass::handleGetUsers": "Route POST '/getUsers' : retourne JsonArray avec tous les usernames de config.users[]",
    "RouteurSolWebClass::handleDeleteUsers": "Route POST '/deleteUsers' : efface users cochés (checkboxes user0..userN) si adminPassword OK",
    "RouteurSolWebClass::handleFileRead": "Tente servir fichier depuis LittleFS : check .lgz (gzip), détermine contentType, envoie AsyncWebServerResponse",
    "RouteurSolWebClass::handleFileError": "Envoie 404 : tente servir Full404.html depuis LittleFS, sinon page HTML 404 inline (Tailwind CSS)",
    "RouteurSolWebClass::handleFileList": "Liste fichiers LittleFS racine en JSON [{type:'dir'/'file', name, size},...] (route non utilisée?)",
    "RouteurSolWebClass::checkSessionParam": "Vérifie paramètre 'sess' (POST) : extrait sessionID et appelle isSessionValid()",
    "RouteurSolWebClass::handleSetRouteurSSEData": "Route POST '/setHomeSSEData' : force envoi SSE routeurEvents via updateRouteurData()",
    "RouteurSolWebClass::updateRouteurData": "Envoie event SSE 'routeurData' avec JSON power/status si routeurEvents.count()>0",
    "RouteurSolWebClass::handleSetSwitches": "Route POST '/setSwitches' : toggle chauffeEau/PAC switches après vérif session",
    "RouteurSolWebClass::handleSetParamsSSEData": "Route POST '/setParamsSSEData' : force envoi SSE routeurParamsEvents via updateRouteurParamsData()",
    "RouteurSolWebClass::updateRouteurParamsData": "Envoie event SSE 'routeurParamsData' avec JSON config (15 params) si routeurParamsEvents.count()>0",
    "RouteurSolWebClass::handleSetParams": "Route POST '/setRouteurParams' : met à jour config params (15 champs) après vérif session, sauvegarde si changement",
    "RouteurSolWebClass::handleChangePassword": "Route GET '/setpw' : change adminPassword si adminpassword param OK (REST API legacy)",
    "RouteurSolWebClass::handleGetRouteurStatus": "Route GET '/getrouteurstatus' : retourne JSON complet status routeur si pw=adminPassword (REST API)",
    "RouteurSolWebClass::handleSetRouteurSwitch": "Route GET '/setrouteurswitch' : toggle chauffe-eau ou PAC si pw OK (REST API)",
    "RouteurSolWebClass::showJsonConfig": "Route ANY '/jsonConfig' : retourne config complet en JSON pretty print (debug)",
    "RouteurSolWebClass::formatBytes": "Convertit taille bytes en String B/KB/MB (copie identique à formatBytes dans main.cpp)",
    "RouteurSolWebClass::getContentType": "Détermine MIME type depuis extension fichier : .html, .css, .js, .ico, .gz",
    "RouteurSolWebClass::generateKey": "Génère sessionID aléatoire (15 chars alphanumériques) et stocke dans activeSessions[] avec ttl",
    
    # espnow.cpp
    "EspNowClass::EspNowClass": "Constructeur vide classe EspNowClass (ESP-NOW communication)",
    "EspNowClass::~EspNowClass": "Destructeur vide classe EspNowClass",
    "EspNowClass::initESPNOW": "Initialise ESP-NOW : WiFi.mode(WIFI_STA), esp_now_init(), register callbacks send/recv, register peers",
    "EspNowClass::sendClientHello": "Envoie message CLIENT_HELLO en broadcast si !foundManager (init phase discovery)",
    "EspNowClass::sendRouteurData": "Envoie structure routeurData (GRID/LOAD/PV power, relayPAC status) au manager si foundManager",
    "EspNowClass::registerPeer": "Ajoute peer ESP-NOW (manager ou broadcast) si pas déjà existant via esp_now_add_peer()",
    "EspNowClass::hasManager": "Retourne bool foundManager (true si SERVER_HELLO reçu et peer enregistré)",
    "EspNowClass::sentCallback": "Callback ESP-NOW envoi : gère ACK (sendStatus=0) ou retry (max maxSendRetries, sinon foundManager=false)",
    "EspNowClass::receiveCallback": "Callback ESP-NOW réception : parse buffer[0] (message type) et traite SERVER_HELLO, SYNCH_TIME, ROUTEUR_REQ_MSG, ERROR_MSG",
    "EspNowClass::getMessageType": "Convertit uint8_t message ID en string descriptif (DATA_MSG, CLIENT_HELLO, SERVER_HELLO, etc.)",
    "EspNowClass::getMessageStatus": "Retourne uint8_t messageStatus (SENTOK, SENTNOACK, SENTABORTED, DATARECEIVEDOK)",
    "EspNowClass::sendData": "Template générique envoi struct via ESP-NOW : sérialise message en uint8_t[] et appelle esp_now_send()",
    "EspNowClass::formatMacAddressToStr": "Convertit MAC address uint8_t[6] en string formaté 'XX:XX:XX:XX:XX:XX' via snprintf",
    "EspNowClass::compareMacAdd": "Compare deux adresses MAC uint8_t[6] byte par byte - retourne true si identiques"
}

def improve_file(filepath: Path) -> bool:
    """Améliore les commentaires d'un fichier"""
    print(f"\n📄 Traitement : {filepath.name}")
    
    content = filepath.read_text(encoding='utf-8')
    original_content = content
    
    replacements = 0
    
    # Pattern pour trouver les commentaires génériques
    pattern = r'(/\*\n\s+\*\s+([^\n]+)\n\s+\*\s+But\s*:\s*\(description automatique\)[^\n]*\n\s+\*\s+Entrées[^\n]*\n\s+\*\s+Sortie[^\n]*\n\s+\*/)'
    
    matches = list(re.finditer(pattern, content))
    
    for match in matches:
        full_comment = match.group(1)
        func_signature = match.group(2).strip()
        
        # Extraire nom fonction (format "Type Class::fonction" ou "void fonction")
        func_match = re.search(r'(?:::)?([A-Za-z0-9_~]+)\s*$', func_signature)
        if not func_match:
            continue
            
        func_name = func_match.group(1)
        
        # Chercher description
        full_key = func_signature  # Try exact match first
        description = DESCRIPTIONS.get(full_key)
        
        if not description:
            # Try just function name
            description = DESCRIPTIONS.get(func_name)
        
        if not description:
            # Try Class::function format
            class_match = re.search(r'([A-Za-z0-9_]+)::([A-Za-z0-9_~]+)', func_signature)
            if class_match:
                class_func_key = f"{class_match.group(1)}::{class_match.group(2)}"
                description = DESCRIPTIONS.get(class_func_key)
        
        if description:
            # Construire nouveau commentaire explicite
            new_comment = f"""/**
 * @brief {description}
 * 
 * @note {func_signature}
 */"""
            
            content = content.replace(full_comment, new_comment)
            replacements += 1
            print(f"  ✓ {func_name}")
        else:
            print(f"  ⚠ Pas de description pour : {func_signature}")
    
    if replacements > 0:
        filepath.write_text(content, encoding='utf-8')
        print(f"✅ {replacements} commentaires améliorés")
        return True
    else:
        print("ℹ️  Aucun changement")
        return False

def main():
    project_root = Path(__file__).parent.parent
    src_dir = project_root / "src"
    
    files = [
        src_dir / "main.cpp",
        src_dir / "routeurWeb.cpp",
        src_dir / "espnow.cpp"
    ]
    
    print("=" * 60)
    print("🚀 Amélioration commentaires - monRouteurSolaire")
    print("=" * 60)
    
    success_count = 0
    for filepath in files:
        if filepath.exists():
            if improve_file(filepath):
                success_count += 1
        else:
            print(f"\n⚠️  Fichier non trouvé: {filepath}")
    
    print("\n" + "=" * 60)
    print(f"✨ Terminé : {success_count}/{len(files)} fichiers améliorés")
    print("=" * 60)

if __name__ == "__main__":
    main()
