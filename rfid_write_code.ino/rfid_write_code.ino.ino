#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D2
#define RST_PIN D1
#define LED_PIN D4

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// NFC Forum NDEF Key A
byte ndefKey[6] = {
  0xD3, 0xF7, 0xD3,
  0xF7, 0xD3, 0xF7
};

// ==========================================
// YOUR 10-DIGIT ID
// ==========================================

String textToWrite = "1234567890";

// ==========================================

void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  SPI.begin();
  rfid.PCD_Init();

  // Load NDEF key
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = ndefKey[i];
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("       NFC NDEF WRITER");
  Serial.println("================================");

  Serial.print("Text to write: ");
  Serial.println(textToWrite);

  Serial.println();
  Serial.println("Place card on RC522...");
}


// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println();
  Serial.println("CARD DETECTED!");

  // Print UID
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

  // Write NDEF
  bool success = writeNDEFText(textToWrite);

  if (success) {

    Serial.println();
    Serial.println("================================");
    Serial.println("       WRITE SUCCESS!");
    Serial.println("================================");

    Serial.print("NDEF TEXT: ");
    Serial.println(textToWrite);

    Serial.println();
    Serial.println("Remove card.");

    // LED ON
    digitalWrite(LED_PIN, LOW);
    delay(1500);
    digitalWrite(LED_PIN, HIGH);

  } else {

    Serial.println();
    Serial.println("================================");
    Serial.println("        WRITE FAILED");
    Serial.println("================================");

    // Blink LED 3 times
    for (byte i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, LOW);
      delay(150);
      digitalWrite(LED_PIN, HIGH);
      delay(150);
    }
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  // Don't immediately rewrite same card
  delay(3000);

  Serial.println();
  Serial.println("Ready for card...");
}


// ==========================================
// WRITE NDEF TEXT
// ==========================================

bool writeNDEFText(String text) {

  byte data[48];

  // Clear all blocks
  for (byte i = 0; i < 48; i++) {
    data[i] = 0x00;
  }

  byte textLength = text.length();

  // NDEF Text Record length
  byte ndefLength =
    1 + 1 + 1 + 1 + 3 + textLength;

  byte index = 0;

  // ----------------------------------------
  // NDEF TLV
  // ----------------------------------------

  data[index++] = 0x03;
  data[index++] = ndefLength;

  // ----------------------------------------
  // NDEF Record Header
  // ----------------------------------------

  data[index++] = 0xD1;

  // Type Length
  data[index++] = 0x01;

  // Payload Length
  data[index++] = 3 + textLength;

  // Type = "T"
  data[index++] = 0x54;

  // UTF-8 + language length = 2
  data[index++] = 0x02;

  // Language = "en"
  data[index++] = 'e';
  data[index++] = 'n';

  // ----------------------------------------
  // Actual text
  // ----------------------------------------

  for (byte i = 0; i < textLength; i++) {
    data[index++] = text[i];
  }

  // NDEF terminator
  data[index++] = 0xFE;

  // ----------------------------------------
  // Authenticate Block 4
  // ----------------------------------------

  Serial.println("Authenticating NDEF sector...");

  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    4,
    &key,
    &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Authentication failed: ");
    Serial.println(
      rfid.GetStatusCodeName(status)
    );

    return false;
  }

  Serial.println("Authentication SUCCESS");

  // ----------------------------------------
  // Write Block 4
  // ----------------------------------------

  status = rfid.MIFARE_Write(
    4,
    data,
    16
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 4 failed: ");
    Serial.println(
      rfid.GetStatusCodeName(status)
    );

    return false;
  }

  Serial.println("Block 4 written.");

  // ----------------------------------------
  // Write Block 5
  // ----------------------------------------

  status = rfid.MIFARE_Write(
    5,
    data + 16,
    16
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 5 failed: ");
    Serial.println(
      rfid.GetStatusCodeName(status)
    );

    return false;
  }

  Serial.println("Block 5 written.");

  // ----------------------------------------
  // Write Block 6
  // ----------------------------------------

  status = rfid.MIFARE_Write(
    6,
    data + 32,
    16
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Block 6 failed: ");
    Serial.println(
      rfid.GetStatusCodeName(status)
    );

    return false;
  }

  Serial.println("Block 6 written.");

  return true;
}