#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

// =====================================================
// DEFAULT CONFIGURATION
// =====================================================

const char* DEFAULT_WIFI_SSID     = "Daves_5G";
const char* DEFAULT_WIFI_PASSWORD = "guddu+2375";
const char* DEFAULT_SERVER_URL    = "http://192.168.43.125:5000";

const char* CONFIG_FILE = "/config.json";

// Current configuration
String WIFI_SSID;
String WIFI_PASSWORD;
String SERVER_URL;
ESP8266WebServer statusServer(80);

// =====================================================
// RC522 PINS
// =====================================================

#define SS_PIN  D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN);

// =====================================================
// MIFARE CLASSIC KEY A
// =====================================================

MFRC522::MIFARE_Key key;

byte ndefKey[6] = {
  0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7
};

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void setDefaultConfig();
bool loadSavedConfig();
bool saveConfig();
void printCurrentConfig();
void serialConfigMode();
bool connectWiFi();
String readNDEFText();
bool verifyCard(String number);
bool checkForConfigCommand();
void handleStatus();

// =====================================================
// DEFAULT CONFIG
// =====================================================

void setDefaultConfig() {
  WIFI_SSID = DEFAULT_WIFI_SSID;
  WIFI_PASSWORD = DEFAULT_WIFI_PASSWORD;
  SERVER_URL = DEFAULT_SERVER_URL;
}

// =====================================================
// SAVE CONFIG TO LITTLEFS
// =====================================================

bool saveConfig() {
  Serial.println();
  Serial.println("Saving configuration...");

  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println("ERROR: Could not open config file.");
    return false;
  }

  JsonDocument doc;
  doc["ssid"] = WIFI_SSID;
  doc["password"] = WIFI_PASSWORD;
  doc["server"] = SERVER_URL;

  if (serializeJson(doc, file) == 0) {
    Serial.println("ERROR: Failed to write configuration.");
    file.close();
    return false;
  }
  file.close();
  Serial.println("Configuration saved to LittleFS.");
  return true;
}

// =====================================================
// LOAD CONFIG FROM LITTLEFS
// =====================================================

bool loadSavedConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("No saved configuration found.");
    return false;
  }

  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println("ERROR: Could not open config file.");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("ERROR: Invalid configuration file.");
    return false;
  }

  if (!doc["ssid"].is<String>() || !doc["password"].is<String>() || !doc["server"].is<String>()) {
    Serial.println("ERROR: Configuration fields missing.");
    return false;
  }

  WIFI_SSID = doc["ssid"].as<String>();
  WIFI_PASSWORD = doc["password"].as<String>();
  SERVER_URL = doc["server"].as<String>();

  Serial.println("Saved configuration loaded.");
  return true;
}

// =====================================================
// PRINT CURRENT CONFIG
// =====================================================

void printCurrentConfig() {
  Serial.println();
  Serial.println("========== CURRENT CONFIG ==========");
  Serial.print("SSID   : ");
  Serial.println(WIFI_SSID);
  Serial.print("SERVER : ");
  Serial.println(SERVER_URL);
  Serial.println("PASSWORD: ********");
  Serial.println("====================================");
  Serial.println();
}

// =====================================================
// CHECK FOR CONFIG COMMAND
// =====================================================

bool checkForConfigCommand() {
  if (!Serial.available()) {
    return false;
  }
  String command = Serial.readStringUntil('\n');
  command.trim();

  if (command.equalsIgnoreCase("CONFIG")) {
    serialConfigMode();
    return true;
  }
  return false;
}

// =====================================================
// USB SERIAL CONFIGURATION MODE
// =====================================================

