#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ==================================================
// DEFAULT / FALLBACK CONFIGURATION
// ==================================================
// Used when:
// 1. No saved configuration exists
// 2. Saved WiFi cannot connect
// 3. Saved configuration is invalid

const char* DEFAULT_WIFI_SSID = "MDPHONE";
const char* DEFAULT_WIFI_PASSWORD = "$$md2010";
const char* DEFAULT_SERVER_URL = "http://10.122.236.162:5000";

// ==================================================
// CURRENT CONFIGURATION
// ==================================================

String WIFI_SSID;
String WIFI_PASSWORD;
String SERVER_URL;

const char* CONFIG_FILE = "/config.json";


// ==================================================
// RFID CONFIGURATION
// ==================================================

#define SS_PIN  D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key;

byte ndefKey[6] = {
  0xD3,
  0xF7,
  0xD3,
  0xF7,
  0xD3,
  0xF7
};


// ==================================================
// SET DEFAULT CONFIG
// ==================================================

void setDefaultConfig() {

  WIFI_SSID = DEFAULT_WIFI_SSID;
  WIFI_PASSWORD = DEFAULT_WIFI_PASSWORD;
  SERVER_URL = DEFAULT_SERVER_URL;

  Serial.println();
  Serial.println("Using DEFAULT firmware configuration.");
}


// ==================================================
// LOAD SAVED CONFIGURATION
// ==================================================

bool loadSavedConfig() {

  if (!LittleFS.exists(CONFIG_FILE)) {

    Serial.println(
      "No saved configuration found."
    );

    return false;
  }

  File file = LittleFS.open(
    CONFIG_FILE,
    "r"
  );

  if (!file) {

    Serial.println(
      "Failed to open saved configuration."
    );

    return false;
  }

  StaticJsonDocument<512> doc;

  DeserializationError error =
    deserializeJson(doc, file);

  file.close();

  if (error) {

    Serial.print(
      "Saved config JSON error: "
    );

    Serial.println(
      error.c_str()
    );

    return false;
  }

  if (
    !doc["wifi_ssid"].is<const char*>() ||
    !doc["wifi_password"].is<const char*>() ||
    !doc["server_url"].is<const char*>()
  ) {

    Serial.println(
      "Saved configuration is incomplete."
    );

    return false;
  }

  String savedSSID =
    doc["wifi_ssid"].as<String>();

  String savedPassword =
    doc["wifi_password"].as<String>();

  String savedServer =
    doc["server_url"].as<String>();

  if (
    savedSSID.length() == 0 ||
    savedPassword.length() == 0 ||
    savedServer.length() == 0
  ) {

    Serial.println(
      "Saved configuration is invalid."
    );

    return false;
  }

  WIFI_SSID = savedSSID;
  WIFI_PASSWORD = savedPassword;
  SERVER_URL = savedServer;

  Serial.println();
  Serial.println(
    "Saved configuration loaded."
  );

  Serial.print("Saved SSID: ");
  Serial.println(WIFI_SSID);

  Serial.print("Saved Server: ");
  Serial.println(SERVER_URL);

  return true;
}


// ==================================================
// SAVE CONFIGURATION
// ==================================================

bool saveConfig(
  String newSSID,
  String newPassword,
  String newServerURL
) {

  StaticJsonDocument<512> doc;

  doc["wifi_ssid"] = newSSID;
  doc["wifi_password"] = newPassword;
  doc["server_url"] = newServerURL;

  File file = LittleFS.open(
    CONFIG_FILE,
    "w"
  );

  if (!file) {

    Serial.println(
      "Failed to open configuration file."
    );

    return false;
  }

  size_t written =
    serializeJson(doc, file);

  file.close();

  if (written == 0) {

    Serial.println(
      "Failed to write configuration."
    );

    return false;
  }

  Serial.println(
    "New configuration saved to LittleFS."
  );

  return true;
}


// ==================================================
// TRY WIFI CONNECTION
// ==================================================

