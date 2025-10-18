#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

// ============================================================================
// BOARD CONFIGURATION - CUSTOMIZE FOR EACH DEVICE
// ============================================================================

#define BOARD_ID "BOARD_005"
#define FIRMWARE_VERSION "2.2.0-CONFIG-PORTAL"
#define NUM_SWITCHES 4

// Pin Definitions
#define RELAY_PIN_1 13
#define RELAY_PIN_2 12
#define RELAY_PIN_3 14
#define RELAY_PIN_4 27

#define BUTTON_PIN_1 26
#define BUTTON_PIN_2 25
#define BUTTON_PIN_3 33
#define BUTTON_PIN_4 32

#define STATUS_LED 2
#define RESET_PIN 0

// Debounce Configuration
#define DEBOUNCE_DELAY 50
#define STABLE_READ_COUNT 3
#define DEBOUNCE_CHECK_INTERVAL 10

// Config Mode Trigger
#define CONFIG_TRIGGER_COUNT 7        // Number of presses to enter config mode
#define CONFIG_TRIGGER_TIMEOUT 3000   // Time window for presses (3 seconds)

// EEPROM Configuration
#define EEPROM_SIZE 512
#define EEPROM_SSID_ADDR 0
#define EEPROM_PASS_ADDR 100
#define EEPROM_MAGIC_ADDR 200
#define EEPROM_MAGIC_VALUE 0xAB  // Magic byte to check if EEPROM has valid data

// Access Point Configuration
#define AP_SSID "SmartSwitch_" BOARD_ID
#define AP_PASSWORD "12345678"
#define AP_TIMEOUT 300000  // 5 minutes

// Supabase Configuration (will be used after WiFi is configured)
String supabase_url = "https://nchshzvzjwlhquvjzhsi.supabase.co";
String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im5jaHNoenZ6andsaHF1dmp6aHNpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjAwNzU4NDIsImV4cCI6MjA3NTY1MTg0Mn0.ASwxbx9m6a09MT8x31qvkSwy2yBLHAVhOMZ3jutLNS8";

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

WebServer server(80);
DNSServer dnsServer;

// WiFi credentials (loaded from EEPROM)
String wifi_ssid = "";
String wifi_password = "";

// Switch states
bool switchStates[NUM_SWITCHES] = {false, false, false, false};
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

// Config mode trigger detection
int configTriggerCount = 0;
unsigned long configTriggerFirstPress = 0;
bool configTriggerDetected = false;

// Enhanced debouncing variables
struct DebounceState {
  bool lastStableState;
  bool currentReading;
  unsigned long lastDebounceTime;
  int stableReadCount;
  bool lastRawReading;
};

DebounceState debounceStates[NUM_SWITCHES];

// Update queue
struct SwitchUpdate {
  int switchIndex;
  bool state;
  bool pending;
  unsigned long timestamp;
};

SwitchUpdate updateQueue[10];
int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

