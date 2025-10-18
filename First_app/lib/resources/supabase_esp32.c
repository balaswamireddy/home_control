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
#define FIRMWARE_VERSION "3.0.0-SUPABASE-REALTIME"
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

// Supabase Configuration
String supabase_url = "https://nchshzvzjwlhquvjzhsi.supabase.co";
String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im5jaHNoenZ6andsaHF1dmp6aHNpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjAwNzU4NDIsImV4cCI6MjA3NTY1MTg0Mn0.ASwxbx9m6a09MT8x31qvkSwy2yBLHAVhOMZ3jutLNS8";

// Realtime endpoints
String realtime_switches_endpoint = "/rest/v1/realtime_switches";
String device_status_endpoint = "/rest/v1/device_status";

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
unsigned long lastRealtimeSync = 0;

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
  Serial.println("║   SMART HOME ESP32 CONTROLLER v3.0.0   ║");
  Serial.println("║     SUPABASE REALTIME + SQL VERSION     ║");
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
  unsigned long lastRealtime = 0;
  
  Serial.println("[BACKGROUND TASK] Supabase realtime task started on Core 0");
  
  for(;;) {
    if (isConnectedToWiFi) {
      // Process pending updates
      if (queueCount > 0 && millis() - lastUpdate > 500) {
        processPendingUpdate();
        lastUpdate = millis();
      }
      
      // Poll for remote changes
      if (millis() - lastPoll > 5000) {
        pollSupabaseForChanges();
        lastPoll = millis();
      }
      
      // Send heartbeat
      if (millis() - lastHeartbeat > 30000) {
        sendHeartbeat();
        lastHeartbeat = millis();
      }

      // Sync with realtime tables
      if (millis() - lastRealtime > 2000) {
        syncRealtimeData();
        lastRealtime = millis();
      }
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
    pinMode(buttonPins[i], INPUT_PULLUP);
    digitalWrite(relayPins[i], LOW);
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
    Serial.println("  ⚠ No valid credentials found");
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
  html += "<h1>🏠 Smart Home Config (Supabase)</h1>";
  html += "<div class='board-id'>Board ID: " + String(BOARD_ID) + "</div>";
  html += "<div class='info'>Connect your smart home controller to WiFi. This device now uses Supabase for real-time communication.</div>";
  
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
    for (int i = 0; i < n; ++i) {
      int strength = WiFi.RSSI(i);
      String encryption = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
      String signal = String(strength) + "dBm";
      
      html += "<div class='network-item' onclick='selectNetwork(\"" + WiFi.SSID(i) + "\")'>";
      html += "<strong>" + WiFi.SSID(i) + "</strong>";
      html += "<span class='signal'>" + signal + " (" + encryption + ")</span>";
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
    Serial.println("✓ WiFi Connected!");
    Serial.println("IP Address: " + WiFi.localIP().toString());
    Serial.println("Gateway: " + WiFi.gatewayIP().toString());
    Serial.println("Subnet: " + WiFi.subnetMask().toString());
    Serial.println("DNS: " + WiFi.dnsIP().toString());
    Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");

    // Test Supabase connection
    testSupabaseConnection();

    // Initialize board in database
    initializeBoardInDatabase();

    // Start background task for database operations
    xTaskCreatePinnedToCore(
      backgroundDatabaseTask,
      "DatabaseTask",
      10000,
      NULL,
      1,
      &backgroundTaskHandle,
      0
    );
    Serial.println("✓ Background Supabase task started on Core 0");
  } else {
    isConnectedToWiFi = false;
    Serial.println("✗ WiFi Connection Failed!");
    Serial.println("Entering configuration mode...");
    enterConfigMode();
  }
}

// ============================================================================
// SUPABASE OPERATIONS
// ============================================================================

bool testSupabaseConnection() {
  Serial.println("\n[SUPABASE] Testing connection...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = supabase_url + "/rest/v1/boards?id=eq." + String(BOARD_ID) + "&select=id";
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.setTimeout(3000);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    Serial.println("  ✓ Supabase connection successful");
    isConnectedToSupabase = true;
    http.end();
    return true;
  } else {
    Serial.println("  ✗ Supabase connection failed: " + String(httpResponseCode));
    isConnectedToSupabase = false;
    http.end();
    return false;
  }
}

bool initializeBoardInDatabase() {
  Serial.println("\n[SUPABASE] Initializing board in database...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  // Initialize board
  String url = supabase_url + "/rest/v1/boards";
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "resolution=merge-duplicates");
  
  DynamicJsonDocument doc(512);
  doc["id"] = BOARD_ID;
  doc["name"] = "ESP32 Smart Switch " + String(BOARD_ID);
  doc["status"] = "online";
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["last_online"] = "now()";
  doc["mac_address"] = deviceMAC;
  doc["is_active"] = true;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.POST(requestBody);
  http.end();
  
  // Initialize switches
  for (int i = 0; i < NUM_SWITCHES; i++) {
    String switchId = String(BOARD_ID) + "_switch_" + String(i + 1);
    
    url = supabase_url + "/rest/v1/switches";
    http.begin(client, url);
    http.addHeader("apikey", supabase_key);
    http.addHeader("Authorization", "Bearer " + supabase_key);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "resolution=merge-duplicates");
    
    DynamicJsonDocument switchDoc(512);
    switchDoc["id"] = switchId;
    switchDoc["board_id"] = BOARD_ID;
    switchDoc["name"] = "Switch " + String(i + 1);
    switchDoc["position"] = i;
    switchDoc["state"] = switchStates[i];
    switchDoc["is_enabled"] = true;
    
    String switchBody;
    serializeJson(switchDoc, switchBody);
    
    httpResponseCode = http.POST(switchBody);
    http.end();
  }
  
  // Initialize device status
  syncDeviceStatus(true);
  
  Serial.println("  ✓ Board initialized in database");
  return true;
}

