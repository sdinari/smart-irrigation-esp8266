#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <time.h>

// Structure pour un programme d'arrosage
struct IrrigationProgram {
  bool enabled;
  bool days[7]; // D=0, L=1, M=2, M=3, J=4, V=5, S=6 (Dimanche à Samedi)
  int hour;
  int minute;
  int duration; // en minutes
  char name[32];
};

// Structure pour stocker la configuration complète
struct Config {
  char ssid[32];
  char password[64];
  char timezone[32]; // ex: "GMT0" pour Maroc/Casablanca
  IrrigationProgram programs[8]; // Maximum 8 programmes
  int programCount;
  bool configured;
  int relayPin; // Pin pour contrôler le relais
};

Config config;
String dayNames[] = {"D", "L", "M", "M", "J", "V", "S"};
String dayFullNames[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};

// Serveur web sur le port 80
ESP8266WebServer server(80);

// Serveur DNS pour le portail captif
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Variables de fonctionnement
bool configMode = false;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 60000; // 1 minute
bool irrigationActive = false;
unsigned long irrigationStartTime = 0;
int currentProgramDuration = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDémarrage du système d'irrigation ESP8266...");
  
  // Initialiser l'EEPROM
  EEPROM.begin(1024);
  
  // Charger la configuration depuis l'EEPROM
  loadConfig();
  
  // Configurer le pin du relais
  pinMode(config.relayPin, OUTPUT);
  digitalWrite(config.relayPin, LOW); // Relais éteint au démarrage
  
  // Vérifier si la configuration existe
  if (!config.configured) {
    Serial.println("Configuration non trouvée, démarrage en mode configuration");
    startConfigMode();
  } else {
    Serial.println("Configuration trouvée, démarrage en mode normal");
    startNormalMode();
  }
}

void loop() {
  if (configMode) {
    // Gérer les requêtes DNS en mode configuration
    dnsServer.processNextRequest();
    // Gérer les requêtes web
    server.handleClient();
  } else {
    // Mode normal - vérifier les programmes d'arrosage
    unsigned long currentMillis = millis();
    
    // Vérifier si l'arrosage en cours doit s'arrêter
    if (irrigationActive) {
      if (currentMillis - irrigationStartTime >= (currentProgramDuration * 60000UL)) {
        stopIrrigation();
      }
    }
    
    // Vérifier les programmes toutes les minutes
    if (currentMillis - lastCheckTime >= checkInterval) {
      lastCheckTime = currentMillis;
      checkIrrigationSchedule();
    }
    
    // Vérifier les commandes série
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      if (command == "reset") {
        Serial.println("Réinitialisation de la configuration");
        resetConfig();
      } else if (command == "start") {
        Serial.println("Démarrage manuel de l'arrosage (5 min)");
        startIrrigation(5, "Manuel");
      } else if (command == "stop") {
        Serial.println("Arrêt manuel de l'arrosage");
        stopIrrigation();
      }
    }
    
    // Gérer les requêtes web
    server.handleClient();
  }
}

void startConfigMode() {
  configMode = true;
  
  // Créer un point d'accès
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266-Irrigation");
  Serial.print("Point d'accès créé. IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Configuration du serveur DNS
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Configuration des routes du serveur web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/programs", HTTP_GET, handlePrograms);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/saveProgram", HTTP_POST, handleSaveProgram);
  server.on("/deleteProgram", HTTP_GET, handleDeleteProgram);
  server.onNotFound(handleRoot);
  
  // Démarrer le serveur web
  server.begin();
  Serial.println("Serveur web démarré");
}

void startNormalMode() {
  configMode = false;
  
  // Se connecter au WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  
  Serial.print("Connexion à ");
  Serial.print(config.ssid);
  
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnecté au WiFi");
    Serial.print("Adresse IP: ");
    Serial.println(WiFi.localIP());
    
    // Configuration du temps avec la timezone définie
    setenv("TZ", config.timezone, 1);
    tzset();
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Synchronisation de l'heure...");
    
    // Configuration des routes du serveur web
    server.on("/", HTTP_GET, handleStatus);
    server.on("/config", HTTP_GET, handleRoot);
    server.on("/programs", HTTP_GET, handlePrograms);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/saveProgram", HTTP_POST, handleSaveProgram);
    server.on("/deleteProgram", HTTP_GET, handleDeleteProgram);
    server.on("/manual", HTTP_GET, handleManual);
    server.on("/reset", HTTP_GET, []() {
      server.send(200, "text/html", "<html><body><h1>Reset</h1><p>Configuration réinitialisée.</p></body></html>");
      resetConfig();
    });
    
    server.begin();
    checkIrrigationSchedule();
  } else {
    Serial.println("\nÉchec de connexion WiFi. Mode configuration.");
    startConfigMode();
  }
}