TaskHandle_t backgroundTaskHandle = NULL;

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  SMART HOME ESP32 CONTROLLER v2.2.0   ║");
  Serial.println("║      CONFIG PORTAL + FSM VERSION       ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("Board ID: " + String(BOARD_ID));
  Serial.println("Firmware: " + String(FIRMWARE_VERSION));

  deviceMAC = WiFi.macAddress();
  Serial.println("MAC Address: " + deviceMAC);
  Serial.println("");

  initializePins();
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize debounce states
  for (int i = 0; i < NUM_SWITCHES; i++) {
    debounceStates[i].lastStableState = false;
    debounceStates[i].currentReading = false;
    debounceStates[i].lastDebounceTime = 0;
    debounceStates[i].stableReadCount = 0;
    debounceStates[i].lastRawReading = digitalRead(buttonPins[i]) == LOW;
  }

  // Load WiFi credentials from EEPROM
  loadCredentials();

  // Check if credentials exist
  if (wifi_ssid.length() > 0) {
    Serial.println("✓ WiFi credentials found in EEPROM");
    Serial.println("SSID: " + wifi_ssid);
    connectToWiFi();
  } else {
    Serial.println("⚠ No WiFi credentials found!");
    Serial.println("Press Switch 1 seven times to enter config mode");
  }
  
  Serial.println("\n✓ Setup complete");
  Serial.println("Press Switch 1 " + String(CONFIG_TRIGGER_COUNT) + " times within " + String(CONFIG_TRIGGER_TIMEOUT/1000) + " seconds to enter config mode\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Handle config mode
  if (isConfigMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Blink LED in config mode
    digitalWrite(STATUS_LED, (millis() / 500) % 2);
    delay(10);
    return;
  }

  // Check physical buttons with debouncing
  if (millis() - lastButtonCheck > DEBOUNCE_CHECK_INTERVAL) {
    checkPhysicalButtonsWithDebounce();
    lastButtonCheck = millis();
  }

  // Status LED
  digitalWrite(STATUS_LED, isConnectedToWiFi ? HIGH : LOW);

  // Print status
  if (millis() - lastStatusPrint > 30000) {
    printStatus();
    lastStatusPrint = millis();
  }

  delay(1);
}

// ============================================================================
// BACKGROUND DATABASE TASK
// ============================================================================

void backgroundDatabaseTask(void * parameter) {
  unsigned long lastPoll = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastUpdate = 0;
  
  Serial.println("[BACKGROUND TASK] Database task started on Core 0");
  
  for(;;) {
    if (WiFi.status() != WL_CONNECTED || isConfigMode) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    
    if (queueCount > 0 && millis() - lastUpdate > 200) {
      processPendingUpdate();
      lastUpdate = millis();
      vTaskDelay(200 / portTICK_PERIOD_MS);
      continue;
    }
    
    if (millis() - lastPoll > 2000) {
      pollDatabaseForChanges();
      lastPoll = millis();
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }
    
    if (millis() - lastHeartbeat > 30000) {
      sendHeartbeat();
      lastHeartbeat = millis();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// PIN INITIALIZATION
// ============================================================================

void initializePins() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(STATUS_LED, OUTPUT);

  Serial.println("✓ GPIO pins initialized");
  Serial.println("✓ All relays initialized to OFF state (LOW)");
}

// ============================================================================
// EEPROM FUNCTIONS
// ============================================================================

void loadCredentials() {
  Serial.println("\n[EEPROM] Loading WiFi credentials...");
  
  // Check magic byte
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    Serial.println("  ✗ No valid credentials stored");
    return;
  }
  
  // Read SSID
  wifi_ssid = "";
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(EEPROM_SSID_ADDR + i);
    if (c == 0) break;
    wifi_ssid += c;
  }
  
  // Read Password
  wifi_password = "";
  for (int i = 0; i < 64; i++) {
    char c = EEPROM.read(EEPROM_PASS_ADDR + i);
    if (c == 0) break;
    wifi_password += c;
  }
  
  Serial.println("  ✓ Credentials loaded");
  Serial.println("  SSID: " + wifi_ssid);
}

void saveCredentials(String ssid, String password) {
  Serial.println("\n[EEPROM] Saving WiFi credentials...");
  
  // Clear existing data
  for (int i = 0; i < 100; i++) {
    EEPROM.write(EEPROM_SSID_ADDR + i, 0);
    EEPROM.write(EEPROM_PASS_ADDR + i, 0);
  }
  
  // Write SSID
  for (int i = 0; i < ssid.length(); i++) {
    EEPROM.write(EEPROM_SSID_ADDR + i, ssid[i]);
  }
  
  // Write Password
  for (int i = 0; i < password.length(); i++) {
    EEPROM.write(EEPROM_PASS_ADDR + i, password[i]);
  }
  
  // Write magic byte
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  
  EEPROM.commit();
  Serial.println("  ✓ Credentials saved");
}

void clearCredentials() {
  Serial.println("\n[EEPROM] Clearing credentials...");
  EEPROM.write(EEPROM_MAGIC_ADDR, 0);
  EEPROM.commit();
  Serial.println("  ✓ Credentials cleared");
}

// ============================================================================
// CONFIG MODE FUNCTIONS
// ============================================================================

void enterConfigMode() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║     ENTERING CONFIGURATION MODE        ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  isConfigMode = true;
  
  // Stop background task if running
  if (backgroundTaskHandle != NULL) {
    vTaskDelete(backgroundTaskHandle);
    backgroundTaskHandle = NULL;
  }
  
  // Disconnect from WiFi
  WiFi.disconnect();
  
  // Setup Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("✓ Access Point started");
  Serial.println("SSID: " + String(AP_SSID));
  Serial.println("Password: " + String(AP_PASSWORD));
  Serial.println("IP Address: " + IP.toString());
  
  // Setup DNS server for captive portal
  dnsServer.start(53, "*", IP);
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);  // Captive portal redirect
  
  server.begin();
  Serial.println("✓ Web server started");
  Serial.println("\nConnect to WiFi: " + String(AP_SSID));
  Serial.println("Browse to: http://" + IP.toString());
  Serial.println("════════════════════════════════════════\n");
  
  // Flash all relays to indicate config mode
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < NUM_SWITCHES; j++) {
      digitalWrite(relayPins[j], HIGH);
    }
    delay(200);
    for (int j = 0; j < NUM_SWITCHES; j++) {
      digitalWrite(relayPins[j], LOW);
    }
    delay(200);
  }
}

