#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <time.h>
#include <Ticker.h>

// Structure for an irrigation program
struct IrrigationProgram {
  bool enabled;
  bool days[7]; // D=0, L=1, M=2, M=3, J=4, V=5, S=6 (Sunday to Saturday)
  int hour;
  int minute;
  int duration; // in seconds
  int periodicity; // in seconds (0 = no repetition)
  char name[32];
  byte priority; // 0 = lowest, 255 = highest
};

// Structure to store the complete configuration
struct Config {
  char ssid[32];
  char password[64];
  char timezone[32]; // e.g., "GMT-2" for Casablanca
  IrrigationProgram programs[8]; // Maximum 8 programs
  int programCount;
  bool configured;
  int relayPin; // Pin to control the relay
};

Config config;
String dayNames[] = {"D", "L", "M", "M", "J", "V", "S"};
String dayFullNames[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};


// Web server on port 80
ESP8266WebServer server(80);

// DNS server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Operational variables
bool configMode = false;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 1000; // 1 second
bool irrigationActive = false;
unsigned long irrigationStartTime = 0;
int currentProgramDuration = 0;
unsigned long lastPeriodicActivation = 0;
int currentPeriodicity = 0;
byte currentProgramPriority = 0;

Ticker watchdog;

// --- Forward Declarations ---
// Declare functions before their definitions to ensure the compiler knows them
void resetWatchdog();
void setup();
void loop();
void checkWiFiConnection();
void logMemoryUsage();
void handleSerialCommand();
void checkIrrigationSchedule();
void startIrrigation(int duration, String programName, int periodicity, byte priority);
void stopIrrigation();
void loadConfig();
void saveConfig();
void resetConfig();
void startConfigMode();
void startNormalMode();
void handleRoot();
void handlePrograms();
void handleStatus();
void handleSave();
void handleSaveProgram();
void handleDeleteProgram();
void handleManual();
// --- End Forward Declarations ---


void resetWatchdog() {
  ESP.wdtFeed();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDémarrage du système d'irrigation ESP8266...");
  
  // Initialize watchdog
  ESP.wdtEnable(8000);
  watchdog.attach(5, resetWatchdog);
  
  // Initialize EEPROM with the exact size of the Config struct
  EEPROM.begin(sizeof(Config));
  
  // Load configuration from EEPROM
  loadConfig();
  
  // Configure the relay pin
  pinMode(config.relayPin, OUTPUT);
  digitalWrite(config.relayPin, LOW); // Relay off at startup
  
  // Check if configuration exists
  if (!config.configured) {
    Serial.println("Configuration non trouvée, démarrage en mode configuration");
    startConfigMode();
  } else {
    Serial.println("Configuration trouvée, démarrage en mode normal");
    startNormalMode();
  }
}

void loop() {
  static unsigned long lastMaintenance = 0;
  
  if (configMode) {
    // Handle DNS requests in config mode (for captive portal)
    dnsServer.processNextRequest();
    // Handle web requests
    server.handleClient();
  } else {
    // Normal mode
    unsigned long currentMillis = millis();
    
    // Manage active irrigation
    if (irrigationActive) {
      if (currentMillis - irrigationStartTime >= (currentProgramDuration * 1000UL)) {
        stopIrrigation(); // Stop irrigation when duration is met
      }
      // This part handles periodic re-activation *within* a program's total duration.
      // If the intent is for the relay to stay ON continuously for the duration, this
      // `else if` block should be removed or modified. If it's for pulse irrigation, it's correct.
      else if (currentPeriodicity > 0 && 
               currentMillis - lastPeriodicActivation >= (currentPeriodicity * 1000UL)) {
        digitalWrite(config.relayPin, HIGH); // Re-activate relay
        Serial.println("Réactivation périodique du relais");
        lastPeriodicActivation = currentMillis; // Update last activation time
      }
    }
    
    // Check irrigation schedules regularly
    if (currentMillis - lastCheckTime >= checkInterval) {
      lastCheckTime = currentMillis;
      checkIrrigationSchedule();
    }
    
    // Periodic maintenance (WiFi check, memory logging)
    if (currentMillis - lastMaintenance >= 60000) { // Every 60 seconds
      lastMaintenance = currentMillis;
      checkWiFiConnection();
      logMemoryUsage();
    }
    
    // Handle web requests
    server.handleClient();
    
    // Handle serial commands
    if (Serial.available()) {
      handleSerialCommand();
    }
  }
  
  yield(); // Important for stability, especially on ESP8266
}