void serialConfigMode() {
  Serial.println();
  Serial.println("====================================");
  Serial.println("      USB CONFIGURATION MODE");
  Serial.println("====================================");
  Serial.println("NodeMCU is now in configuration mode.");
  Serial.println("Normal RFID operation is paused.");
  Serial.println("Commands:");
  Serial.println("SSID=<wifi-name>");
  Serial.println("PASSWORD=<wifi-password>");
  Serial.println("SERVER=<server-url>");
  Serial.println("SAVE");
  Serial.println("SHOW");
  Serial.println("RESET");
  Serial.println("EXIT");

  while (true) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();

      if (line.length() == 0) {
        continue;
      }

      if (line.equalsIgnoreCase("EXIT")) {
        Serial.println("Exiting configuration mode...");
        return;
      }
      
      if (line.equalsIgnoreCase("SHOW")) {
        printCurrentConfig();
        continue;
      }
      
      if (line.equalsIgnoreCase("SAVE")) {
        if (saveConfig()) {
          Serial.println("CONFIGURATION SAVED SUCCESSFULLY");
          Serial.println("Restarting NodeMCU...");
          delay(1000);
          ESP.restart();
        } else {
          Serial.println("ERROR: CONFIGURATION SAVE FAILED");
        }
        continue;
      }
      
      if (line.equalsIgnoreCase("RESET")) {
        LittleFS.remove(CONFIG_FILE);
        Serial.println("Configuration deleted. Restarting...");
        delay(1000);
        ESP.restart();
      }

      int equalsIdx = line.indexOf('=');
      if (equalsIdx > 0) {
        String key = line.substring(0, equalsIdx);
        String value = line.substring(equalsIdx + 1);
        key.trim();
        value.trim();

        if (key.equalsIgnoreCase("SSID")) {
          WIFI_SSID = value;
          Serial.println("SSID updated in memory.");
        }
        else if (key.equalsIgnoreCase("PASSWORD")) {
          WIFI_PASSWORD = value;
          Serial.println("PASSWORD updated in memory.");
        }
        else if (key.equalsIgnoreCase("SERVER")) {
          SERVER_URL = value;
          Serial.println("SERVER updated in memory.");
        }
        else {
          Serial.println("Unknown key.");
        }
      } else {
        Serial.println("Invalid command format.");
      }
    }
    yield();
  }
}

// =====================================================
// CONNECT WI-FI
// =====================================================

bool connectWiFi() {
  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command.equalsIgnoreCase("CONFIG")) {
        Serial.println("\nUSB CONFIG requested.");
        serialConfigMode();
        return false;
      }
    }

    Serial.print(".");
    delay(500);

    if (millis() - startTime > 20000) {
      Serial.println("\nWi-Fi connection timeout.");
      return false;
    }
  }

  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  digitalWrite(LED_BUILTIN, LOW);  
  return true;
}

// =====================================================
// READ NDEF TEXT
// =====================================================

String readNDEFText() {

  // Read Blocks 4, 5 and 6
  // Total = 48 bytes
  byte buffer[48];

  String text = "";

  MFRC522::StatusCode status;

  // ==========================================
  // READ BLOCK 4
  // ==========================================

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    4,
    &key,
    &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    return "";
  }

  byte blockData[18];
  byte size = sizeof(blockData);

  status = rfid.MIFARE_Read(
    4,
    blockData,
    &size
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 4 read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    return "";
  }

  for (int i = 0; i < 16; i++) {
    buffer[i] = blockData[i];
  }


  // ==========================================
  // READ BLOCK 5
  // ==========================================

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    5,
    &key,
    &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 5 authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    return "";
  }

  size = sizeof(blockData);

  status = rfid.MIFARE_Read(
    5,
    blockData,
    &size
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 5 read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    return "";
  }

  for (int i = 0; i < 16; i++) {
    buffer[16 + i] = blockData[i];
  }


  // ==========================================
  // READ BLOCK 6
  // ==========================================

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    6,
    &key,
    &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 6 authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    return "";
  }

  size = sizeof(blockData);

  status = rfid.MIFARE_Read(
    6,
    blockData,
    &size
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 6 read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    return "";
  }

  for (int i = 0; i < 16; i++) {
    buffer[32 + i] = blockData[i];
  }


  // ==========================================
  // FIND NDEF TLV
  // ==========================================

  int index = 0;

  while (index < 48 && buffer[index] == 0x00) {
    index++;
  }

  if (index >= 48 || buffer[index] != 0x03) {

    Serial.println("NDEF TLV not found.");

    return "";
  }

  index++;


  // ==========================================
  // NDEF LENGTH
  // ==========================================

  int ndefLength = buffer[index++];

  if (ndefLength <= 0 ||
      index + ndefLength > 48) {

    Serial.println("Invalid NDEF length.");

    return "";
  }


  // ==========================================
  // NDEF RECORD HEADER
  // ==========================================

  if (buffer[index++] != 0xD1) {

    Serial.println("Invalid NDEF record header.");

    return "";
  }


  // Type length
  byte typeLength = buffer[index++];

  // Payload length
  byte payloadLength = buffer[index++];


  // ==========================================
  // TEXT RECORD
  // ==========================================

  if (typeLength != 1 ||
      buffer[index] != 0x54) {

    Serial.println("NDEF is not a Text record.");

    return "";
  }

  index += typeLength;


  if (payloadLength < 3) {

    Serial.println("NDEF payload too short.");

    return "";
  }


  // ==========================================
  // STATUS BYTE
  // ==========================================

  byte statusByte = buffer[index++];

  byte languageLength = statusByte & 0x3F;

  if (index + languageLength > 48) {

    Serial.println("Invalid language length.");

    return "";
  }

  index += languageLength;


  // ==========================================
  // TEXT
  // ==========================================

  int textLength =
    payloadLength - 1 - languageLength;


  if (textLength <= 0 ||
      index + textLength > 48) {

    Serial.println("Invalid NDEF text length.");

    return "";
  }


  // ==========================================
  // NUMERIC VALIDATION
  // ==========================================

  for (int i = 0; i < textLength; i++) {

    char c = (char)buffer[index + i];

    if (!isDigit(c)) {

      Serial.println(
        "Non-numeric character found in NDEF."
      );

      return "";
    }

    text += c;
  }


  Serial.print("NDEF Number Read: ");
  Serial.println(text);

  return text;
}
// =====================================================
// VERIFY CARD WITH SERVER
// =====================================================