// ============================================================================
// WEB SERVER HANDLERS
// ============================================================================

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: Arial; margin: 0; padding: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
  html += ".board-id { text-align: center; color: #666; font-size: 14px; margin-bottom: 20px; }";
  html += "label { display: block; margin-top: 15px; color: #555; font-weight: bold; }";
  html += "input, select { width: 100%; padding: 12px; margin-top: 5px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; font-size: 16px; }";
  html += "button { width: 100%; padding: 15px; margin-top: 20px; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; font-weight: bold; }";
  html += "button:hover { background: #45a049; }";
  html += ".scan-btn { background: #008CBA; margin-top: 10px; }";
  html += ".scan-btn:hover { background: #007399; }";
  html += ".networks { margin-top: 10px; }";
  html += ".network-item { padding: 10px; background: #f9f9f9; margin: 5px 0; border-radius: 5px; cursor: pointer; border: 2px solid transparent; }";
  html += ".network-item:hover { background: #e8f5e9; border-color: #4CAF50; }";
  html += ".signal { float: right; color: #666; font-size: 12px; }";
  html += ".info { background: #e3f2fd; padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3; }";
  html += "</style>";
  html += "<script>";
  html += "function selectNetwork(ssid) {";
  html += "  document.getElementById('ssid').value = ssid;";
  html += "  document.getElementById('password').focus();";
  html += "}";
  html += "function scanNetworks() {";
  html += "  document.getElementById('networks').innerHTML = '<p>Scanning...</p>';";
  html += "  fetch('/scan').then(r => r.text()).then(data => {";
  html += "    document.getElementById('networks').innerHTML = data;";
  html += "  });";
  html += "}";
  html += "window.onload = function() { scanNetworks(); };";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🏠 Smart Home Config</h1>";
  html += "<div class='board-id'>Board ID: " + String(BOARD_ID) + "</div>";
  html += "<div class='info'>Connect your smart home controller to WiFi by selecting a network and entering the password.</div>";
  
  html += "<button class='scan-btn' onclick='scanNetworks()'>🔄 Scan WiFi Networks</button>";
  html += "<div id='networks' class='networks'></div>";
  
  html += "<form action='/save' method='POST'>";
  html += "<label>WiFi Network (SSID):</label>";
  html += "<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name'>";
  html += "<label>Password:</label>";
  html += "<input type='password' id='password' name='password' required placeholder='Enter WiFi password'>";
  html += "<button type='submit'>💾 Save & Connect</button>";
  html += "</form>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleScan() {
  Serial.println("[CONFIG] Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  String html = "";
  
  if (n == 0) {
    html = "<p>No networks found</p>";
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encryption = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓" : "🔒";
      
      // Signal strength indicator
      String signal = "";
      if (rssi > -50) signal = "📶 Excellent";
      else if (rssi > -60) signal = "📶 Good";
      else if (rssi > -70) signal = "📶 Fair";
      else signal = "📶 Weak";
      
      html += "<div class='network-item' onclick='selectNetwork(\"" + ssid + "\")'>";
      html += encryption + " " + ssid;
      html += "<span class='signal'>" + signal + "</span>";
      html += "</div>";
    }
  }
  
  server.send(200, "text/html", html);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  Serial.println("\n[CONFIG] Received WiFi credentials:");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + String(password.length()) + " characters");
  
  // Save to EEPROM
  saveCredentials(ssid, password);
  
  // Send success page
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; padding: 50px; background: #f0f0f0; }";
  html += ".success { max-width: 500px; margin: 0 auto; background: white; padding: 40px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #4CAF50; }";
  html += "p { color: #666; line-height: 1.6; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='success'>";
  html += "<h1>✓ Configuration Saved!</h1>";
  html += "<p>Your smart home controller will now restart and connect to:<br><b>" + ssid + "</b></p>";
  html += "<p>The device will restart in 3 seconds...</p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  delay(3000);
  Serial.println("\n[SYSTEM] Restarting to apply new WiFi configuration...");
  ESP.restart();
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

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    isConnectedToWiFi = true;
    Serial.println("✓ WiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    Serial.println("Signal strength: " + String(WiFi.RSSI()) + " dBm");

    isConnectedToSupabase = true;

    Serial.println("\n════════════════════════════════════════");
    Serial.println("✓ BOARD READY");
    Serial.println("════════════════════════════════════════\n");
    
    // Start background task
    xTaskCreatePinnedToCore(
      backgroundDatabaseTask,
      "DatabaseTask",
      8192,
      NULL,
      1,
      &backgroundTaskHandle,
      0
    );
    Serial.println("✓ Background database task started on Core 0");
  } else {
    Serial.println("✗ WiFi connection failed!");
    Serial.println("Press Switch 1 seven times to reconfigure WiFi");
    isConnectedToWiFi = false;
  }
}