// --- Utility Functions ---

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnexion WiFi...");
    WiFi.disconnect(); // Disconnect before trying to reconnect
    WiFi.begin(config.ssid, config.password);
  }
}

void logMemoryUsage() {
  Serial.printf("Mémoire libre: %d bytes\n", ESP.getFreeHeap());
  if (ESP.getFreeHeap() < 4000) { // If free memory is below 4KB, consider restarting
    Serial.println("Mémoire faible, redémarrage...");
    ESP.restart();
  }
}

void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim(); // Remove leading/trailing whitespace
  
  if (command == "reset") {
    Serial.println("Réinitialisation de la configuration");
    resetConfig();
  } else if (command == "start") {
    Serial.println("Démarrage manuel de l'arrosage (300 secondes)");
    // Added priority for manual start (e.g., 100 to override lower priority programs)
    startIrrigation(300, "Manuel", 0, 100); 
  } else if (command == "stop") {
    Serial.println("Arrêt manuel de l'arrosage");
    stopIrrigation();
  }
}

void checkIrrigationSchedule() {
  static time_t lastExecutionTime[8] = {0}; // Stores the last execution time for each program
  
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  // Check if time is synchronized (year should be reasonably after 1900)
  if (timeinfo->tm_year < 125) return; // For example, 125 means year 2025 (1900 + 125)
  
  int currentDay = timeinfo->tm_wday; // tm_wday: 0=Sunday, 6=Saturday
  
  for (int i = 0; i < config.programCount; i++) {
    IrrigationProgram& prog = config.programs[i];
    
    if (prog.enabled && prog.days[currentDay]) {
      // Check for exact scheduled time (hour, minute match, and at second 0)
      if (prog.hour == timeinfo->tm_hour && 
          prog.minute == timeinfo->tm_min && 
          timeinfo->tm_sec == 0) {
        // Prevent multiple activations in the same minute if loop runs very fast
        if (now - lastExecutionTime[i] >= 60) { 
          startIrrigation(prog.duration, prog.name, prog.periodicity, prog.priority);
          lastExecutionTime[i] = now;
        }
      }
      
      // This section handles programs with periodicity.
      // It will trigger if periodicity is set and enough time has passed since its last execution.
      // This is a common pattern for "run every X hours" or "run every X minutes" within a day.
      if (prog.periodicity > 0 && (now - lastExecutionTime[i] >= prog.periodicity)) {
        startIrrigation(prog.duration, prog.name, prog.periodicity, prog.priority);
        lastExecutionTime[i] = now;
      }
    }
  }
}

void startIrrigation(int duration, String programName, int periodicity, byte priority) {
  // If irrigation is already active
  if (irrigationActive) {
    if (priority > currentProgramPriority) {
      stopIrrigation(); // Stop current lower-priority irrigation
      Serial.printf("Interruption pour programme prioritaire: %s\n", programName.c_str());
    } else {
      Serial.printf("Programme %s ignoré (priorité inférieure ou égale)\n", programName.c_str());
      return; // Do not start if current program has higher or equal priority
    }
  }
  
  irrigationActive = true;
  irrigationStartTime = millis();
  currentProgramDuration = duration;
  currentPeriodicity = periodicity;
  currentProgramPriority = priority;
  lastPeriodicActivation = millis(); // Reset periodic activation timer for this new program
  
  digitalWrite(config.relayPin, HIGH); // Turn on the relay
  Serial.printf("ARROSAGE DÉMARRÉ - Programme: %s, Durée: %d sec, Périodicité: %d sec, Priorité: %d\n", 
                 programName.c_str(), duration, periodicity, priority);
}

