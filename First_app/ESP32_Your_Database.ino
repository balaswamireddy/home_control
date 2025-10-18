#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>




// ============================================================================
// BOARD CONFIGURATION - CUSTOMIZE FOR EACH DEVICE
// ============================================================================




#define BOARD_ID "BOARD_005"  // UNIQUE ID for each board (BOARD_001, BOARD_002, etc.)
#define FIRMWARE_VERSION "2.0.0"
#define NUM_SWITCHES 4




// Pin Definitions
#define RELAY_PIN_1 13   // GPIO2 - Switch 1 Relay
#define RELAY_PIN_2 12   // GPIO4 - Switch 2 Relay  
#define RELAY_PIN_3 14  // GPIO16 - Switch 3 Relay
#define RELAY_PIN_4 27 // GPIO17 - Switch 4 Relay




#define BUTTON_PIN_1 26 // GPIO18 - Physical button for Switch 1
#define BUTTON_PIN_2 25 // GPIO19 - Physical button for Switch 2
#define BUTTON_PIN_3 33 // GPIO21 - Physical button for Switch 3
#define BUTTON_PIN_4 32 // GPIO22 - Physical button for Switch 4




#define STATUS_LED 2    // GPIO2 - Status LED (built-in)
#define RESET_PIN 0     // GPIO0 - Factory reset button




// Network Configuration
#define CONFIG_SSID "SmartSwitch_" BOARD_ID
#define CONFIG_PASSWORD "12345678"
#define CONFIG_TIMEOUT 300000  // 5 minutes in config mode




// Supabase Configuration - YOUR NEW PROJECT CREDENTIALS
String supabase_url = "https://nchshzvzjwlhquvjzhsi.supabase.co";
String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im5jaHNoenZ6andsaHF1dmp6aHNpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjAwNzU4NDIsImV4cCI6MjA3NTY1MTg0Mn0.ASwxbx9m6a09MT8x31qvkSwy2yBLHAVhOMZ3jutLNS8";
String wifi_ssid = "Stone age";  // YOUR WIFI SSID
String wifi_password = "stoneage";  // YOUR WIFI PASSWORD




// ============================================================================
// GLOBAL VARIABLES
// ============================================================================




WebServer server(80);
HTTPClient http;
WiFiClientSecure client;




// Switch states
bool switchStates[NUM_SWITCHES] = {false, false, false, false};
bool lastSwitchStates[NUM_SWITCHES] = {false, false, false, false};
int relayPins[NUM_SWITCHES] = {RELAY_PIN_1, RELAY_PIN_2, RELAY_PIN_3, RELAY_PIN_4};
int buttonPins[NUM_SWITCHES] = {BUTTON_PIN_1, BUTTON_PIN_2, BUTTON_PIN_3, BUTTON_PIN_4};




// Timing variables
unsigned long lastHeartbeat = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastDatabasePoll = 0;
unsigned long lastStatusPrint = 0;




// State variables
bool isConfigMode = false;
bool isConnectedToWiFi = false;
bool isConnectedToSupabase = false;
String deviceMAC = "";




// Physical switch finite state machine for instant response
enum SwitchState {
  SWITCH_IDLE,
  SWITCH_PRESSED,
  SWITCH_DEBOUNCE_PRESS,
  SWITCH_RELEASED,
  SWITCH_DEBOUNCE_RELEASE
};

struct SwitchFSM {
  SwitchState state;
  bool currentReading;
  bool lastReading;
  bool stableState;
  unsigned long stateChangeTime;
  bool pendingDatabaseUpdate;
  bool pendingUpdateState;
};

SwitchFSM switchFSM[NUM_SWITCHES];
const unsigned long DEBOUNCE_TIME = 20; // 20ms debounce for instant response

// Database update queue for non-blocking operations
struct DatabaseUpdate {
  int switchIndex;
  bool state;
  bool pending;
};

DatabaseUpdate pendingUpdates[NUM_SWITCHES];

// Config mode trigger variables (Switch 1 toggle detection)
int switch1ToggleCount = 0;
unsigned long firstToggleTime = 0;
unsigned long configModeTimeout = 10000; // 10 seconds to complete 7 toggles
bool configModeTriggered = false;




// ============================================================================
// SETUP FUNCTION
// ============================================================================