// ============================================================================
// DATABASE OPERATIONS
// ============================================================================

void pollDatabaseForChanges() {
  if (!isConnectedToWiFi) return;
  
  Serial.println("\n[POLL] Checking for remote changes...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = supabase_url + "/rest/v1/switches?board_id=eq." + String(BOARD_ID) + "&order=position&select=id,name,state,position";
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      JsonArray switches = doc.as<JsonArray>();
      
      bool anyChanges = false;
      for (int i = 0; i < switches.size() && i < NUM_SWITCHES; i++) {
        bool remoteState = switches[i]["state"];
        
        String name;
        if (switches[i]["name"].isNull()) {
          name = "Switch " + String(i + 1);
        } else {
          name = switches[i]["name"].as<String>();
        }
        
        if (switchStates[i] != remoteState) {
          Serial.println("  [REMOTE] " + name + ": " + String(switchStates[i] ? "ON" : "OFF") + " → " + String(remoteState ? "ON" : "OFF"));
          controlRelay(i, remoteState);
          anyChanges = true;
        }
      }
      
      if (!anyChanges) {
        Serial.println("  ✓ No remote changes");
      } else {
        Serial.println("  ✓ Remote changes applied");
      }
    }
  } else if (httpResponseCode > 0) {
    Serial.println("  ✗ HTTP error: " + String(httpResponseCode));
  }
  
  http.end();
}

void processPendingUpdate() {
  if (queueCount == 0) return;
  
  SwitchUpdate update = updateQueue[queueTail];
  
  Serial.println("\n[UPDATE] Sending Switch " + String(update.switchIndex + 1) + " → " + String(update.state ? "ON" : "OFF"));
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String switchId = String(BOARD_ID) + "_switch_" + String(update.switchIndex + 1);
  String url = supabase_url + "/rest/v1/switches?id=eq." + switchId;
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  http.setTimeout(3000);
  
  DynamicJsonDocument doc(256);
  doc["state"] = update.state;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.sendRequest("PATCH", requestBody);
  
  if (httpResponseCode == 200 || httpResponseCode == 204) {
    Serial.println("  ✓ Database updated successfully");
  } else if (httpResponseCode > 0) {
    Serial.println("  ✗ Update failed: HTTP " + String(httpResponseCode));
  }
  
  http.end();
  
  queueTail = (queueTail + 1) % 10;
  queueCount--;
  
  Serial.println("  [QUEUE] " + String(queueCount) + " updates remaining");
}

