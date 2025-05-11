/*
 *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 * More pin layouts for other boards can be found here: https://github.com/miguelbalboa/rfid#pin-layout
 */
#include <Arduino.h>

#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include ".env/WiFiCredentials.h"
#include "lib/RFIDUtils.h"
#include "LittleFS.h"

#define RST_PIN D3 // Configurable, see typical pin layout above
#define SS_PIN D8  // Configurable, see typical pin layout above
#define RELAY_DOOR_PIN D2

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

bool debugger = false;
RFIDUtils rfidUtils;

void setupWifi()
{
  WiFi.begin(SOFT_AP_SSID, SOFT_AP_PASSWORD);

  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println(".");
  }

  Serial.println("Connected to the WiFi network");
}

void turnOnDoorRelay()
{
  digitalWrite(RELAY_DOOR_PIN, HIGH);
  delay(1500);
}

void turnOffDoorRelay()
{
  digitalWrite(RELAY_DOOR_PIN, LOW);
}

void displayContentFromArray(String *arr, int len)
{
  Serial.println("Array content is:");
  for (int i = 0; i < len; i++)
  {
    Serial.println("Array[" + String(i) + "] = " + String(arr[i]));
  }
}

void setup()
{
  Serial.begin(9600); // Initialize serial communications with the PC

  pinMode(RELAY_DOOR_PIN, OUTPUT);
  turnOffDoorRelay();

  while (!Serial)
    ;

  // setupWifi(); // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  rfidUtils._enableDebugger(debugger);
  rfidUtils.updateCardsIdListOnSetup();

  SPI.begin();                       // Init SPI bus
  mfrc522.PCD_Init();                // Init MFRC522
  delay(4);                          // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

void loop()
{
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }

  String cardId;
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    cardId += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardId.trim();

  if (debugger)
  {
    Serial.print("Debugger => loop() - after for::mfrc522.uid. Cards id: ");
    Serial.println(cardId);
  }

  Serial.println();
  Serial.println("-----------------------------------------------------------------------");

  if (rfidUtils.isCardIdAllowed(cardId))
  {
    turnOnDoorRelay();
  }
  else
  {
    String messageTo = "Card id " + cardId + " is not allowed!";
    rfidUtils.sendMessageToServer(messageTo);
  }

  turnOffDoorRelay();

  rfidUtils.updateCardsIdListOnTime();

  delay(1000);

  Serial.println("Ready to touch the Card!");
}