void setup() {
  Serial.begin(115200);
  delay(1000);



  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  SMART HOME ESP32 CONTROLLER v2.0.0   ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("Board ID: " + String(BOARD_ID));
  Serial.println("Firmware: " + String(FIRMWARE_VERSION));



  // Get MAC address
  deviceMAC = WiFi.macAddress();
  Serial.println("MAC Address: " + deviceMAC);
  Serial.println("");



  // Initialize pins
  initializePins();

  // Initialize switch finite state machines
  initializeSwitchFSM();

  // Load WiFi credentials from EEPROM
  loadWiFiCredentials();

  // Connect to WiFi
  if (wifi_ssid.length() > 0) {
    connectToWiFi();
  } else {
    Serial.println("✗ No WiFi credentials found");
    Serial.println("💡 Toggle Switch 1 seven times rapidly to enter WiFi configuration mode");
    Serial.println("   Waiting for config trigger...");
  }
}




// ============================================================================
// MAIN LOOP
// ============================================================================




void loop() {
  // Handle config mode
  if (isConfigMode) {
    server.handleClient();
    // Blink status LED in config mode
    digitalWrite(STATUS_LED, (millis() / 500) % 2);
    return; // No delay for maximum responsiveness
  }

  // ABSOLUTE PRIORITY: Check physical buttons with FSM (ALWAYS FIRST - no timing restrictions)
  processSwitchFSM();

  // Lower priority background tasks (use static counters to minimize millis() calls)
  static unsigned long loopCounter = 0;
  loopCounter++;

  // Only do background tasks every 1000 loops to maintain switch responsiveness
  if (loopCounter % 1000 == 0) {
    // Process pending database updates (non-blocking)
    processPendingDatabaseUpdates();

    // Check WiFi connection (non-blocking reconnection)
    if (WiFi.status() != WL_CONNECTED && wifi_ssid.length() > 0) {
      if (millis() - lastButtonCheck > 5000) { // Only check every 5 seconds
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str()); // Non-blocking
        lastButtonCheck = millis();
      }
    }

    // Background tasks (only if connected and at intervals)
    if (isConnectedToWiFi) {
      // Poll database for remote changes (every 500ms)
      if (millis() - lastDatabasePoll > 500) {
        loadSwitchStatesFromDatabase();
        lastDatabasePoll = millis();
      }

      // Send heartbeat (every 30 seconds)
      if (millis() - lastHeartbeat > 30000) {
        sendHeartbeat();
        lastHeartbeat = millis();
      }
    }

    // Status updates (every 30 seconds)
    if (millis() - lastStatusPrint > 30000) {
      printStatus();
      lastStatusPrint = millis();
    }

    // Status LED - solid on when connected (non-blocking)
    digitalWrite(STATUS_LED, isConnectedToWiFi ? HIGH : LOW);
  }

  // NO DELAY - Maximum responsiveness for physical switches
}




// ============================================================================
// PIN INITIALIZATION
// ============================================================================




void initializePins() {
  // Initialize relay pins as outputs (HIGH = ON, LOW = OFF for this setup)
  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); // Turn off relay initially (LOW = OFF)

    // Initialize button pins as inputs with pullup
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Initialize status LED
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("✓ GPIO pins initialized");
  Serial.println("✓ All relays initialized to OFF state (LOW)");
}

void initializeSwitchFSM() {
  // Initialize finite state machine for each switch
  for (int i = 0; i < NUM_SWITCHES; i++) {
    switchFSM[i].state = SWITCH_IDLE;
    switchFSM[i].currentReading = digitalRead(buttonPins[i]);
    switchFSM[i].lastReading = switchFSM[i].currentReading;
    switchFSM[i].stableState = switchFSM[i].currentReading;
    switchFSM[i].stateChangeTime = 0;
    switchFSM[i].pendingDatabaseUpdate = false;
    switchFSM[i].pendingUpdateState = false;
    
    // Initialize database update queue
    pendingUpdates[i].switchIndex = i;
    pendingUpdates[i].state = false;
    pendingUpdates[i].pending = false;
  }
  
  Serial.println("✓ Switch FSM initialized for instant response");
}




// ============================================================================
// WIFI CONNECTION
// ============================================================================