void stopIrrigation() {
  if (!irrigationActive) return; // Only stop if it's active
  
  irrigationActive = false;
  currentPeriodicity = 0; // Reset for next program
  currentProgramPriority = 0; // Reset for next program
  digitalWrite(config.relayPin, LOW); // Turn off the relay
  Serial.println("ARROSAGE ARRÊTÉ");
}

// --- Configuration Management ---

void loadConfig() {
  EEPROM.get(0, config);
  
  // If the 'configured' flag is not true, it means EEPROM is empty or corrupted,
  // so we initialize with default values.
  if (config.configured != true) {
    Serial.println("Initialisation de la configuration par défaut.");
    // Initialize with default values, ensuring strings are null-terminated
    strncpy(config.ssid, "", sizeof(config.ssid) -1); config.ssid[sizeof(config.ssid)-1] = '\0';
    strncpy(config.password, "", sizeof(config.password) -1); config.password[sizeof(config.password)-1] = '\0';
    strncpy(config.timezone, "GMT-2", sizeof(config.timezone) -1); config.timezone[sizeof(config.timezone)-1] = '\0'; // Example: Casablanca/Morocco
    
    config.programCount = 0;
    config.configured = false; // System is not yet configured
    config.relayPin = 4; // Default relay pin (e.g., D2 on NodeMCU)
    
    // Initialize default values for all 8 programs
    for (int i = 0; i < 8; i++) {
      config.programs[i].enabled = false;
      for (int j = 0; j < 7; j++) {
        config.programs[i].days[j] = false;
      }
      config.programs[i].hour = 6;
      config.programs[i].minute = 0;
      config.programs[i].duration = 600; // 10 minutes in seconds
      config.programs[i].periodicity = 0; // No periodicity by default
      config.programs[i].priority = 50; // Default medium priority
      sprintf(config.programs[i].name, "Programme %d", i + 1);
      config.programs[i].name[sizeof(config.programs[i].name) -1] = '\0'; // Ensure null-termination
    }
  }
}

void saveConfig() {
  // Ensure programCount doesn't exceed the array limit
  if (config.programCount > 8) {
    config.programCount = 8;
  }
  
  EEPROM.put(0, config); // Write the entire config struct to EEPROM
  EEPROM.commit(); // Commit changes to flash memory
  Serial.println("Configuration sauvegardée.");
}

void resetConfig() {
  config.configured = false; // Mark as unconfigured
  config.programCount = 0; // Reset program count
  saveConfig(); // Save the unconfigured state
  delay(1000); // Wait a bit for serial output
  ESP.restart(); // Restart the ESP to enter configuration mode
}

// --- Operating Modes ---

