#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D2
#define RST_PIN D1
#define LED_PIN D4

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// NFC Forum NDEF key
byte ndefKey[6] = {
  0xD3, 0xF7, 0xD3,
  0xF7, 0xD3, 0xF7
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  SPI.begin();
  rfid.PCD_Init();

  // Load NDEF Key A
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = ndefKey[i];
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("       NFC NDEF READER");
  Serial.println("================================");
  Serial.println("RC522 Ready");
  Serial.println("Waiting for card...");
}


void loop() {

  // Check for card
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("CARD DETECTED");

  // -----------------------------
  // Print UID
  // -----------------------------

  Serial.print("UID: ");

  for (byte i = 0; i < rfid.uid.size; i++) {

    if (rfid.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      Serial.print(":");
    }
  }

  Serial.println();

  // -----------------------------
  // Authenticate Sector 1
  // Block 4 belongs to Sector 1
  // -----------------------------

  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    4,
    &key,
    &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    stopCard();
    delay(1000);

    return;
  }

  Serial.println("Authentication: SUCCESS");

  // -----------------------------
  // Read Blocks 4, 5 and 6
  // -----------------------------

  byte ndefData[48];
  byte buffer[18];

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

      Serial.println(
        rfid.GetStatusCodeName(status)
      );

      stopCard();
      delay(1000);

      return;
    }

    // Store block data
    for (byte i = 0; i < 16; i++) {

      ndefData[
        (block - 4) * 16 + i
      ] = buffer[i];
    }
  }

  // -----------------------------
  // Extract NDEF Text
  // -----------------------------

  String text = extractNDEFText(
    ndefData,
    48
  );

  Serial.println();

  if (text.length() > 0) {

    Serial.println("================================");
    Serial.println("          NDEF DATA");
    Serial.println("================================");

    Serial.print("TEXT: ");
    Serial.println(text);

    Serial.println("================================");

    // LED ON
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);

  } else {

    Serial.println("No NDEF text record found.");
  }

  // Stop communication
  stopCard();

  Serial.println();
  Serial.println("Waiting for next card...");

  delay(1000);
}


// =====================================================
// Extract Text from NDEF record
// =====================================================

String extractNDEFText(
  byte *data,
  byte length
) {

  // Search for NDEF TLV
  for (byte i = 0; i < length - 2; i++) {

    // NDEF Message TLV
    if (data[i] != 0x03) {
      continue;
    }

    byte ndefLength = data[i + 1];

    if (ndefLength == 0) {
      continue;
    }

    if ((i + 2 + ndefLength) > length) {
      continue;
    }

    byte start = i + 2;

    // NDEF header
    byte header = data[start];

    // D1 = Short Record + MB + ME + Well Known Type
    if (header != 0xD1) {
      continue;
    }

    byte typeLength = data[start + 1];
    byte payloadLength = data[start + 2];

    // Text record type length must be 1
    if (typeLength != 1) {
      continue;
    }

    // 'T' = Text
    if (data[start + 3] != 0x54) {
      continue;
    }

    // Payload starts after:
    //
    // Header
    // Type Length
    // Payload Length
    // Type
    //
    byte payloadStart = start + 4;

    if (payloadLength < 1) {
      continue;
    }

    // Status byte
    byte statusByte = data[payloadStart];

    // Language code length
    byte languageLength =
      statusByte & 0x3F;

    if (
      languageLength + 1 >
      payloadLength
    ) {
      continue;
    }

    // Actual text starts here
    byte textStart =
      payloadStart + 1 + languageLength;

    byte textLength =
      payloadLength -
      1 -
      languageLength;

    String result = "";

    for (byte j = 0; j < textLength; j++) {

      result += (char)data[
        textStart + j
      ];
    }

    return result;
  }

  return "";
}


// =====================================================
// Stop RFID communication
// =====================================================

void stopCard() {

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}