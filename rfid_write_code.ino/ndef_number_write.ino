#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// ========================================
// ONLY CHANGE THIS
// ========================================

String CARD_NUMBER = "07984253060";

// ========================================

byte keyA[6] = {
  0xD3, 0xF7, 0xD3,
  0xF7, 0xD3, 0xF7
};


void setup() {

  Serial.begin(115200);

  SPI.begin();
  rfid.PCD_Init();

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = keyA[i];
  }

  Serial.println();
  Serial.println("RFID NDEF WRITER");
  Serial.print("Number: ");
  Serial.println(CARD_NUMBER);
  Serial.println("Place card...");
}


void loop() {

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println();
  Serial.println("Card detected!");


  // ========================================
  // BUILD COMPLETE NDEF MESSAGE
  // ========================================

  byte ndef[32];

  int ndefLength = 0;

  // NDEF Header
  ndef[ndefLength++] = 0xD1;

  // Type Length
  ndef[ndefLength++] = 0x01;

  // Payload Length
  // 1 status + 2 language + number
  byte payloadLength = 3 + CARD_NUMBER.length();

  ndef[ndefLength++] = payloadLength;

  // Type = T
  ndef[ndefLength++] = 0x54;

  // Status byte
  ndef[ndefLength++] = 0x02;

  // Language = "en"
  ndef[ndefLength++] = 0x65;
  ndef[ndefLength++] = 0x6E;

  // Number
  for (int i = 0; i < CARD_NUMBER.length(); i++) {
    ndef[ndefLength++] = CARD_NUMBER[i];
  }


  // ========================================
  // COMPLETE TLV
  // ========================================

  byte data[40];

  int dataLength = 0;

  // NDEF TLV
  data[dataLength++] = 0x03;

  // NDEF length
  data[dataLength++] = ndefLength;

  // NDEF message
  for (int i = 0; i < ndefLength; i++) {
    data[dataLength++] = ndef[i];
  }

  // Terminator
  data[dataLength++] = 0xFE;


  // ========================================
  // PREPARE BLOCKS
  // ========================================

  byte block4[16] = {0};
  byte block5[16] = {0};
  byte block6[16] = {0};

  for (int i = 0; i < 16 && i < dataLength; i++) {
    block4[i] = data[i];
  }

  for (int i = 16; i < 32 && i < dataLength; i++) {
    block5[i - 16] = data[i];
  }

  for (int i = 32; i < 48 && i < dataLength; i++) {
    block6[i - 32] = data[i];
  }


  // ========================================
  // WRITE BLOCK 4
  // ========================================

  if (!writeBlock(4, block4)) {
    finishCard();
    delay(2000);
    return;
  }


  // ========================================
  // WRITE BLOCK 5
  // ========================================

  if (!writeBlock(5, block5)) {
    finishCard();
    delay(2000);
    return;
  }


  // ========================================
  // WRITE BLOCK 6
  // ========================================

  if (!writeBlock(6, block6)) {
    finishCard();
    delay(2000);
    return;
  }


  Serial.println();
  Serial.println("==============================");
  Serial.println("WRITE SUCCESSFUL");
  Serial.println("==============================");
  Serial.print("NDEF Number: ");
  Serial.println(CARD_NUMBER);
  Serial.println();


  finishCard();

  delay(3000);
}


// ========================================
// WRITE BLOCK
// ========================================

bool writeBlock(byte blockNumber, byte *data) {

  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    blockNumber,
    &key,
    &rfid.uid
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Authentication failed on block ");
    Serial.print(blockNumber);
    Serial.print(": ");

    Serial.println(
      rfid.GetStatusCodeName(status)
    );

    return false;
  }


  status = rfid.MIFARE_Write(
    blockNumber,
    data,
    16
  );

  if (status != MFRC522::STATUS_OK) {

    Serial.print("Write failed on block ");
    Serial.print(blockNumber);
    Serial.print(": ");

    Serial.println(
      rfid.GetStatusCodeName(status)
    );

    return false;
  }


  Serial.print("Block ");
  Serial.print(blockNumber);
  Serial.println(" written.");

  return true;
}


// ========================================
// FINISH CARD
// ========================================

void finishCard() {

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}