void connectToWiFi() {
  Serial.println("\n════════════════════════════════════════");
  Serial.println("CONNECTING TO WIFI");
  Serial.println("════════════════════════════════════════");
  Serial.println("SSID: " + wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  // Non-blocking connection check with switch processing
  unsigned long startTime = millis();
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    // Process switches even during WiFi connection
    processSwitchFSM();
    
    // Only print dots and increment counter every second
    if (millis() - startTime >= 1000) {
      Serial.print(".");
      attempts++;
      startTime = millis();
    }
    
    // Small delay to prevent excessive CPU usage but maintain switch responsiveness
    delayMicroseconds(100); // 0.1ms delay instead of 1000ms
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    isConnectedToWiFi = true;
    Serial.println("✓ WiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    Serial.println("Signal strength: " + String(WiFi.RSSI()) + " dBm");

    // Configure HTTPS client
    client.setInsecure();
    isConnectedToSupabase = true;

    Serial.println("\n════════════════════════════════════════");
    Serial.println("✓ BOARD READY");
    Serial.println("════════════════════════════════════════\n");

    // Load initial switch states
    loadSwitchStatesFromDatabase();
  } else {
    Serial.println("✗ WiFi connection failed!");
    isConnectedToWiFi = false;
  }
}




// ============================================================================
// SUPABASE DATABASE FUNCTIONS
// ============================================================================




void loadSwitchStatesFromDatabase() {
  if (!isConnectedToWiFi) return;
  
  Serial.println("\n[POLL] Checking switch states...");
  
  HTTPClient http;
  String url = supabase_url + "/rest/v1/switches?board_id=eq." + String(BOARD_ID) + "&order=position&select=id,name,state,position";
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      JsonArray switches = doc.as<JsonArray>();
      
      for (int i = 0; i < switches.size() && i < NUM_SWITCHES; i++) {
        bool newState = switches[i]["state"];
        
        // Fixed: Proper null check for name field
        String name;
        if (switches[i]["name"].isNull()) {
          name = "Switch " + String(i + 1);
        } else {
          name = switches[i]["name"].as<String>();
        }
        
        // TOGGLE LOGIC: Only update relay if database state differs from current state
        // This allows remote control to override physical switch position
        if (switchStates[i] != newState) {
          Serial.println("  [REMOTE] " + name + ": " + String(switchStates[i] ? "ON" : "OFF") + " → " + String(newState ? "ON" : "OFF"));
          
          // Update relay to match database state (remote control wins)
          controlRelay(i, newState);
          
          // If physical switch is in opposite position, user will need to toggle it to regain physical control
          if (switchFSM[i].stableState && !newState) {
            Serial.println("      Physical switch is pressed but relay turned OFF by remote - toggle physical switch to regain control");
          } else if (!switchFSM[i].stableState && newState) {
            Serial.println("      Physical switch is released but relay turned ON by remote - toggle physical switch to regain control");
          }
        }
      }
    }
  } else if (httpResponseCode == 401) {
    Serial.println("[ERROR] 401 Unauthorized - Check database RLS policies!");
  } else if (httpResponseCode != 200) {
    Serial.println("[ERROR] HTTP " + String(httpResponseCode));
  }
  
  http.end();
}




void updateSwitchInDatabase(int switchIndex, bool state, String triggeredBy) {
  if (!isConnectedToWiFi) return;



  Serial.println("\n[UPDATE] Switch " + String(switchIndex + 1) + " → " + String(state ? "ON" : "OFF"));



  HTTPClient http;
  String switchId = String(BOARD_ID) + "_switch_" + String(switchIndex + 1);
  String url = supabase_url + "/rest/v1/switches?id=eq." + switchId;



  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");



  DynamicJsonDocument doc(256);
  doc["state"] = state;



  String requestBody;
  serializeJson(doc, requestBody);



  int httpResponseCode = http.sendRequest("PATCH", requestBody);



  if (httpResponseCode == 200 || httpResponseCode == 204) {
    Serial.println("✓ Database updated");
  } else {
    Serial.println("✗ Update failed: " + String(httpResponseCode));
  }



  http.end();
}