bool verifyCard(String number) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi is not connected. Verification skipped.");
    return false;
  }

  HTTPClient http;
  WiFiClient client;
  String endpoint = SERVER_URL + "/verify";

  Serial.println("\nSending verification request...");
  Serial.print("URL: ");
  Serial.println(endpoint);
  Serial.print("Number: ");
  Serial.println(number);

  if (!http.begin(client, endpoint)) {
    Serial.println("HTTP connection failed.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["number"] = number;
  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);

  if (httpCode <= 0) {
    Serial.print("HTTP request failed: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return false;
  }

  Serial.print("HTTP Status: ");
  Serial.println(httpCode);

  String response = http.getString();
  Serial.print("Server Response: ");
  Serial.println(response);

  http.end();

  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error) {
    Serial.println("Invalid server JSON.");
    return false;
  }

  bool success = responseDoc["success"] | false;
  String access = responseDoc["access"] | "";

  if (success && access == "granted") {
    Serial.println("\n********************************");
    Serial.println("         ACCESS GRANTED");
    Serial.println("********************************\n");
    return true;
  }

  if (success && access == "denied") {
    Serial.println("\n********************************");
    Serial.println("         ACCESS DENIED");
    Serial.println("********************************\n");
    return false;
  }

  Serial.println("Unknown server response.");
  return false;
}

void handleStatus() {
  JsonDocument doc;

  doc["status"] = "online";
  doc["device"] = "NodeMCU";
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime"] = millis() / 1000;

  String response;
  serializeJson(doc, response);

  statusServer.send(200, "application/json", response);
}



// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n====================================");
  Serial.println("      RFID ACCESS CONTROLLER");
  Serial.println("====================================\n");

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed.");
    return;
  }
  Serial.println("LittleFS mounted.");

  setDefaultConfig();
  if (loadSavedConfig()) {
    Serial.println("Using saved configuration.");
  } else {
    Serial.println("Using default configuration.");
    saveConfig();
  }
  printCurrentConfig();

  SPI.begin();
  rfid.PCD_Init();
  delay(100);

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = ndefKey[i];
  }
  Serial.println("RC522 initialized.");


  bool wifiConnected = connectWiFi();
  if (!wifiConnected) {
    Serial.println("\nWi-Fi not connected.");
    Serial.println("You can type CONFIG anytime through USB.\n");
  }
  if (wifiConnected) {
    statusServer.on("/status", HTTP_GET, handleStatus);
    statusServer.begin();

    Serial.println("Wireless status server started.");
    Serial.print("Status URL: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/status");
  }

  Serial.println("\n====================================");
  Serial.println("SYSTEM READY");
  Serial.println("====================================\n");
  Serial.println("USB command available anytime: CONFIG\n");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  statusServer.handleClient();
  if (checkForConfigCommand()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWi-Fi disconnected.");
    if (!connectWiFi()) {
      delay(1000);
      return;
    }
  }

  if (!rfid.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  Serial.println("\n------------------------------------");
  Serial.println("RFID CARD DETECTED");
  Serial.println("------------------------------------");

  String number = readNDEFText();

  if (number.length() == 0) {
    Serial.println("Verification skipped: No valid numeric NDEF found.");
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(1000);
    return;
  }
  
  Serial.print("Parsed NDEF Number: ");
  Serial.println(number);

  verifyCard(number);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  Serial.println("\nCard processing completed.");
  Serial.println("USB CONFIG remains available anytime.\n");

  delay(1500);
}