void startConfigMode() {
  configMode = true;
  WiFi.mode(WIFI_AP); // Set ESP to Access Point mode
  WiFi.softAP("ESP8266-Irrigation"); // Create an AP with this SSID
  Serial.print("Point d'accès créé. IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Start DNS server to redirect all requests to the ESP's IP for captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Define web server routes for configuration mode
  server.on("/", HTTP_GET, handleRoot);
  server.on("/programs", HTTP_GET, handlePrograms);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/saveProgram", HTTP_POST, handleSaveProgram);
  server.on("/deleteProgram", HTTP_GET, []() { handleDeleteProgram(); });
  server.onNotFound(handleRoot); // For captive portal, redirect all unknown paths to root
  
  server.begin(); // Start the web server
  Serial.println("Serveur web démarré en mode configuration.");
}

void startNormalMode() {
  configMode = false;
  WiFi.mode(WIFI_STA); // Set ESP to Station mode
  WiFi.begin(config.ssid, config.password); // Connect to configured WiFi
  
  Serial.print("Connexion à ");
  Serial.print(config.ssid);
  
  int attempt = 0;
  // Try connecting to WiFi for a limited time (e.g., 20 * 500ms = 10 seconds)
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnecté au WiFi");
    Serial.print("Adresse IP: ");
    Serial.println(WiFi.localIP());
    
    // Set timezone and synchronize time via NTP
    setenv("TZ", config.timezone, 1); // Set the timezone environment variable
    tzset(); // Apply timezone settings
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Configure NTP servers
    Serial.println("Synchronisation de l'heure...");
    
    // Define web server routes for normal operating mode
    server.on("/", HTTP_GET, handleStatus); // Main status page
    server.on("/config", HTTP_GET, handleRoot); // Link to config page (WiFi, relay)
    server.on("/programs", HTTP_GET, handlePrograms); // Link to programs management page
    server.on("/save", HTTP_POST, handleSave); // Re-use save handlers
    server.on("/saveProgram", HTTP_POST, handleSaveProgram);
    server.on("/deleteProgram", HTTP_GET, []() { handleDeleteProgram(); });
    server.on("/manual", HTTP_GET, handleManual); // Manual irrigation control
    server.on("/reset", HTTP_GET, []() { // System reset button
      server.send(200, "text/html", "<html><body><h1>Reset</h1><p>Configuration réinitialisée. Redémarrage...</p></body></html>");
      resetConfig(); // Call reset function
    });
    
    server.begin(); // Start the web server
    Serial.println("Serveur web démarré en mode normal.");
  } else {
    Serial.println("\nÉchec de connexion WiFi. Retour en mode configuration.");
    startConfigMode(); // If WiFi connection fails, revert to config mode
  }
}