void sendHeartbeat() {
  if (!isConnectedToWiFi) return;



  Serial.println("\n[HEARTBEAT] Sending...");



  HTTPClient http;
  String url = supabase_url + "/rest/v1/boards?id=eq." + String(BOARD_ID);



  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");



  DynamicJsonDocument doc(256);
  doc["status"] = "online";



  String requestBody;
  serializeJson(doc, requestBody);



  int httpResponseCode = http.sendRequest("PATCH", requestBody);



  if (httpResponseCode == 200 || httpResponseCode == 204) {
    Serial.println("✓ Heartbeat sent");
  }



  http.end();
}




// ============================================================================
// PHYSICAL SWITCH CONTROL
// ============================================================================




void controlRelay(int switchIndex, bool state) {
  // Ultra-fast relay control - no checks, no serial prints for maximum speed
  digitalWrite(relayPins[switchIndex], state ? HIGH : LOW);
  switchStates[switchIndex] = state;
}




void processSwitchFSM() {
  unsigned long currentTime = millis();
  
  for (int i = 0; i < NUM_SWITCHES; i++) {
    // Always read current state
    bool previousReading = switchFSM[i].currentReading;
    switchFSM[i].currentReading = digitalRead(buttonPins[i]) == LOW; // LOW = pressed
    
    // INSTANT RELAY CONTROL - Control relay IMMEDIATELY on any pin state change
    if (switchFSM[i].currentReading != previousReading) {
      // Pin state changed - CONTROL RELAY INSTANTLY (zero delay)
      digitalWrite(relayPins[i], switchFSM[i].currentReading ? HIGH : LOW);
      switchStates[i] = switchFSM[i].currentReading;
      
      // Optional: Minimal debug output (remove if causing any delay)
      // Serial.println("[INSTANT] Switch " + String(i + 1) + " → " + String(switchFSM[i].currentReading ? "ON" : "OFF"));
    }
    
    // Finite State Machine for debounced database updates (relay already controlled above)
    switch (switchFSM[i].state) {
      
      case SWITCH_IDLE:
        // Wait for state change
        if (switchFSM[i].currentReading != switchFSM[i].stableState) {
          switchFSM[i].state = switchFSM[i].currentReading ? SWITCH_DEBOUNCE_PRESS : SWITCH_DEBOUNCE_RELEASE;
          switchFSM[i].stateChangeTime = currentTime;
        }
        break;
        
      case SWITCH_DEBOUNCE_PRESS:
        // Debouncing press (relay already controlled above)
        if (switchFSM[i].currentReading) {
          if (currentTime - switchFSM[i].stateChangeTime >= DEBOUNCE_TIME) {
            // Confirmed press - update stable state
            switchFSM[i].state = SWITCH_PRESSED;
            switchFSM[i].stableState = true;
            
            // Queue database update (non-blocking)
            queueDatabaseUpdate(i, true);
            
            // Check config mode trigger for Switch 1
            if (i == 0) {
              checkConfigModeTrigger();
            }
          }
        } else {
          // False trigger, return to idle
          switchFSM[i].state = SWITCH_IDLE;
        }
        break;
        
      case SWITCH_PRESSED:
        // Wait for release
        if (!switchFSM[i].currentReading) {
          switchFSM[i].state = SWITCH_DEBOUNCE_RELEASE;
          switchFSM[i].stateChangeTime = currentTime;
        }
        break;
        
      case SWITCH_DEBOUNCE_RELEASE:
        // Debouncing release (relay already controlled above)
        if (!switchFSM[i].currentReading) {
          if (currentTime - switchFSM[i].stateChangeTime >= DEBOUNCE_TIME) {
            // Confirmed release - update stable state
            switchFSM[i].state = SWITCH_RELEASED;
            switchFSM[i].stableState = false;
            
            // Queue database update (non-blocking)
            queueDatabaseUpdate(i, false);
            
            // Check config mode trigger for Switch 1
            if (i == 0) {
              checkConfigModeTrigger();
            }
          }
        } else {
          // False trigger, return to pressed
          switchFSM[i].state = SWITCH_PRESSED;
        }
        break;
        
      case SWITCH_RELEASED:
        // Wait for next press or return to idle
        switchFSM[i].state = SWITCH_IDLE;
        break;
    }
  }
}

void queueDatabaseUpdate(int switchIndex, bool state) {
  // Queue database update for background processing
  pendingUpdates[switchIndex].switchIndex = switchIndex;
  pendingUpdates[switchIndex].state = state;
  pendingUpdates[switchIndex].pending = true;
}

