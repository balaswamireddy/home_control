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


#define BOARD_ID "BOARD_001"  // UNIQUE ID for each board (BOARD_001, BOARD_002, etc.)
#define FIRMWARE_VERSION "2.0.0"
#define NUM_SWITCHES 4


// Pin Definitions
#define RELAY_PIN_1 2   // GPIO2 - Switch 1 Relay
#define RELAY_PIN_2 4   // GPIO4 - Switch 2 Relay  
#define RELAY_PIN_3 16  // GPIO16 - Switch 3 Relay
#define RELAY_PIN_4 17  // GPIO17 - Switch 4 Relay


#define BUTTON_PIN_1 18 // GPIO18 - Physical button for Switch 1
#define BUTTON_PIN_2 19 // GPIO19 - Physical button for Switch 2
#define BUTTON_PIN_3 21 // GPIO21 - Physical button for Switch 3
#define BUTTON_PIN_4 22 // GPIO22 - Physical button for Switch 4


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


// Button debouncing
bool lastButtonStates[NUM_SWITCHES] = {false, false, false, false};
unsigned long lastButtonPress[NUM_SWITCHES] = {0, 0, 0, 0};


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

  // Connect to WiFi
  if (wifi_ssid.length() > 0) {
    connectToWiFi();
  } else {
    Serial.println("✗ No WiFi credentials - please set wifi_ssid and wifi_password in code");
    Serial.println("For now, enter your WiFi credentials in the code and re-upload");
    while(1) { delay(1000); }
  }
}


// ============================================================================
// MAIN LOOP
// ============================================================================


void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting reconnection...");
    connectToWiFi();
    delay(5000);
    return;
  }

  // Check physical buttons (every 50ms)
  if (millis() - lastButtonCheck > 50) {
    checkPhysicalButtons();
    lastButtonCheck = millis();
  }

  // Poll database for switch changes (every 2 seconds)
  if (millis() - lastDatabasePoll > 2000) {
    loadSwitchStatesFromDatabase();
    lastDatabasePoll = millis();
  }

  // Send heartbeat (every 30 seconds)
  if (millis() - lastHeartbeat > 30000) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  // Print status summary (every 30 seconds)
  if (millis() - lastStatusPrint > 30000) {
    printStatus();
    lastStatusPrint = millis();
  }

  // Status LED - solid on when connected
  digitalWrite(STATUS_LED, isConnectedToWiFi ? HIGH : LOW);

  delay(10);
}


// ============================================================================
// PIN INITIALIZATION
// ============================================================================


void initializePins() {
  // Initialize relay pins as outputs (active LOW for most relay modules)
  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Turn off relay (assuming active LOW)

    // Initialize button pins as inputs with pullup
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Initialize status LED
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("✓ GPIO pins initialized");
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
        
        // Only update if state changed
        if (switchStates[i] != newState) {
          Serial.println("  [" + String(i + 1) + "] " + name + ": " + String(switchStates[i] ? "ON" : "OFF") + " → " + String(newState ? "ON" : "OFF"));
          switchStates[i] = newState;
          controlRelay(i, newState);
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
  if (switchIndex >= 0 && switchIndex < NUM_SWITCHES) {
    // Most relay modules are active LOW (LOW = ON, HIGH = OFF)
    digitalWrite(relayPins[switchIndex], state ? LOW : HIGH);
    switchStates[switchIndex] = state;
  }
}


void checkPhysicalButtons() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    bool currentButtonState = digitalRead(buttonPins[i]) == LOW; // Active LOW button

    // Debouncing
    if (currentButtonState != lastButtonStates[i]) {
      if (millis() - lastButtonPress[i] > 50) { // 50ms debounce
        if (currentButtonState) { // Button pressed
          // Toggle switch state
          bool newState = !switchStates[i];
          controlRelay(i, newState);

          // Update database
          updateSwitchInDatabase(i, newState, "physical");

          Serial.println("\n[BUTTON] Physical button " + String(i + 1) + " pressed → " + String(newState ? "ON" : "OFF"));
        }
        lastButtonPress[i] = millis();
      }
      lastButtonStates[i] = currentButtonState;
    }
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