// --- Web Handlers ---

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuration Irrigation</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; color: #333; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 25px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += "h1 { color: #4CAF50; text-align: center; margin-bottom: 25px; }";
  html += "label { display: block; margin-top: 18px; font-weight: bold; color: #555; }";
  html += "input[type='text'], input[type='password'], input[type='number'] { width: calc(100% - 22px); padding: 11px; margin-top: 8px; border: 1px solid #ccc; border-radius: 6px; box-sizing: border-box; font-size: 1em; }";
  html += "button { background: #4CAF50; color: white; padding: 13px 22px; margin: 15px 5px 0; border: none; border-radius: 6px; cursor: pointer; font-size: 1.1em; transition: background 0.3s ease; }";
  html += "button:hover { background: #45a049; }";
  html += ".nav-button { background: #2196F3; }";
  html += ".nav-button:hover { background: #1976D2; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>🌱 Configuration Irrigation</h1>";
  html += "<form action='/save' method='POST'>";
  
  html += "<label for='ssid'>SSID WiFi:</label>";
  html += "<input type='text' id='ssid' name='ssid' value='" + String(config.ssid) + "' required>";
  
  html += "<label for='password'>Mot de passe WiFi:</label>";
  html += "<input type='password' id='password' name='password' value='" + String(config.password) + "'>";
  
  html += "<label for='relayPin'>Pin du relais (GPIO):</label>";
  html += "<input type='number' id='relayPin' name='relayPin' value='" + String(config.relayPin) + "' min='0' max='16'>";
  
  html += "<button type='submit'>Enregistrer WiFi & Redémarrer</button>";
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
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; color: #333; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 25px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += "h1 { color: #4CAF50; text-align: center; margin-bottom: 25px; }";
  html += ".program { border: 1px solid #ddd; margin: 15px 0; padding: 20px; border-radius: 8px; background: #f9f9f9; box-shadow: inset 0 1px 3px rgba(0,0,0,0.05); }";
  html += "h3 { color: #2196F3; margin-top: 0; }";
  html += ".days { display: flex; flex-wrap: wrap; gap: 8px; margin: 10px 0 15px; }";
  html += ".day { padding: 8px 12px; border: 1px solid #bbb; border-radius: 5px; cursor: pointer; background: #eee; transition: background 0.2s, color 0.2s; font-size: 0.95em; }";
  html += ".day.selected { background: #4CAF50; color: white; border-color: #4CAF50; }";
  html += "label { display: block; margin: 15px 0 6px 0; font-weight: bold; color: #555; }";
  html += "input[type='text'], input[type='number'], select { width: calc(100% - 22px); padding: 10px; margin: 5px 0; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box; font-size: 1em; }";
  html += "input[type='number'] { width: 80px; display: inline-block; }"; // For hour/minute inputs
  html += "input[type='checkbox'] { margin-right: 8px; transform: scale(1.2); }";
  html += "button { background: #4CAF50; color: white; padding: 11px 18px; margin: 8px 5px 0; border: none; border-radius: 5px; cursor: pointer; font-size: 1em; transition: background 0.3s ease; }";
  html += "button:hover { background: #45a049; }";
  html += ".delete { background: #f44336; }";
  html += ".delete:hover { background: #d32f2f; }";
  html += ".back-button { background: #607D8B; margin-top: 25px; }";
  html += ".back-button:hover { background: #455A64; }";
  html += ".info { background: #e0f2f7; border: 1px solid #b3e5fc; padding: 12px; border-radius: 5px; text-align: center; color: #01579B; margin-top: 15px; }";
  html += "</style>";
  html += "<script>";
  html += "function toggleDay(progId, dayIndex) {"; // progId can be index or "new"
  html += "  var checkbox = document.getElementById('day_' + progId + '_' + dayIndex);";
  html += "  var dayDiv = document.getElementById('dayDiv_' + progId + '_' + dayIndex);";
  html += "  checkbox.checked = !checkbox.checked;";
  html += "  dayDiv.className = checkbox.checked ? 'day selected' : 'day';";
  html += "}";
  html += "</script></head><body>";
  html += "<div class='container'>";
  html += "<h1>🕒 Programmes d'Arrosage</h1>";
  
  // Display existing programs
  for (int i = 0; i < config.programCount; i++) {
    IrrigationProgram& prog = config.programs[i];
    html += "<div class='program'>";
    html += "<form action='/saveProgram' method='POST'>";
    html += "<input type='hidden' name='index' value='" + String(i) + "'>"; // Hidden field for program index
    
    html += "<label for='name_" + String(i) + "'>Nom du programme:</label>";
    html += "<input type='text' id='name_" + String(i) + "' name='name' value='" + String(prog.name) + "'>";
    
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
    
    html += "<label for='hour_" + String(i) + "'>Heure:</label>";
    html += "<input type='number' id='hour_" + String(i) + "' name='hour' value='" + String(prog.hour) + "' min='0' max='23'> h ";
    html += "<input type='number' name='minute' value='" + String(prog.minute) + "' min='0' max='59'> min";
    
    html += "<label for='duration_" + String(i) + "'>Durée (secondes):</label>";
    html += "<input type='number' id='duration_" + String(i) + "' name='duration' value='" + String(prog.duration) + "' min='1' max='86400'>";
    
    html += "<label for='periodicity_" + String(i) + "'>Périodicité (secondes, 0=désactivé):</label>";
    html += "<input type='number' id='periodicity_" + String(i) + "' name='periodicity' value='" + String(prog.periodicity) + "' min='0' max='86400'>";
    
    html += "<label for='enabled_" + String(i) + "'><input type='checkbox' id='enabled_" + String(i) + "' name='enabled'";
    if (prog.enabled) html += " checked";
    html += "> Activer ce programme</label>";

    html += "<label for='priority_" + String(i) + "'>Priorité (0-255):</label>";
    html += "<input type='number' id='priority_" + String(i) + "' name='priority' value='" + String(prog.priority) + "' min='0' max='255'>";
    
    html += "<button type='submit'>Sauvegarder</button>";
    html += "<button type='button' class='delete' onclick=\"if(confirm('Êtes-vous sûr de vouloir supprimer ce programme ?')) window.location.href='/deleteProgram?index=" + String(i) + "'\">Supprimer</button>";
    html += "</form></div>";
  }
  
  // Option to add a new program (only if max limit not reached)
  if (config.programCount < 8) {
    html += "<div class='program'>";
    html += "<h3>➕ Nouveau Programme</h3>";
    html += "<form action='/saveProgram' method='POST'>";
    html += "<input type='hidden' name='index' value='-1'>"; // -1 indicates a new program
    
    html += "<label>Nom:</label>";
    html += "<input type='text' name='name' value='Nouveau programme'>";
    
    html += "<label>Jours:</label>";
    html += "<div class='days'>";
    for (int j = 0; j < 7; j++) {
      html += "<div id='dayDiv_new_" + String(j) + "' class='day' onclick='toggleDay(\"new\"," + String(j) + ")'>" + dayNames[j] + "</div>";
      html += "<input type='checkbox' id='day_new_" + String(j) + "' name='day" + String(j) + "' style='display:none;'>"; // Hidden checkbox
    }
    html += "</div>";
    
    html += "<label>Heure:</label>";
    html += "<input type='number' name='hour' value='6' min='0' max='23'> h ";
    html += "<input type='number' name='minute' value='0' min='0' max='59'> min";
    
    html += "<label>Durée (secondes):</label>";
    html += "<input type='number' name='duration' value='600' min='1' max='86400'>";
    
    html += "<label>Périodicité (secondes, 0=désactivé):</label>";
    html += "<input type='number' name='periodicity' value='0' min='0' max='86400'>";

    html += "<label>Priorité (0-255):</label>";
    html += "<input type='number' name='priority' value='50' min='0' max='255'>";
    
    html += "<label><input type='checkbox' name='enabled' checked> Activer</label>";
    
    html += "<button type='submit'>Ajouter Programme</button>";
    html += "</form></div>";
  } else {
    html += "<div class='info'>Nombre maximum de programmes atteint (8)</div>";
  }
  
  html += "<button class='back-button' onclick=\"window.location.href='/'\">Retour</button>";
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
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; color: #333; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 25px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); text-align: center; }";
  html += "h1 { color: #4CAF50; margin-bottom: 25px; }";
  html += ".status { padding: 20px; margin: 20px 0; border-radius: 10px; font-size: 20px; font-weight: bold; box-shadow: 0 2px 4px rgba(0,0,0,0.15); }";
  html += ".active { background: #4CAF50; color: white; }";
  html += ".inactive { background: #f44336; color: white; }";
  html += ".info { margin: 12px 0; padding: 10px; background: #e0f2f7; border-radius: 6px; text-align: left; color: #01579B; font-size: 0.95em; border: 1px solid #b3e5fc;}";
  html += "button { background: #2196F3; color: white; padding: 13px 22px; margin: 10px 8px; border: none; border-radius: 6px; cursor: pointer; font-size: 1.1em; transition: background 0.3s ease; }";
  html += "button:hover { background: #1976D2; }";
  html += ".manual { background: #FF9800; }";
  html += ".manual:hover { background: #F57C00; }";
  html += ".stop-button { background: #f44336; }";
  html += ".stop-button:hover { background: #d32f2f; }";
  html += "</style>";
  html += "<meta http-equiv='refresh' content='30'>"; // Auto-refresh every 30 seconds
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🌱 État du Système d'Irrigation</h1>";
  
  // Irrigation Status Display
  html += "<div class='status " + String(irrigationActive ? "active" : "inactive") + "'>";
  if (irrigationActive) {
    int elapsed = (millis() - irrigationStartTime) / 1000;
    int remaining = currentProgramDuration - elapsed;
    if (remaining < 0) remaining = 0; // Ensure remaining time is not negative
    html += "🚿 ARROSAGE EN COURS<br>";
    html += "Durée restante: " + String(remaining) + " sec<br>";
    if (currentPeriodicity > 0) {
      html += "Périodicité: toutes les " + String(currentPeriodicity) + " sec";
    }
  } else {
    html += "💧 ARROSAGE ARRÊTÉ";
  }
  html += "</div>";
  
  // System Information Display
  html += "<div class='info'><strong>WiFi:</strong> " + String(config.ssid) + "</div>";
  html += "<div class='info'><strong>IP:</strong> " + WiFi.localIP().toString() + "</div>";
  // Format time with leading zeros if necessary
  String timeStr = String(timeinfo->tm_hour) + ":" + 
                   (timeinfo->tm_min < 10 ? "0" : "") + String(timeinfo->tm_min) + ":" + 
                   (timeinfo->tm_sec < 10 ? "0" : "") + String(timeinfo->tm_sec);
  html += "<div class='info'><strong>Heure:</strong> " + timeStr + "</div>";
  html += "<div class='info'><strong>Jour:</strong> " + dayFullNames[timeinfo->tm_wday] + "</div>";
  html += "<div class='info'><strong>Mémoire libre:</strong> " + String(ESP.getFreeHeap()) + " bytes</div>";
  
  int activeCount = 0;
  for (int i = 0; i < config.programCount; i++) {
    if (config.programs[i].enabled) activeCount++;
  }
  html += "<div class='info'><strong>Programmes configurés/actifs:</strong> " + String(config.programCount) + "/" + String(activeCount) + "</div>";
  
  // Control Buttons
  html += "<button onclick=\"window.location.href='/programs'\">Gérer les Programmes</button>";
  html += "<button onclick=\"window.location.href='/config'\">Paramètres WiFi/Relais</button>";
  html += "<button class='manual' onclick=\"window.location.href='/manual?action=start'\">Arrosage Manuel (5 min)</button>";
  if (irrigationActive) {
    html += "<button class='stop-button' onclick=\"window.location.href='/manual?action=stop'\">Arrêter l'arrosage</button>";
  }
  html += "<button class='stop-button' onclick=\"if(confirm('Ceci réinitialisera toute la configuration et redémarrera. Continuer ?')) window.location.href='/reset'\">Réinitialiser le Système</button>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid")) {
    strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0'; // Ensure null-termination
  }
  if (server.hasArg("password")) {
    strncpy(config.password, server.arg("password").c_str(), sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0'; // Ensure null-termination
  }
  if (server.hasArg("relayPin")) config.relayPin = server.arg("relayPin").toInt();
  
  config.configured = true; // Mark as configured
  saveConfig();
  
  server.send(200, "text/html", "<html><body><h1>Configuration sauvegardée</h1><p>Redémarrage du système...</p><script>setTimeout(function(){ window.location.href='/'; }, 3000);</script></body></html>");
  delay(1000); // Give time for browser to receive the response
  ESP.restart(); // Restart ESP to apply new WiFi settings
}

