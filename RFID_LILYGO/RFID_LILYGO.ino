#include <SPI.h>
#include <MFRC522.h>
#include "BluetoothSerial.h"
//
//#define SCK  18
//#define MISO  19
//#define MOSI  23
//#define CS  5
#define SDA_PIN 21
#define RST_PIN 15
#define RELAY 14
String auth = "";
int relay1 = LOW;
//SPIClass spi = SPIClass(VSPI);
BluetoothSerial SerialBT;
MFRC522 mfrc522(SDA_PIN, RST_PIN);
void setup() {
  // put your setup code here, to run once:
  pinMode(RELAY, OUTPUT);
  Serial.begin(115200);
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  Serial.println("Approximate your card to the reader...");
  SerialBT.print("Approximate your card to the reader...");

}

void loop() {
  // put your main code here, to run repeatedly:
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  //Show UID on serial monitor
  Serial.print("UID tag :");
  SerialBT.print("UID Tag : ");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    SerialBT.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    SerialBT.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message : ");
  SerialBT.print("Message");
  content.toUpperCase();
  if ((content.substring(1) == "73 67 24 F6") || (content.substring(1) == "73 67 24 F6")) //change here the UID of the card/cards that you want to give access
  {
    Serial.println("Authorized Access");
    auth = "Authorized Access";
    SerialBT.print("Authorized Access");
    Serial.println();
    relay1 = ~ relay1;
    digitalWrite(RELAY, relay1);
    delay(500);
  }

  else   {
    Serial.println("Access Denied");
    SerialBT.print("Access Denied");
    auth = "Access Denied" ;
    delay(3000);
  }
}