void pollSupabaseForChanges() {
  if (!isConnectedToWiFi) return;
  
  Serial.println("\n[POLL] Checking for remote changes...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = supabase_url + "/rest/v1/realtime_switches?board_id=eq." + String(BOARD_ID) + "&order=switch_index";
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.setTimeout(3000);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);
    
    JsonArray switches = doc.as<JsonArray>();
    
    bool changesDetected = false;
    for (JsonObject switchObj : switches) {
      int switchIndex = switchObj["switch_index"];
      bool remoteState = switchObj["state"];
      
      if (switchIndex >= 0 && switchIndex < NUM_SWITCHES) {
        if (switchStates[switchIndex] != remoteState) {
          Serial.println("  [REMOTE CHANGE] Switch " + String(switchIndex + 1) + " → " + String(remoteState ? "ON" : "OFF"));
          controlRelay(switchIndex, remoteState);
          changesDetected = true;
        }
      }
    }
    
    if (!changesDetected) {
      Serial.println("  ✓ No remote changes detected");
    }
  } else {
    Serial.println("  ✗ Failed to poll changes: " + String(httpResponseCode));
  }
  
  http.end();
}

void processPendingUpdate() {
  if (queueCount == 0) return;
  
  SwitchUpdate update = updateQueue[queueTail];
  
  Serial.println("\n[UPDATE] Sending Switch " + String(update.switchIndex + 1) + " → " + String(update.state ? "ON" : "OFF"));
  
  // Update main switches table
  updateMainSwitchTable(update.switchIndex, update.state);
  
  // Update realtime table
  updateRealtimeTable(update.switchIndex, update.state);
  
  queueTail = (queueTail + 1) % 10;
  queueCount--;
  
  Serial.println("  [QUEUE] " + String(queueCount) + " updates remaining");
}

bool updateMainSwitchTable(int switchIndex, bool state) {
  WiFiClientSecure client;
  client.setInsecure();
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
  doc["last_state_change"] = "now()";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.sendRequest("PATCH", requestBody);
  bool success = (httpResponseCode == 200 || httpResponseCode == 204);
  
  if (success) {
    Serial.println("  ✓ Main table updated");
  } else {
    Serial.println("  ✗ Main table update failed: " + String(httpResponseCode));
  }
  
  http.end();
  return success;
}

bool updateRealtimeTable(int switchIndex, bool state) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String switchId = String(BOARD_ID) + "_switch_" + String(switchIndex + 1);
  String url = supabase_url + "/rest/v1/realtime_switches?id=eq." + switchId;
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  
  DynamicJsonDocument doc(256);
  doc["state"] = state;
  doc["last_updated"] = "now()";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.sendRequest("PATCH", requestBody);
  bool success = (httpResponseCode == 200 || httpResponseCode == 204);
  
  if (success) {
    Serial.println("  ✓ Realtime table updated");
  } else {
    Serial.println("  ✗ Realtime table update failed: " + String(httpResponseCode));
  }
  
  http.end();
  return success;
}

void syncRealtimeData() {
  // Ensure realtime table is in sync with our current states
  for (int i = 0; i < NUM_SWITCHES; i++) {
    String switchId = String(BOARD_ID) + "_switch_" + String(i + 1);
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    
    String url = supabase_url + "/rest/v1/realtime_switches";
    
    http.begin(client, url);
    http.addHeader("apikey", supabase_key);
    http.addHeader("Authorization", "Bearer " + supabase_key);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "resolution=merge-duplicates");
    
    DynamicJsonDocument doc(256);
    doc["id"] = switchId;
    doc["board_id"] = BOARD_ID;
    doc["switch_index"] = i;
    doc["state"] = switchStates[i];
    doc["last_updated"] = "now()";
    
    String requestBody;
    serializeJson(doc, requestBody);
    
    http.POST(requestBody);
    http.end();
  }
}