bool tryWiFi(
  String ssid,
  String password
) {

  Serial.println();
  Serial.println("==============================");
  Serial.println("       WIFI CONNECTION");
  Serial.println("==============================");

  Serial.print("SSID: ");
  Serial.println(ssid);

  WiFi.disconnect();

  delay(300);

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    ssid.c_str(),
    password.c_str()
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 20
  ) {

    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    Serial.println(
      "WiFi connected!"
    );

    Serial.print(
      "IP Address: "
    );

    Serial.println(
      WiFi.localIP()
    );

    return true;
  }

  Serial.println(
    "WiFi connection failed."
  );

  WiFi.disconnect();

  return false;
}


// ==================================================
// CONNECT USING SAVED CONFIG,
// THEN FALLBACK TO DEFAULT
// ==================================================

bool connectWiFi() {

  // ------------------------------------------------
  // First: Try currently loaded configuration
  // ------------------------------------------------

  Serial.println();
  Serial.println(
    "Trying current configuration..."
  );

  if (
    tryWiFi(
      WIFI_SSID,
      WIFI_PASSWORD
    )
  ) {

    return true;
  }

  // ------------------------------------------------
  // If current config failed, try defaults
  // ------------------------------------------------

  bool alreadyDefault =
    WIFI_SSID == DEFAULT_WIFI_SSID &&
    WIFI_PASSWORD == DEFAULT_WIFI_PASSWORD;

  if (alreadyDefault) {

    Serial.println(
      "Current configuration is already default."
    );

    return false;
  }

  Serial.println();
  Serial.println(
    "Saved WiFi failed."
  );

  Serial.println(
    "Trying DEFAULT WiFi..."
  );

  if (
    tryWiFi(
      DEFAULT_WIFI_SSID,
      DEFAULT_WIFI_PASSWORD
    )
  ) {

    // Use default values as current config
    WIFI_SSID = DEFAULT_WIFI_SSID;
    WIFI_PASSWORD = DEFAULT_WIFI_PASSWORD;
    SERVER_URL = DEFAULT_SERVER_URL;

    Serial.println(
      "Fallback to DEFAULT configuration successful."
    );

    return true;
  }

  Serial.println();
  Serial.println(
    "DEFAULT WiFi also failed."
  );

  return false;
}


// ==================================================
// FETCH LATEST CONFIG FROM FLASK
// ==================================================

bool fetchRemoteConfig() {

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "Cannot fetch config: WiFi not connected."
    );

    return false;
  }

  String configURL =
    SERVER_URL + "/device/config";

  Serial.println();
  Serial.println("==============================");
  Serial.println("    FETCHING LATEST CONFIG");
  Serial.println("==============================");

  Serial.print(
    "URL: "
  );

  Serial.println(
    configURL
  );

  WiFiClient client;

  HTTPClient http;

  http.setTimeout(5000);

  if (
    !http.begin(
      client,
      configURL
    )
  ) {

    Serial.println(
      "Could not initialize HTTP."
    );

    return false;
  }

  http.addHeader(
    "Accept",
    "application/json"
  );

  int httpCode =
    http.GET();

  if (httpCode <= 0) {

    Serial.print(
      "Config request failed: "
    );

    Serial.println(
      http.errorToString(
        httpCode
      )
    );

    http.end();

    return false;
  }

  Serial.print(
    "HTTP Status: "
  );

  Serial.println(
    httpCode
  );

  if (
    httpCode != HTTP_CODE_OK
  ) {

    Serial.println(
      "Config endpoint returned an error."
    );

    http.end();

    return false;
  }

  String response =
    http.getString();

  http.end();

  Serial.println(
    "Config received."
  );

  StaticJsonDocument<768> doc;

  DeserializationError error =
    deserializeJson(
      doc,
      response
    );

  if (error) {

    Serial.print(
      "Invalid config JSON: "
    );

    Serial.println(
      error.c_str()
    );

    return false;
  }

  if (
    !doc["success"].is<bool>() ||
    !doc["success"].as<bool>()
  ) {

    Serial.println(
      "Server returned unsuccessful configuration."
    );

    return false;
  }

  if (
    !doc["wifi_ssid"].is<const char*>() ||
    !doc["wifi_password"].is<const char*>() ||
    !doc["server_url"].is<const char*>()
  ) {

    Serial.println(
      "Required configuration fields missing."
    );

    return false;
  }

  String newSSID =
    doc["wifi_ssid"].as<String>();

  String newPassword =
    doc["wifi_password"].as<String>();

  String newServerURL =
    doc["server_url"].as<String>();

  if (
    newSSID.length() == 0 ||
    newPassword.length() == 0 ||
    newServerURL.length() == 0
  ) {

    Serial.println(
      "Received configuration is invalid."
    );

    return false;
  }

  // Remove trailing slash
  if (
    newServerURL.endsWith("/")
  ) {

    newServerURL.remove(
      newServerURL.length() - 1
    );
  }

  // ------------------------------------------------
  // Check whether configuration changed
  // ------------------------------------------------

  bool changed =
    newSSID != WIFI_SSID ||
    newPassword != WIFI_PASSWORD ||
    newServerURL != SERVER_URL;

  if (!changed) {

    Serial.println(
      "Configuration is already up to date."
    );

    return false;
  }

  Serial.println();
  Serial.println(
    "NEW CONFIGURATION RECEIVED"
  );

  Serial.print(
    "New SSID: "
  );

  Serial.println(
    newSSID
  );

  Serial.print(
    "New Server: "
  );

  Serial.println(
    newServerURL
  );

  // ------------------------------------------------
  // Save new configuration
  // ------------------------------------------------

  if (
    !saveConfig(
      newSSID,
      newPassword,
      newServerURL
    )
  ) {

    Serial.println(
      "Failed to save new configuration."
    );

    return false;
  }

  Serial.println();
  Serial.println(
    "Configuration updated successfully."
  );

  Serial.println(
    "Restarting NodeMCU..."
  );

  delay(1500);

  ESP.restart();

  return true;
}