void handleSaveProgram() {
  int index = server.arg("index").toInt();
  
  if (index == -1) { // This is a new program
    if (config.programCount < 8) { // Check if we have space for a new program
      index = config.programCount;
      config.programCount++; // Increment count of active programs
    } else {
      server.send(400, "text/html", "<html><body><h1>Erreur</h1><p>Maximum 8 programmes sont autorisés.</p><script>setTimeout(function(){ window.location.href='/programs'; }, 3000);</script></body></html>");
      return;
    }
  } else if (index < 0 || index >= 8) { // Invalid index for an existing program (shouldn't happen with proper frontend)
    server.send(400, "text/html", "<html><body><h1>Erreur</h1><p>Index de programme invalide.</p><script>setTimeout(function(){ window.location.href='/programs'; }, 3000);</script></body></html>");
    return;
  }
  
  IrrigationProgram& prog = config.programs[index]; // Get a reference to the program struct
  
  strncpy(prog.name, server.arg("name").c_str(), sizeof(prog.name) - 1);
  prog.name[sizeof(prog.name) - 1] = '\0'; // Ensure null-termination
  
  prog.hour = server.arg("hour").toInt();
  prog.minute = server.arg("minute").toInt();
  prog.duration = server.arg("duration").toInt();
  prog.periodicity = server.arg("periodicity").toInt();
  prog.priority = server.arg("priority").toInt(); // Assign priority
  prog.enabled = server.hasArg("enabled"); // Check if 'enabled' checkbox was present
  
  for (int i = 0; i < 7; i++) {
    prog.days[i] = server.hasArg("day" + String(i)); // Check if each day checkbox was present
  }
  
  saveConfig(); // Save the updated configuration
  
  String html = "<html><body><h1>Programme sauvegardé</h1><script>setTimeout(function(){ window.location.href='/programs'; }, 1000);</script></body></html>";
  server.send(200, "text/html", html);
}