void syncDeviceStatus(bool online) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = supabase_url + "/rest/v1/device_status";
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "resolution=merge-duplicates");
  
  DynamicJsonDocument doc(512);
  doc["board_id"] = BOARD_ID;
  doc["online"] = online;
  doc["last_seen"] = "now()";
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["ip_address"] = WiFi.localIP().toString();
  
  JsonObject metadata = doc.createNestedObject("metadata");
  metadata["mac_address"] = deviceMAC;
  metadata["rssi"] = WiFi.RSSI();
  metadata["free_heap"] = ESP.getFreeHeap();
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  http.POST(requestBody);
  http.end();
}

void sendHeartbeat() {
  if (!isConnectedToWiFi) return;
  
  Serial.println("\n[HEARTBEAT] Sending status update...");
  
  // Update main board table
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = supabase_url + "/rest/v1/boards?id=eq." + String(BOARD_ID);
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  
  DynamicJsonDocument doc(256);
  doc["status"] = "online";
  doc["last_online"] = "now()";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.sendRequest("PATCH", requestBody);
  http.end();
  
  // Update device status table
  syncDeviceStatus(true);
  
  if (httpResponseCode == 200 || httpResponseCode == 204) {
    Serial.println("  ✓ Heartbeat sent successfully");
  } else {
    Serial.println("  ✗ Heartbeat failed: " + String(httpResponseCode));
  }
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

void queueDatabaseUpdate(int switchIndex, bool state) {
  if (queueCount >= 10) {
    Serial.println("[QUEUE] Queue full, dropping oldest update");
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
    bool currentReading = digitalRead(buttonPins[i]) == LOW;
    
    if (currentReading != debounceStates[i].lastRawReading) {
      debounceStates[i].lastDebounceTime = millis();
      debounceStates[i].stableReadCount = 0;
    }
    
    if ((millis() - debounceStates[i].lastDebounceTime) > DEBOUNCE_DELAY) {
      if (currentReading == debounceStates[i].currentReading) {
        debounceStates[i].stableReadCount++;
      } else {
        debounceStates[i].currentReading = currentReading;
        debounceStates[i].stableReadCount = 1;
      }
      
      if (debounceStates[i].stableReadCount >= STABLE_READ_COUNT) {
        if (debounceStates[i].currentReading != debounceStates[i].lastStableState) {
          debounceStates[i].lastStableState = debounceStates[i].currentReading;
          
          if (debounceStates[i].currentReading) {
            if (i == 0) {
              handleConfigTrigger();
            }
            
            bool newState = !switchStates[i];
            controlRelay(i, newState);
            queueDatabaseUpdate(i, newState);
          }
        }
      }
    }
    
    debounceStates[i].lastRawReading = currentReading;
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
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
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
    Serial.println("[RELAY] Switch " + String(switchIndex + 1) + " → " + String(state ? "ON" : "OFF"));
  }
}

// ============================================================================
// STATUS PRINTING
// ============================================================================

void printStatus() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║       SUPABASE SYSTEM STATUS           ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("Board ID: " + String(BOARD_ID));
  Serial.println("Firmware: " + String(FIRMWARE_VERSION));
  Serial.println("Mode: " + String(isConfigMode ? "CONFIG MODE" : "NORMAL"));
  Serial.println("WiFi: " + String(isConnectedToWiFi ? "✓ Connected" : "✗ Disconnected"));
  Serial.println("Supabase: " + String(isConnectedToSupabase ? "✓ Connected" : "✗ Disconnected"));
  if (isConnectedToWiFi) {
    Serial.println("IP: " + WiFi.localIP().toString());
    Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  }
  Serial.println("Pending Updates: " + String(queueCount));
  Serial.println("Uptime: " + String(millis() / 1000) + "s");
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");

  Serial.println("\nSwitch States:");
  for (int i = 0; i < NUM_SWITCHES; i++) {
    Serial.println("  Switch " + String(i + 1) + ": " + String(switchStates[i] ? "ON" : "OFF"));
  }

  Serial.println("\n💡 Press Switch 1 seven times to enter config mode");
  Serial.println("🔄 Real-time sync with Supabase enabled");
  Serial.println("════════════════════════════════════════\n");
}

// ============================================================================
// END OF CODE
// ============================================================================