void checkIrrigationSchedule() {
  if (irrigationActive) return; // Ne pas démarrer si déjà en cours
  
  // Utiliser directement la timezone configurée
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  // Utiliser directement tm_wday (0=Dimanche, 1=Lundi, ..., 6=Samedi)
  int currentDay = timeinfo->tm_wday;
  int currentHour = timeinfo->tm_hour;
  int currentMin = timeinfo->tm_min;
  
  Serial.printf("Vérification: %s %02d:%02d\n", dayNames[currentDay].c_str(), currentHour, currentMin);
  
  for (int i = 0; i < config.programCount; i++) {
    IrrigationProgram& prog = config.programs[i];
    
    if (prog.enabled && prog.days[currentDay] && 
        prog.hour == currentHour && prog.minute == currentMin) {
      
      Serial.printf("Démarrage programme: %s (%d min)\n", prog.name, prog.duration);
      startIrrigation(prog.duration, prog.name);
      break;
    }
  }
}

void startIrrigation(int duration, String programName) {
  if (irrigationActive) return;
  
  irrigationActive = true;
  irrigationStartTime = millis();
  currentProgramDuration = duration;
  
  digitalWrite(config.relayPin, HIGH);
  Serial.printf("ARROSAGE DÉMARRÉ - Programme: %s, Durée: %d min\n", programName.c_str(), duration);
}

void stopIrrigation() {
  if (!irrigationActive) return;
  
  irrigationActive = false;
  digitalWrite(config.relayPin, LOW);
  Serial.println("ARROSAGE ARRÊTÉ");
}