// ==================================================
// READ NDEF TEXT
// ==================================================

String readNDEFText() {

  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    4,
    &key,
    &(rfid.uid)
  );

  if (
    status != MFRC522::STATUS_OK
  ) {

    Serial.print(
      "Authentication failed: "
    );

    Serial.println(
      rfid.GetStatusCodeName(
        status
      )
    );

    return "";
  }

  byte data[48];

  byte buffer[18];

  // Read blocks 4, 5, 6
  for (
    byte block = 4;
    block <= 6;
    block++
  ) {

    byte size =
      sizeof(buffer);

    status = rfid.MIFARE_Read(
      block,
      buffer,
      &size
    );

    if (
      status != MFRC522::STATUS_OK
    ) {

      Serial.print(
        "Block "
      );

      Serial.print(
        block
      );

      Serial.print(
        " read failed: "
      );

      Serial.println(
        rfid.GetStatusCodeName(
          status
        )
      );

      return "";
    }

    for (
      byte i = 0;
      i < 16;
      i++
    ) {

      data[
        (block - 4) * 16 + i
      ] = buffer[i];
    }
  }

  // ------------------------------------------------
  // Parse NDEF Text Record
  // ------------------------------------------------

  for (
    byte i = 0;
    i < 46;
    i++
  ) {

    if (
      data[i] != 0x03
    )
      continue;

    byte ndefLength =
      data[i + 1];

    if (
      ndefLength == 0
    )
      continue;

    if (
      i + 2 + ndefLength > 48
    )
      continue;

    byte start =
      i + 2;

    // NDEF Text record
    if (
      data[start] != 0xD1
    )
      continue;

    byte typeLength =
      data[start + 1];

    byte payloadLength =
      data[start + 2];

    if (
      typeLength != 1
    )
      continue;

    // "T"
    if (
      data[start + 3] != 0x54
    )
      continue;

    byte payloadStart =
      start + 4;

    if (
      payloadLength < 1
    )
      continue;

    byte statusByte =
      data[payloadStart];

    byte languageLength =
      statusByte & 0x3F;

    if (
      languageLength + 1 >
      payloadLength
    )
      continue;

    byte textStart =
      payloadStart +
      1 +
      languageLength;

    byte textLength =
      payloadLength -
      1 -
      languageLength;

    String text = "";

    for (
      byte j = 0;
      j < textLength;
      j++
    ) {

      text += (char)data[
        textStart + j
      ];
    }

    return text;
  }

  return "";
}


// ==================================================
// VERIFY CARD
// ==================================================