void handleDeleteProgram() {
  Serial.println("Tentative de suppression de programme");
  
  if (!server.hasArg("index")) {
    Serial.println("Erreur: pas d'index");
    server.send(400, "text/html", "<html><body><h1>Erreur</h1><p>Paramètre d'index manquant.</p><script>setTimeout(function(){ window.location.href='/programs'; }, 3000);</script></body></html>");
    return;
  }

  int index = server.arg("index").toInt();
  Serial.print("Index reçu: ");
  Serial.println(index);
  
  if (index < 0 || index >= config.programCount) {
    Serial.print("Erreur: index invalide (count=");
    Serial.print(config.programCount);
    Serial.println(")");
    server.send(400, "text/html", "<html><body><h1>Erreur</h1><p>Index de programme invalide.</p><script>setTimeout(function(){ window.location.href='/programs'; }, 3000);</script></body></html>");
    return;
  }

  Serial.println("Suppression en cours...");
  
  // Shift all subsequent programs one position to the left to fill the gap
  for (int i = index; i < config.programCount - 1; i++) {
    config.programs[i] = config.programs[i + 1];
  }
  
  config.programCount--; // Decrement the total number of configured programs
  
  // Optionally, clear the last program slot (now technically unused after shifting)
  // This helps ensure EEPROM consistency and avoids stale data if programCount is low
  if (config.programCount < 8) { // Only attempt to clear if there's a valid slot index
    config.programs[config.programCount].enabled = false;
    for (int j = 0; j < 7; j++) {
      config.programs[config.programCount].days[j] = false;
    }
    config.programs[config.programCount].hour = 6;
    config.programs[config.programCount].minute = 0;
    config.programs[config.programCount].duration = 600;
    config.programs[config.programCount].periodicity = 0;
    config.programs[config.programCount].priority = 50;
    sprintf(config.programs[config.programCount].name, "Programme %d", config.programCount + 1); // Rename the placeholder
    config.programs[config.programCount].name[sizeof(config.programs[config.programCount].name) -1] = '\0'; // Null-terminate
  }

  saveConfig(); // Save the modified configuration
  
  String html = "<html><body><h1>Programme supprimé</h1><script>setTimeout(function(){ window.location.href='/programs'; }, 1000);</script></body></html>";
  server.send(200, "text/html", html);
  
  Serial.println("Programme supprimé avec succès");
}

void handleManual() {
  String action = server.arg("action");
  
  if (action == "start") {
    // Manually start irrigation for 300 seconds (5 minutes) with no periodicity and a priority of 100
    startIrrigation(300, "Manuel", 0, 100); 
    server.send(200, "text/html", "<html><body><h1>Arrosage manuel démarré (300 sec)</h1><script>setTimeout(function(){ window.location.href='/'; }, 2000);</script></body></html>");
  } else if (action == "stop") {
    stopIrrigation(); // Stop any active irrigation
    server.send(200, "text/html", "<html><body><h1>Arrosage arrêté</h1><script>setTimeout(function(){ window.location.href='/'; }, 2000);</script></body></html>");
  } else {
    server.send(400, "text/html", "<html><body><h1>Action non reconnue</h1></body></html>");
  }
}
