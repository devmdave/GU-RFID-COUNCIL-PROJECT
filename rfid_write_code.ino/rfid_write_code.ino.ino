#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

byte block = 4;

void setup() {
  Serial.begin(115200);
  delay(1000);

  SPI.begin();
  rfid.PCD_Init();

  // Default MIFARE Classic key
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println();
  Serial.println("==============================");
  Serial.println("    MIFARE CLASSIC READER");
  Serial.println("==============================");
  Serial.println("Place card on RC522...");
}

void loop() {

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println();
  Serial.println("CARD DETECTED!");

  Serial.print("UID: ");

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10)
      Serial.print("0");

    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }

  Serial.println();

  // Authenticate Block 4
  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    block,
    &key,
    &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    delay(2000);
    return;
  }

  Serial.println("Authentication successful!");

  // Read block
  byte buffer[18];
  byte size = sizeof(buffer);

  status = rfid.MIFARE_Read(
    block,
    buffer,
    &size
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));

  } else {

    Serial.println("==============================");
    Serial.println("        BLOCK 4 DATA");
    Serial.println("==============================");

    Serial.print("HEX: ");

    for (byte i = 0; i < 16; i++) {
      if (buffer[i] < 0x10)
        Serial.print("0");

      Serial.print(buffer[i], HEX);
      Serial.print(" ");
    }

    Serial.println();

    Serial.print("TEXT: ");

    for (byte i = 0; i < 16; i++) {

      if (buffer[i] >= 32 && buffer[i] <= 126)
        Serial.print((char)buffer[i]);
      else
        Serial.print(".");
    }

    Serial.println();

    Serial.println("==============================");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  Serial.println("Remove card...");
  delay(3000);
}