void sendHeartbeat() {
  if (!isConnectedToWiFi) return;
  
  Serial.println("\n[HEARTBEAT] Sending status update...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = supabase_url + "/rest/v1/boards?id=eq." + String(BOARD_ID);
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  http.setTimeout(3000);
  
  DynamicJsonDocument doc(256);
  doc["status"] = "online";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.sendRequest("PATCH", requestBody);
  
  if (httpResponseCode == 200 || httpResponseCode == 204) {
    Serial.println("  ✓ Heartbeat sent");
  }
  
  http.end();
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

void queueDatabaseUpdate(int switchIndex, bool state) {
  if (queueCount >= 10) {
    Serial.println("[QUEUE] Queue full! Dropping oldest update");
    queueTail = (queueTail + 1) % 10;
    queueCount--;
  }
  
  updateQueue[queueHead].switchIndex = switchIndex;
  updateQueue[queueHead].state = state;
  updateQueue[queueHead].pending = true;
  updateQueue[queueHead].timestamp = millis();
  
  queueHead = (queueHead + 1) % 10;
  queueCount++;
  
  Serial.println("[QUEUE] Switch " + String(switchIndex + 1) + " update queued (" + String(queueCount) + " in queue)");
}

// ============================================================================
// ENHANCED DEBOUNCING
// ============================================================================

void checkPhysicalButtonsWithDebounce() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    bool rawReading = digitalRead(buttonPins[i]) == LOW;
    
    if (rawReading != debounceStates[i].lastRawReading) {
      debounceStates[i].lastDebounceTime = millis();
      debounceStates[i].stableReadCount = 0;
      debounceStates[i].lastRawReading = rawReading;
    } else {
      if (millis() - debounceStates[i].lastDebounceTime > DEBOUNCE_DELAY) {
        
        if (debounceStates[i].stableReadCount < STABLE_READ_COUNT) {
          debounceStates[i].stableReadCount++;
        }
        
        if (debounceStates[i].stableReadCount >= STABLE_READ_COUNT) {
          
          if (rawReading != debounceStates[i].lastStableState) {
            
            debounceStates[i].lastStableState = rawReading;
            
            // Special handling for Switch 1 - Config mode trigger
            if (i == 0 && rawReading) {
              handleConfigTrigger();
            }
            
            if (rawReading) {
              Serial.println("\n[PHYSICAL] Switch " + String(i + 1) + " pulled LOW (debounced)");
              controlRelay(i, true);
              queueDatabaseUpdate(i, true);
              
            } else {
              Serial.println("\n[PHYSICAL] Switch " + String(i + 1) + " released HIGH (debounced)");
              controlRelay(i, false);
              queueDatabaseUpdate(i, false);
            }
          }
        }
      }
    }
  }
}

void handleConfigTrigger() {
  unsigned long now = millis();
  
  // Reset counter if timeout exceeded
  if (now - configTriggerFirstPress > CONFIG_TRIGGER_TIMEOUT) {
    configTriggerCount = 0;
  }
  
  // Increment counter
  if (configTriggerCount == 0) {
    configTriggerFirstPress = now;
  }
  configTriggerCount++;
  
  Serial.println("[CONFIG TRIGGER] Press " + String(configTriggerCount) + "/" + String(CONFIG_TRIGGER_COUNT));
  
  // Flash LED to indicate progress
  for (int i = 0; i < configTriggerCount; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(50);
    digitalWrite(STATUS_LED, LOW);
    delay(50);
  }
  
  // Enter config mode if trigger count reached
  if (configTriggerCount >= CONFIG_TRIGGER_COUNT) {
    configTriggerCount = 0;
    enterConfigMode();
  }
}

// ============================================================================
// RELAY CONTROL
// ============================================================================

void controlRelay(int switchIndex, bool state) {
  if (switchIndex >= 0 && switchIndex < NUM_SWITCHES) {
    digitalWrite(relayPins[switchIndex], state ? HIGH : LOW);
    switchStates[switchIndex] = state;
    
    Serial.println("[RELAY] Switch " + String(switchIndex + 1) + " → " + String(state ? "ON (HIGH)" : "OFF (LOW)"));
  }
}

// ============================================================================
// STATUS PRINTING
// ============================================================================

void printStatus() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         SYSTEM STATUS SUMMARY          ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("Board ID: " + String(BOARD_ID));
  Serial.println("Mode: " + String(isConfigMode ? "CONFIG MODE" : "NORMAL"));
  Serial.println("WiFi: " + String(isConnectedToWiFi ? "✓ Connected" : "✗ Disconnected"));
  if (isConnectedToWiFi) {
    Serial.println("  SSID: " + wifi_ssid);
    Serial.println("  IP: " + WiFi.localIP().toString());
    Serial.println("  Signal: " + String(WiFi.RSSI()) + " dBm");
  }
  Serial.println("Pending Updates: " + String(queueCount));
  Serial.println("Uptime: " + String(millis() / 1000) + "s");
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");

  Serial.println("\nSwitch States:");
  for (int i = 0; i < NUM_SWITCHES; i++) {
    Serial.println("  Switch " + String(i + 1) + ": " + String(switchStates[i] ? "ON ⚡" : "OFF"));
  }

  Serial.println("\n💡 Press Switch 1 seven times to enter config mode");
  Serial.println("════════════════════════════════════════\n");
}

// ============================================================================
// END OF CODE
// ============================================================================