void loadConfig() {
  EEPROM.get(0, config);
  
  if (config.configured != true) {
    // Initialiser avec des valeurs par défaut
    strcpy(config.ssid, "");
    strcpy(config.password, "");
    strcpy(config.timezone, "GMT-2"); // Maroc/Casablanca  
    config.programCount = 0;
    config.configured = false;
    config.relayPin = 4; // Pin D2 sur NodeMCU
    
    // Initialiser les programmes
    for (int i = 0; i < 8; i++) {
      config.programs[i].enabled = false;
      for (int j = 0; j < 7; j++) {
        config.programs[i].days[j] = false;
      }
      config.programs[i].hour = 6;
      config.programs[i].minute = 0;
      config.programs[i].duration = 10;
      sprintf(config.programs[i].name, "Programme %d", i + 1);
    }
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void resetConfig() {
  config.configured = false;
  saveConfig();
  delay(1000);
  ESP.restart();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuration Irrigation</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
  html += "label { display: block; margin-top: 15px; font-weight: bold; }";
  html += "input[type='text'], input[type='password'], input[type='number'] { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #ddd; border-radius: 5px; }";
  html += "button { background: #4CAF50; color: white; padding: 12px 20px; margin: 10px 5px; border: none; border-radius: 5px; cursor: pointer; }";
  html += "button:hover { background: #45a049; }";
  html += ".nav-button { background: #2196F3; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>🌱 Configuration Irrigation</h1>";
  html += "<form action='/save' method='POST'>";
  
  html += "<label>SSID WiFi:</label>";
  html += "<input type='text' name='ssid' value='" + String(config.ssid) + "' required>";
  
  html += "<label>Mot de passe WiFi:</label>";
  html += "<input type='password' name='password' value='" + String(config.password) + "'>";
  
  html += "<label>Pin du relais:</label>";
  html += "<input type='number' name='relayPin' value='" + String(config.relayPin) + "' min='0' max='16'>";
  
  html += "<button type='submit'>Enregistrer WiFi</button>";
  html += "</form>";
  
  html += "<button class='nav-button' onclick=\"window.location.href='/programs'\">Gérer les Programmes</button>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handlePrograms() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Programmes d'Arrosage</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
  html += ".program { border: 1px solid #ddd; margin: 10px 0; padding: 15px; border-radius: 5px; background: #f9f9f9; }";
  html += ".days { display: flex; gap: 5px; margin: 10px 0; }";
  html += ".day { padding: 5px 10px; border: 1px solid #ddd; border-radius: 3px; cursor: pointer; }";
  html += ".day.selected { background: #4CAF50; color: white; }";
  html += "label { display: block; margin: 10px 0 5px 0; font-weight: bold; }";
  html += "input, select { padding: 8px; margin: 5px; border: 1px solid #ddd; border-radius: 3px; }";
  html += "button { background: #4CAF50; color: white; padding: 10px 15px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; }";
  html += ".delete { background: #f44336; }";
  html += "</style>";
  html += "<script>";
  html += "function toggleDay(progIndex, dayIndex) {";
  html += "  var checkbox = document.getElementById('day_' + progIndex + '_' + dayIndex);";
  html += "  var dayDiv = document.getElementById('dayDiv_' + progIndex + '_' + dayIndex);";
  html += "  checkbox.checked = !checkbox.checked;";
  html += "  dayDiv.className = checkbox.checked ? 'day selected' : 'day';";
  html += "}";
  html += "</script></head><body>";
  html += "<div class='container'>";
  html += "<h1>🕒 Programmes d'Arrosage</h1>";
  
  // Afficher les programmes existants
  for (int i = 0; i < config.programCount; i++) {
    IrrigationProgram& prog = config.programs[i];
    html += "<div class='program'>";
    html += "<form action='/saveProgram' method='POST'>";
    html += "<input type='hidden' name='index' value='" + String(i) + "'>";
    
    html += "<label>Nom du programme:</label>";
    html += "<input type='text' name='name' value='" + String(prog.name) + "'>";
    
    html += "<label>Jours de la semaine:</label>";
    html += "<div class='days'>";
    for (int j = 0; j < 7; j++) {
      html += "<div id='dayDiv_" + String(i) + "_" + String(j) + "' class='day";
      if (prog.days[j]) html += " selected";
      html += "' onclick='toggleDay(" + String(i) + "," + String(j) + ")'>" + dayNames[j] + "</div>";
      html += "<input type='checkbox' id='day_" + String(i) + "_" + String(j) + "' name='day" + String(j) + "' style='display:none;'";
      if (prog.days[j]) html += " checked";
      html += ">";
    }
    html += "</div>";
    
    html += "<label>Heure:</label>";
    html += "<input type='number' name='hour' value='" + String(prog.hour) + "' min='0' max='23'> h ";
    html += "<input type='number' name='minute' value='" + String(prog.minute) + "' min='0' max='59'> min";
    
    html += "<label>Durée (minutes):</label>";
    html += "<input type='number' name='duration' value='" + String(prog.duration) + "' min='1' max='240'>";
    
    html += "<label><input type='checkbox' name='enabled'";
    if (prog.enabled) html += " checked";
    html += "> Activer ce programme</label>";
    
    html += "<button type='submit'>Sauvegarder</button>";
    html += "<button type='button' class='delete' onclick=\"if(confirm('Supprimer ce programme?')) window.location.href='/deleteProgram?index=" + String(i) + "'\">Supprimer</button>";
    html += "</form></div>";
  }
  
  // Nouveau programme
  if (config.programCount < 8) {
    html += "<div class='program'>";
    html += "<h3>➕ Nouveau Programme</h3>";
    html += "<form action='/saveProgram' method='POST'>";
    html += "<input type='hidden' name='index' value='-1'>";
    
    html += "<label>Nom:</label>";
    html += "<input type='text' name='name' value='Nouveau programme'>";
    
    html += "<label>Jours:</label>";
    html += "<div class='days'>";
    for (int j = 0; j < 7; j++) {
      html += "<div id='dayDiv_new_" + String(j) + "' class='day' onclick='toggleDay(\"new\"," + String(j) + ")'>" + dayNames[j] + "</div>";
      html += "<input type='checkbox' id='day_new_" + String(j) + "' name='day" + String(j) + "' style='display:none;'>";
    }
    html += "</div>";
    
    html += "<label>Heure:</label>";
    html += "<input type='number' name='hour' value='6' min='0' max='23'> h ";
    html += "<input type='number' name='minute' value='0' min='0' max='59'> min";
    
    html += "<label>Durée (minutes):</label>";
    html += "<input type='number' name='duration' value='10' min='1' max='240'>";
    
    html += "<label><input type='checkbox' name='enabled' checked> Activer</label>";
    
    html += "<button type='submit'>Ajouter Programme</button>";
    html += "</form></div>";
  }
  
  html += "<button onclick=\"window.location.href='/'\">Retour</button>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>État Irrigation</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
  html += ".status { padding: 15px; margin: 10px 0; border-radius: 8px; text-align: center; font-size: 18px; }";
  html += ".active { background: #4CAF50; color: white; }";
  html += ".inactive { background: #f44336; color: white; }";
  html += ".info { margin: 10px 0; padding: 10px; background: #f9f9f9; border-radius: 5px; }";
  html += "button { background: #2196F3; color: white; padding: 12px 20px; margin: 10px 5px; border: none; border-radius: 5px; cursor: pointer; }";
  html += ".manual { background: #FF9800; }";
  html += "</style>";
  html += "<meta http-equiv='refresh' content='30'>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🌱 État du Système d'Irrigation</h1>";
  
  // État de l'arrosage
  html += "<div class='status " + String(irrigationActive ? "active" : "inactive") + "'>";
  if (irrigationActive) {
    int elapsed = (millis() - irrigationStartTime) / 60000;
    int remaining = currentProgramDuration - elapsed;
    html += "🚿 ARROSAGE EN COURS<br>Temps restant: " + String(remaining) + " min";
  } else {
    html += "💧 ARROSAGE ARRÊTÉ";
  }
  html += "</div>";
  
  // Informations système
  html += "<div class='info'><strong>WiFi:</strong> " + String(config.ssid) + "</div>";
  html += "<div class='info'><strong>IP:</strong> " + WiFi.localIP().toString() + "</div>";
  html += "<div class='info'><strong>Heure:</strong> " + String(timeinfo->tm_hour) + ":" + 
          (timeinfo->tm_min < 10 ? "0" : "") + String(timeinfo->tm_min) + "</div>";
  html += "<div class='info'><strong>Jour:</strong> " + dayFullNames[timeinfo->tm_wday] + "</div>";
  html += "<div class='info'><strong>Programmes actifs:</strong> ";
  
  int activeCount = 0;
  for (int i = 0; i < config.programCount; i++) {
    if (config.programs[i].enabled) activeCount++;
  }
  html += String(activeCount) + "/" + String(config.programCount) + "</div>";
  
  // Boutons de contrôle
  html += "<button onclick=\"window.location.href='/programs'\">Programmes</button>";
  html += "<button onclick=\"window.location.href='/config'\">Configuration</button>";
  html += "<button class='manual' onclick=\"window.location.href='/manual?action=start'\">Arrosage Manuel</button>";
  if (irrigationActive) {
    html += "<button class='manual' onclick=\"window.location.href='/manual?action=stop'\">Arrêter</button>";
  }
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid")) strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid) - 1);
  if (server.hasArg("password")) strncpy(config.password, server.arg("password").c_str(), sizeof(config.password) - 1);
  if (server.hasArg("relayPin")) config.relayPin = server.arg("relayPin").toInt();
  
  config.configured = true;
  saveConfig();
  
  server.send(200, "text/html", "<html><body><h1>Configuration sauvée</h1><p>Redémarrage...</p></body></html>");
  delay(1000);
  ESP.restart();
}

void handleSaveProgram() {
  int index = server.arg("index").toInt();
  
  if (index == -1) {
    // Nouveau programme
    if (config.programCount < 8) {
      index = config.programCount;
      config.programCount++;
    } else {
      server.send(400, "text/html", "<html><body><h1>Erreur</h1><p>Maximum 8 programmes</p></body></html>");
      return;
    }
  }
  
  IrrigationProgram& prog = config.programs[index];
  
  strncpy(prog.name, server.arg("name").c_str(), sizeof(prog.name) - 1);
  prog.hour = server.arg("hour").toInt();
  prog.minute = server.arg("minute").toInt();
  prog.duration = server.arg("duration").toInt();
  prog.enabled = server.hasArg("enabled");
  
  for (int i = 0; i < 7; i++) {
    prog.days[i] = server.hasArg("day" + String(i));
  }
  
  saveConfig();
  
  String html = "<html><body><h1>Programme sauvé</h1>";
  html += "<script>setTimeout(function(){ window.location.href='/programs'; }, 2000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleDeleteProgram() {
  int index = server.arg("index").toInt();
  
  if (index >= 0 && index < config.programCount) {
    // Décaler tous les programmes suivants
    for (int i = index; i < config.programCount - 1; i++) {
      config.programs[i] = config.programs[i + 1];
    }
    config.programCount--;
    saveConfig();
  }
  
  String html = "<html><body><h1>Programme supprimé</h1>";
  html += "<script>setTimeout(function(){ window.location.href='/programs'; }, 2000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleManual() {
  String action = server.arg("action");
  
  if (action == "start") {
    startIrrigation(5, "Manuel");
    server.send(200, "text/html", "<html><body><h1>Arrosage manuel démarré (5 min)</h1><script>setTimeout(function(){ window.location.href='/'; }, 2000);</script></body></html>");
  } else if (action == "stop") {
    stopIrrigation();
    server.send(200, "text/html", "<html><body><h1>Arrosage arrêté</h1><script>setTimeout(function(){ window.location.href='/'; }, 2000);</script></body></html>");
  } else {
    server.send(400, "text/html", "<html><body><h1>Action non reconnue</h1></body></html>");
  }
}