void verifyCard(
  String number
) {

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "WiFi is not connected."
    );

    return;
  }

  WiFiClient client;

  HTTPClient http;

  String verifyURL =
    SERVER_URL + "/verify";

  Serial.println();
  Serial.println("==============================");
  Serial.println("       VERIFYING CARD");
  Serial.println("==============================");

  Serial.print(
    "Card Number: "
  );

  Serial.println(
    number
  );

  if (
    !http.begin(
      client,
      verifyURL
    )
  ) {

    Serial.println(
      "HTTP connection failed."
    );

    return;
  }

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  String json =
    "{\"number\":\"" +
    number +
    "\"}";

  Serial.print(
    "Sending: "
  );

  Serial.println(
    json
  );

  int httpCode =
    http.POST(
      json
    );

  if (
    httpCode > 0
  ) {

    Serial.print(
      "HTTP Status: "
    );

    Serial.println(
      httpCode
    );

    String response =
      http.getString();

    Serial.print(
      "Server Response: "
    );

    Serial.println(
      response
    );

    if (
      response.indexOf(
        "\"access\":\"granted\""
      ) >= 0
    ) {

      Serial.println();
      Serial.println(
        "=============================="
      );

      Serial.println(
        "       ACCESS: GRANTED"
      );

      Serial.println(
        "=============================="
      );

    }
    else if (
      response.indexOf(
        "\"access\":\"denied\""
      ) >= 0
    ) {

      Serial.println();
      Serial.println(
        "=============================="
      );

      Serial.println(
        "       ACCESS: DENIED"
      );

      Serial.println(
        "=============================="
      );

    }
    else {

      Serial.println(
        "UNKNOWN SERVER RESPONSE"
      );
    }

  }
  else {

    Serial.print(
      "HTTP POST failed: "
    );

    Serial.println(
      http.errorToString(
        httpCode
      )
    );
  }

  http.end();
}


// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("       NFC ACCESS SYSTEM");
  Serial.println("==============================");


  // ==================================================
  // LITTLEFS
  // ==================================================

  bool filesystemReady =
    LittleFS.begin();

  if (
    filesystemReady
  ) {

    Serial.println(
      "LittleFS initialized."
    );

    // Try saved configuration first.
    // If unavailable, use defaults.

    if (
      !loadSavedConfig()
    ) {

      setDefaultConfig();
    }

  }
  else {

    Serial.println(
      "LittleFS initialization failed."
    );

    // Continue using firmware defaults.
    setDefaultConfig();
  }


  // ==================================================
  // RFID
  // ==================================================

  SPI.begin();

  rfid.PCD_Init();

  for (
    byte i = 0;
    i < 6;
    i++
  ) {

    key.keyByte[i] =
      ndefKey[i];
  }

  Serial.println(
    "RFID reader initialized."
  );


  // ==================================================
  // WIFI
  // ==================================================

  bool connected =
    connectWiFi();


  // ==================================================
  // FETCH LATEST CONFIG
  // ==================================================

  if (
    connected
  ) {

    // If this fails:
    // keep current configuration
    // and continue RFID operation.

    fetchRemoteConfig();
  }


  // ==================================================
  // READY
  // ==================================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("         SYSTEM READY");
  Serial.println("==============================");

  Serial.println(
    "Waiting for card..."
  );
}


// ==================================================
// LOOP
// ==================================================

void loop() {

  if (
    !rfid.PICC_IsNewCardPresent()
  )
    return;

  if (
    !rfid.PICC_ReadCardSerial()
  )
    return;


  Serial.println();
  Serial.println("==============================");
  Serial.println("       CARD DETECTED");
  Serial.println("==============================");


  String number =
    readNDEFText();


  if (
    number.length() > 0
  ) {

    Serial.println(
      "CARD NUMBER:"
    );

    Serial.println(
      number
    );

    verifyCard(
      number
    );

  }
  else {

    Serial.println(
      "No NDEF text found."
    );
  }


  rfid.PICC_HaltA();

  rfid.PCD_StopCrypto1();


  // Prevent duplicate scans
  delay(1000);


  Serial.println();

  Serial.println(
    "Waiting for card..."
  );
}