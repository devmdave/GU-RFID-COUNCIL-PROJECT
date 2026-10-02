#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// =====================================================
// Wi-Fi SETTINGS
// =====================================================

const char* WIFI_SSID = "MDPHONE";
const char* WIFI_PASSWORD = "$$md2010";

// IMPORTANT:
// Replace this with your LAPTOP'S IPv4 address.
// Example:
// http://192.168.43.125:5000/verify
const char* SERVER_URL = "http://10.122.236.162:5000/verify";

// =====================================================
// RC522 PINS
// =====================================================

#define SS_PIN  D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// NFC Forum NDEF Key A
byte ndefKey[6] = {
  0xD3, 0xF7, 0xD3,
  0xF7, 0xD3, 0xF7
};

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);
  delay(500);

  SPI.begin();
  rfid.PCD_Init();

  // Load NDEF key
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = ndefKey[i];
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("     RFID ACCESS SYSTEM");
  Serial.println("================================");

  // Connect to Wi-Fi
  connectWiFi();

  Serial.println();
  Serial.println("RFID Reader Ready.");
  Serial.println("Waiting for card...");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // No new card
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Cannot read card
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("CARD DETECTED");

  // Read NDEF text
  String number = readNDEFText();

  if (number.length() == 0) {

    Serial.println("ERROR: No NDEF text found.");

    stopCard();

    delay(1000);
    Serial.println("Waiting for card...");
    return;
  }

  Serial.print("CARD NUMBER: ");
  Serial.println(number);

  // Make sure Wi-Fi is still connected
  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Wi-Fi disconnected.");
    Serial.println("Reconnecting...");

    connectWiFi();
  }

  // Verify with Flask
  if (WiFi.status() == WL_CONNECTED) {

    verifyCard(number);

  } else {

    Serial.println("ACCESS: UNKNOWN");
    Serial.println("Reason: Wi-Fi unavailable.");
  }

  Serial.println("================================");

  // Stop communication with current card
  stopCard();

  // Prevent repeated scans of the same card
  delay(1500);

  Serial.println("Waiting for card...");
}

// =====================================================
// CONNECT TO WI-FI
// =====================================================

void connectWiFi() {

  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {

    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi connected!");

    Serial.print("NodeMCU IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println("Wi-Fi connection FAILED.");
    Serial.println("Check hotspot name/password.");
  }
}

// =====================================================
// READ NDEF TEXT
// =====================================================

String readNDEFText() {

  MFRC522::StatusCode status;

  // Authenticate Sector 1 / Block 4
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

  byte data[48];
  byte buffer[18];

  // Read Blocks 4, 5 and 6
  for (byte block = 4; block <= 6; block++) {

    byte size = sizeof(buffer);

    status = rfid.MIFARE_Read(
      block,
      buffer,
      &size
    );

    if (status != MFRC522::STATUS_OK) {

      Serial.print("Block ");
      Serial.print(block);
      Serial.print(" read failed: ");
      Serial.println(rfid.GetStatusCodeName(status));

      return "";
    }

    for (byte i = 0; i < 16; i++) {

      data[(block - 4) * 16 + i] = buffer[i];
    }
  }

  // Find NDEF TLV
  for (byte i = 0; i < 46; i++) {

    if (data[i] != 0x03) {
      continue;
    }

    byte ndefLength = data[i + 1];

    if (ndefLength == 0) {
      continue;
    }

    if (i + 2 + ndefLength > 48) {
      continue;
    }

    byte start = i + 2;

    // NDEF Text Record
    if (data[start] != 0xD1) {
      continue;
    }

    byte typeLength = data[start + 1];
    byte payloadLength = data[start + 2];

    if (typeLength != 1) {
      continue;
    }

    // Type must be "T"
    if (data[start + 3] != 0x54) {
      continue;
    }

    byte payloadStart = start + 4;

    if (payloadLength < 1) {
      continue;
    }

    byte statusByte = data[payloadStart];

    byte languageLength = statusByte & 0x3F;

    if (languageLength + 1 > payloadLength) {
      continue;
    }

    byte textStart =
      payloadStart + 1 + languageLength;

    byte textLength =
      payloadLength - 1 - languageLength;

    String text = "";

    for (byte j = 0; j < textLength; j++) {

      text += (char)data[textStart + j];
    }

    return text;
  }

  return "";
}

// =====================================================
// SEND NUMBER TO FLASK
// =====================================================

void verifyCard(String number) {

  Serial.println();
  Serial.println("Sending number to backend...");

  WiFiClient client;
  HTTPClient http;

  // Connect to Flask
  if (!http.begin(client, SERVER_URL)) {

    Serial.println("ERROR: Could not initialize HTTP connection.");

    return;
  }

  // Tell Flask we are sending JSON
  http.addHeader(
    "Content-Type",
    "application/json"
  );

  // Create JSON
  String json = "{\"number\":\"" + number + "\"}";

  Serial.print("POST: ");
  Serial.println(SERVER_URL);

  Serial.print("JSON: ");
  Serial.println(json);

  // Send POST request
  int httpCode = http.POST(json);

  Serial.print("HTTP Status: ");
  Serial.println(httpCode);

  if (httpCode > 0) {

    String response = http.getString();

    Serial.print("Backend Response: ");
    Serial.println(response);

    // Simple response parsing
    if (response.indexOf("\"access\":\"granted\"") >= 0) {

      Serial.println();
      Serial.println("ACCESS: GRANTED");

    }
    else if (response.indexOf("\"access\":\"denied\"") >= 0) {

      Serial.println();
      Serial.println("ACCESS: DENIED");

    }
    else {

      Serial.println();
      Serial.println("ACCESS: UNKNOWN");
      Serial.println("Unexpected backend response.");
    }

  } else {

    Serial.print("HTTP request failed: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// =====================================================
// STOP CARD
// =====================================================

void stopCard() {

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}