void processPendingDatabaseUpdates() {
  // Process one pending database update per loop iteration (non-blocking)
  static int updateIndex = 0;
  
  for (int count = 0; count < NUM_SWITCHES; count++) {
    if (pendingUpdates[updateIndex].pending) {
      // Process this update
      updateSwitchInDatabase(
        pendingUpdates[updateIndex].switchIndex, 
        pendingUpdates[updateIndex].state, 
        "physical"
      );
      
      // Mark as processed
      pendingUpdates[updateIndex].pending = false;
      break; // Only process one update per loop iteration
    }
    
    // Move to next switch
    updateIndex = (updateIndex + 1) % NUM_SWITCHES;
  }
}

// ============================================================================
// CONFIG MODE TRIGGER DETECTION
// ============================================================================

void checkConfigModeTrigger() {
  unsigned long currentTime = millis();
  
  // Reset counter if too much time has passed since first toggle
  if (switch1ToggleCount > 0 && (currentTime - firstToggleTime > configModeTimeout)) {
    Serial.println("[CONFIG] Toggle timeout - resetting counter");
    switch1ToggleCount = 0;
  }
  
  // If this is the first toggle, record the time
  if (switch1ToggleCount == 0) {
    firstToggleTime = currentTime;
  }
  
  // Increment toggle counter
  switch1ToggleCount++;
  
  Serial.println("[CONFIG] Switch 1 toggle count: " + String(switch1ToggleCount) + "/7");
  
  // Check if we've reached 7 toggles within the timeout period
  if (switch1ToggleCount >= 7) {
    Serial.println("\n🔧 CONFIG MODE TRIGGERED! Switch 1 toggled 7 times.");
    Serial.println("Starting WiFi configuration mode...");
    
    configModeTriggered = true;
    switch1ToggleCount = 0; // Reset counter
    
    // Enter config mode
    startConfigMode();
  }
}

// ============================================================================
// EEPROM FUNCTIONS FOR WIFI CREDENTIALS
// ============================================================================

void saveWiFiCredentials(String ssid, String password) {
  Serial.println("Saving WiFi credentials to EEPROM...");
  
  // Initialize EEPROM if not already done
  EEPROM.begin(512);
  
  // Clear EEPROM first
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  
  // Write SSID length and data
  EEPROM.write(0, ssid.length());
  for (int i = 0; i < ssid.length(); i++) {
    EEPROM.write(i + 1, ssid[i]);
  }
  
  // Write Password length and data
  EEPROM.write(100, password.length());
  for (int i = 0; i < password.length(); i++) {
    EEPROM.write(i + 101, password[i]);
  }
  
  EEPROM.commit();
  Serial.println("✓ WiFi credentials saved to EEPROM");
}

void loadWiFiCredentials() {
  Serial.println("Loading WiFi credentials from EEPROM...");
  
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Read SSID
  int ssidLength = EEPROM.read(0);
  if (ssidLength > 0 && ssidLength < 32) {
    wifi_ssid = "";
    for (int i = 0; i < ssidLength; i++) {
      wifi_ssid += char(EEPROM.read(i + 1));
    }
  }
  
  // Read Password
  int passwordLength = EEPROM.read(100);
  if (passwordLength > 0 && passwordLength < 64) {
    wifi_password = "";
    for (int i = 0; i < passwordLength; i++) {
      wifi_password += char(EEPROM.read(i + 101));
    }
  }
  
  if (wifi_ssid.length() > 0) {
    Serial.println("✓ Found stored WiFi credentials for: " + wifi_ssid);
  } else {
    Serial.println("✗ No stored WiFi credentials found in EEPROM");
  }
}

// ============================================================================
// CONFIG MODE FUNCTIONS
// ============================================================================

void startConfigMode() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         WiFi CONFIG MODE STARTED       ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  isConfigMode = true;
  
  // Disconnect from current WiFi
  WiFi.disconnect();
  
  // Set up access point
  WiFi.mode(WIFI_AP);
  String apName = CONFIG_SSID;
  WiFi.softAP(apName.c_str(), CONFIG_PASSWORD);
  
  IPAddress ip = WiFi.softAPIP();
  Serial.println("✓ Access Point Started");
  Serial.println("  SSID: " + apName);
  Serial.println("  Password: " + String(CONFIG_PASSWORD));
  Serial.println("  IP: " + ip.toString());
  Serial.println("  Web Interface: http://" + ip.toString());
  
  // Set up mDNS
  if (MDNS.begin("smartswitch")) {
    Serial.println("  mDNS: http://smartswitch.local");
  }
  
  // Configure web server routes
  setupConfigWebServer();
  server.begin();
  
  Serial.println("\n🌐 Connect to WiFi '" + apName + "' and visit:");
  Serial.println("   http://" + ip.toString() + " or http://smartswitch.local");
  Serial.println("   Password: " + String(CONFIG_PASSWORD));
  Serial.println("\n⏱️  Config mode timeout: " + String(CONFIG_TIMEOUT/1000/60) + " minutes");
  
  // Handle config mode with timeout
  unsigned long configStartTime = millis();
  while (isConfigMode && (millis() - configStartTime < CONFIG_TIMEOUT)) {
    server.handleClient();
    
    // Continue processing switches even in config mode for responsiveness
    processSwitchFSM();
    
    // Blink status LED to indicate config mode
    digitalWrite(STATUS_LED, (millis() / 500) % 2);
    
    // No blocking delay - maximum responsiveness
  }
  
  if (isConfigMode) {
    Serial.println("⏰ Config mode timeout - restarting...");
    ESP.restart();
  }
}

void setupConfigWebServer() {
  // Serve main configuration page
  server.on("/", HTTP_GET, handleConfigRoot);
  
  // Handle WiFi scan
  server.on("/scan", HTTP_GET, handleWiFiScan);
  
  // Handle WiFi configuration
  server.on("/config", HTTP_POST, handleWiFiConfig);
  
  // Handle device info
  server.on("/info", HTTP_GET, handleDeviceInfo);
  
  // Handle restart
  server.on("/restart", HTTP_POST, handleRestart);
  
  // Handle 404
  server.onNotFound(handleNotFound);
}

void handleConfigRoot() {
  String html = generateConfigHTML();
  server.send(200, "text/html", html);
}

void handleWiFiScan() {
  Serial.println("Scanning for WiFi networks...");
  
  String json = "{\"networks\":[";
  int n = WiFi.scanNetworks();
  
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  
  json += "],\"count\":" + String(n) + "}";
  
  server.send(200, "application/json", json);
}

void handleWiFiConfig() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  Serial.println("Received WiFi configuration:");
  Serial.println("  SSID: " + ssid);
  Serial.println("  Password: [" + String(password.length()) + " chars]");
  
  if (ssid.length() > 0) {
    // Save credentials to EEPROM
    saveWiFiCredentials(ssid, password);
    
    // Update global variables
    wifi_ssid = ssid;
    wifi_password = password;
    
    String response = "{\"status\":\"success\",\"message\":\"WiFi credentials saved. Restarting device...\"}";
    server.send(200, "application/json", response);
    
    delay(2000);
    ESP.restart();
  } else {
    String response = "{\"status\":\"error\",\"message\":\"SSID cannot be empty\"}";
    server.send(400, "application/json", response);
  }
}

void handleDeviceInfo() {
  String json = "{";
  json += "\"board_id\":\"" + String(BOARD_ID) + "\",";
  json += "\"firmware\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"mac\":\"" + deviceMAC + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"switches\":" + String(NUM_SWITCHES);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleRestart() {
  String response = "{\"status\":\"success\",\"message\":\"Restarting device...\"}";
  server.send(200, "application/json", response);
  delay(1000);
  ESP.restart();
}

void handleNotFound() {
  String response = "{\"status\":\"error\",\"message\":\"Not found\"}";
  server.send(404, "application/json", response);
}

String generateConfigHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Smart Switch WiFi Config</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);";
  html += "min-height:100vh;padding:20px;margin:0;color:#333;}";
  html += ".container{max-width:500px;margin:0 auto;background:rgba(255,255,255,0.95);";
  html += "border-radius:20px;padding:30px;box-shadow:0 20px 40px rgba(0,0,0,0.1);}";
  html += ".header{text-align:center;margin-bottom:30px;}";
  html += ".header h1{color:#333;margin-bottom:10px;font-size:2.2em;}";
  html += ".section{margin-bottom:25px;padding:20px;background:rgba(255,255,255,0.7);";
  html += "border-radius:15px;}";
  html += ".btn{padding:12px 24px;border:none;border-radius:10px;font-size:1em;";
  html += "font-weight:600;cursor:pointer;margin:5px;transition:all 0.3s;}";
  html += ".btn-primary{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;}";
  html += ".btn-primary:hover{transform:translateY(-2px);box-shadow:0 5px 15px rgba(102,126,234,0.4);}";
  html += ".btn-secondary{background:#f8f9fa;color:#333;border:2px solid #dee2e6;}";
  html += ".btn-danger{background:linear-gradient(135deg,#ff6b6b 0%,#ee5a52 100%);color:white;}";
  html += ".form-group{margin-bottom:20px;}";
  html += ".form-group label{display:block;margin-bottom:8px;font-weight:600;color:#333;}";
  html += ".form-group input{width:100%;padding:12px 15px;border:2px solid #dee2e6;";
  html += "border-radius:10px;font-size:1em;box-sizing:border-box;}";
  html += ".form-group input:focus{border-color:#667eea;outline:none;}";
  html += ".network-list{margin:20px 0;max-height:300px;overflow-y:auto;}";
  html += ".network-item{padding:15px;margin-bottom:10px;background:rgba(255,255,255,0.8);";
  html += "border:2px solid transparent;border-radius:10px;cursor:pointer;transition:all 0.3s;}";
  html += ".network-item:hover{background:rgba(102,126,234,0.1);border-color:#667eea;}";
  html += ".network-item.selected{background:rgba(102,126,234,0.2);border-color:#667eea;}";
  html += ".info-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:20px;}";
  html += ".info-item{padding:12px;background:rgba(255,255,255,0.8);border-radius:8px;}";
  html += ".status{padding:10px;border-radius:8px;margin:10px 0;text-align:center;}";
  html += ".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
  html += ".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>📶 WiFi Setup</h1>";
  html += "<p><strong>" + String(BOARD_ID) + "</strong> Configuration</p>";
  html += "<p><small>Toggle Switch 1 seven times to access this page</small></p>";
  html += "</div>";
  
  html += "<div class='section'>";
  html += "<h2>Available Networks</h2>";
  html += "<button onclick='scanNetworks()' class='btn btn-secondary' id='scanBtn'>🔍 Scan Networks</button>";
  html += "<div id='networkList' class='network-list'></div>";
  html += "</div>";
  
  html += "<div class='section'>";
  html += "<h2>WiFi Credentials</h2>";
  html += "<form onsubmit='submitConfig(event)'>";
  html += "<div class='form-group'>";
  html += "<label for='ssid'>📡 Network Name (SSID):</label>";
  html += "<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi network name'>";
  html += "</div>";
  html += "<div class='form-group'>";
  html += "<label for='password'>🔐 Password:</label>";
  html += "<input type='password' id='password' name='password' placeholder='Enter WiFi password (leave empty for open networks)'>";
  html += "</div>";
  html += "<button type='submit' class='btn btn-primary'>💾 Save & Connect</button>";
  html += "</form>";
  html += "<div id='status'></div>";
  html += "</div>";
  
  html += "<div class='section'>";
  html += "<h2>Device Information</h2>";
  html += "<div class='info-grid'>";
  html += "<div class='info-item'><strong>Board ID:</strong><br>" + String(BOARD_ID) + "</div>";
  html += "<div class='info-item'><strong>Firmware:</strong><br>" + String(FIRMWARE_VERSION) + "</div>";
  html += "<div class='info-item'><strong>MAC Address:</strong><br>" + deviceMAC + "</div>";
  html += "<div class='info-item'><strong>Switches:</strong><br>" + String(NUM_SWITCHES) + " switches</div>";
  html += "</div>";
  html += "<button onclick='restartDevice()' class='btn btn-danger'>🔄 Restart Device</button>";
  html += "</div></div>";
  
  html += "<script>";
  html += "let networks = [];";
  html += "function scanNetworks() {";
  html += "  document.getElementById('scanBtn').textContent = '🔍 Scanning...';";
  html += "  fetch('/scan').then(r => r.json()).then(data => {";
  html += "    networks = data.networks;";
  html += "    displayNetworks(networks);";
  html += "    document.getElementById('scanBtn').textContent = '🔍 Scan Networks';";
  html += "  }).catch(e => {";
  html += "    document.getElementById('scanBtn').textContent = '❌ Scan Failed';";
  html += "    setTimeout(() => document.getElementById('scanBtn').textContent = '🔍 Scan Networks', 2000);";
  html += "  });";
  html += "}";
  html += "function displayNetworks(networks) {";
  html += "  const list = document.getElementById('networkList');";
  html += "  list.innerHTML = '';";
  html += "  if(networks.length === 0) {";
  html += "    list.innerHTML = '<p>No networks found. Try scanning again.</p>';";
  html += "    return;";
  html += "  }";
  html += "  networks.sort((a,b) => b.rssi - a.rssi);";
  html += "  networks.forEach(network => {";
  html += "    const div = document.createElement('div');";
  html += "    div.className = 'network-item';";
  html += "    div.onclick = () => selectNetwork(network.ssid);";
  html += "    const signal = network.rssi > -60 ? '📶📶📶' : network.rssi > -80 ? '📶📶' : '📶';";
  html += "    const secure = network.secure ? '🔒' : '🔓';";
  html += "    div.innerHTML = '<strong>' + network.ssid + '</strong><br>' + secure + ' ' + signal + ' (' + network.rssi + ' dBm)';";
  html += "    list.appendChild(div);";
  html += "  });";
  html += "}";
  html += "function selectNetwork(ssid) {";
  html += "  document.getElementById('ssid').value = ssid;";
  html += "  document.querySelectorAll('.network-item').forEach(item => item.classList.remove('selected'));";
  html += "  event.currentTarget.classList.add('selected');";
  html += "}";
  html += "function submitConfig(event) {";
  html += "  event.preventDefault();";
  html += "  const formData = new FormData(event.target);";
  html += "  const statusDiv = document.getElementById('status');";
  html += "  statusDiv.innerHTML = '<div class=\"status\">⏳ Saving credentials...</div>';";
  html += "  fetch('/config', { method: 'POST', body: formData })";
  html += "    .then(r => r.json())";
  html += "    .then(result => {";
  html += "      if(result.status === 'success') {";
  html += "        statusDiv.innerHTML = '<div class=\"status success\">✅ ' + result.message + '</div>';";
  html += "        setTimeout(() => {";
  html += "          statusDiv.innerHTML = '<div class=\"status\">🔄 Device restarting... Please reconnect to your WiFi network.</div>';";
  html += "        }, 2000);";
  html += "      } else {";
  html += "        statusDiv.innerHTML = '<div class=\"status error\">❌ ' + result.message + '</div>';";
  html += "      }";
  html += "    }).catch(e => {";
  html += "      statusDiv.innerHTML = '<div class=\"status error\">❌ Connection error. Please try again.</div>';";
  html += "    });";
  html += "}";
  html += "function restartDevice() {";
  html += "  if(confirm('Are you sure you want to restart the device?')) {";
  html += "    fetch('/restart', { method: 'POST' });";
  html += "    document.getElementById('status').innerHTML = '<div class=\"status\">🔄 Device restarting...</div>';";
  html += "  }";
  html += "}";
  html += "window.onload = function() { scanNetworks(); };";
  html += "</script>";
  html += "</body></html>";
  
  return html;
}




// ============================================================================
// STATUS PRINTING
// ============================================================================




void printStatus() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         SYSTEM STATUS SUMMARY          ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("Board ID: " + String(BOARD_ID));
  Serial.println("WiFi: " + String(isConnectedToWiFi ? "✓ Connected" : "✗ Disconnected"));
  if (isConnectedToWiFi) {
    Serial.println("  IP: " + WiFi.localIP().toString());
    Serial.println("  Signal: " + String(WiFi.RSSI()) + " dBm");
  }
  Serial.println("Uptime: " + String(millis() / 1000) + "s");



  Serial.println("\nSwitch States:");
  for (int i = 0; i < NUM_SWITCHES; i++) {
    Serial.println("  Switch " + String(i + 1) + ": " + String(switchStates[i] ? "ON ⚡" : "OFF"));
  }



  Serial.println("════════════════════════════════════════\n");
}




// ============================================================================
// END OF CODE
// ============================================================================



