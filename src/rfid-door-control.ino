#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include "../EnvVariables.h"
#include "../lib/Debugger/Debugger.h"
#include "../lib/LedIndicator/LedIndicator.h"
#include "../lib/PinoutsBoards/ArduinoNanoPinouts.h"  // You can choose pinouts definition for your board

MFRC522 mfrc522(SS_PIN, RST_PIN);
// Preferences preferences; // To save cards to FLASH memory
LedIndicator ledIndicator(LED_GREEN, LED_RED);

uint8_t cards_size = 0;
const uint8_t maxCards = 16;
Debugger debugger(true);
bool clearAllCardsInMemory = false;

struct CardsInfoObject{
  String cardID;
  bool isMaster = false;
};

CardsInfoObject cards[maxCards];
byte eeCardsAddress = 0;

void setup() {
  debugger.init();
  debugger.logToSerial("Serial iniciando");

  // populateCardsArrayFromEEPROM();

  debugger.logToSerial("Cards count = " + String(cards_size));

  saveMasterCardToMemory();

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(RELAY_DOOR_PIN, OUTPUT);

  keepDoorClosed();

  rfidInit();
}

void populateCardsArrayFromEEPROM() {
  // EEPROM.get(cardsCountAddress, cardsCountObj);
  EEPROM.get(eeCardsAddress, cards);
  // for(CardsInfoObject card : cards) {
    
  // }
}

void updateEEPROM(CardsInfoObject cardsObj[]) {
  // EEPROM.write(eeCardsAddress, 0); // clear all data
  // clearEEPROM();
  EEPROM.put(eeCardsAddress, cardsObj);
}

// void clearEEPROM() {
//   for (int i = 0 ; i < EEPROM.length() ; i++) {
//     EEPROM.write(i, 0);
//   }
// }

void saveMasterCardToMemory() {
  bool masterCardAlreadyExists = false;

  for(CardsInfoObject card : cards) {
    if(card.cardID == MASTER_CARD_ID) { 
      masterCardAlreadyExists = true;
      break;
    }
  }

  if(!masterCardAlreadyExists) { // save master card only if it not exists previously
    CardsInfoObject masterCard;
    masterCard.cardID = MASTER_CARD_ID;
    masterCard.isMaster = true;

    bool isCardSaved = saveCard(masterCard);

    logMasterCardSave(isCardSaved);
  }
}

bool saveCard(CardsInfoObject card) {
  debugger.logToSerial("Saving card...");

  if(cards_size <= maxCards) {
    cards[cards_size++] = card;
    // updateEEPROM(cards);
    debugger.logToSerial("Card saved!");
    
    return true;
  }

  debugger.logToSerial("Card not saved!");
  return false;
}

void removeCardFromArray(int index) {
  for(size_t i = index; i < cards_size; i++) {
    cards[i] = cards[i+1];
  }

  cards_size--;
  
  // updateEEPROM(cards);
}

void logCards() {
  for(CardsInfoObject card : cards) {
    if(!card.cardID.equalsIgnoreCase("")) {
      debugger.logToSerial(card.cardID);
    }
  }
}

void logMasterCardSave(bool isCardSaved) {
  if (isCardSaved) {
    debugger.logToSerial("Master card saved! Card count = " + String(cards_size));
  } else {
    debugger.logToSerial("Master card CANNOT be saved! Card count = " + String(cards_size));
  }
}

void rfidInit() {
  SPI.begin();                        // Init SPI bus
  mfrc522.PCD_Init();                 // Init MFRC522
  delay(4);                           // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  debugger.logToSerial("Scan PICC to see UID, SAK, type, and data blocks...");
}

void loop() {
  if (!isCardPresent()) {
    return;
  }

  String cardId = cardIdRead();

  if(isMasterCard(cardId)) {
    if(cards_size <= maxCards) {
      int status = processCard();
    
      debugger.logToSerial("Debugger => processCard() - status: " + String(status));
    
      addNewCardIndicator(status);
    } else {
      debugger.logToSerial("Cards array reaches all capacity of " + String(maxCards) + ". Remove a card to add more.");
    }
  } else {
    debugger.logToSerial("Debugger => loop() - Cards id: " + cardId);

    CardsInfoObject newCard;
    newCard.cardID = cardId;

    if (isCardAllowed(newCard)) {
      ledIndicator.indicate(ledIndicator.indicatorType.SUCCESS_TAP);
      openTheDoor();
    } else {
      ledIndicator.indicate(ledIndicator.indicatorType.FAILED_TAP);
    }
  }

  keepDoorClosed();

  delay(1000);

  showCardsInMemory();

  mfrc522.PICC_HaltA(); 

  debugger.logToSerial("RFID sensor is ready to read a card!");

  if(debugger._isDebuggerEnable()) Serial.flush();
}

void addNewCardIndicator(int status) {
  if(status == 200) {
    debugger.logToSerial("Success: Card added!");
    ledIndicator.indicate(ledIndicator.indicatorType.CARD_ADDED_SUCCESS);
  } else if(status == 202){
    debugger.logToSerial("Success: Card removed!");
    ledIndicator.indicate(ledIndicator.indicatorType.CARD_REMOVED);
  } else if(status == 204) {
    debugger.logToSerial("Failed: Card does not exist!");
    ledIndicator.indicate(ledIndicator.indicatorType.CARD_DOES_NOT_EXIST);
  } else if(status == 409) {
    debugger.logToSerial("Failed: Card already exists!");
    ledIndicator.indicate(ledIndicator.indicatorType.CARD_ALREADY_EXISTS);
  } else if(status == 400) {
    debugger.logToSerial("Failed: Master card tapped!");
    ledIndicator.indicate(ledIndicator.indicatorType.MASTER_CARD_NOT_PERMITTED);
  } else if(status == 500) {
    debugger.logToSerial("Failed: Backend side error!");
    ledIndicator.indicate(ledIndicator.indicatorType.BACKEND_ERROR);
  } else {
    debugger.logToSerial("Error: Cannot have status code: " + String(status));
  }
}

void showCardsInMemory() {
  debugger.logToSerial("-------------------------------");
  debugger.logToSerial("Cards in memory (Including Master Card): " + String(cards_size));
  // debugger.logToSerial("Sizeof cards = : " + sizeof(cards));
  debugger.logToSerial("-------------------------------");
}

bool isCardPresent() {
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  return true;
}

String cardIdRead() {
  String cardId;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardId += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardId.trim();
  
  return cardId;
}

bool isCardAllowed(CardsInfoObject cardParam) {
  debugger.logToSerial("isCardAllowed -> cardID param = " + cardParam.cardID);
  
  for(CardsInfoObject card : cards) {
    if(!card.cardID.equalsIgnoreCase("")) {
      debugger.logToSerial("isCardAllowed -> from cards array = " + card.cardID);
    }

    if(card.cardID == cardParam.cardID) { // save master card only if it not exists previously 
      return true;
    }
  }
  return false;
}

bool isCardAlreadyExists(CardsInfoObject card) {
  return isCardAllowed(card);
}

bool isMasterCard(String cardId) {
  return cardId.equals(MASTER_CARD_ID);
}

void waitForCard(Indicator::type indicatorType) {
  while(!isCardPresent()) {
    ledIndicator.indicate(indicatorType);
  }
}

int processCard() {
  debugger.logToSerial("Scan the new card that you want to add...");
  
  ledIndicator.indicate(ledIndicator.indicatorType.ENTER_NEW_CARD_CONDITION);

  waitForCard(ledIndicator.indicatorType.WAITING_CARD);

  String cardId = cardIdRead();

  if(isMasterCard(cardId)) {
    // master card tapped again, so can remove a card from database
    return removeCard();
  }

  CardsInfoObject card = getCardById(cardId);

  if(!card.cardID.equalsIgnoreCase("") && isCardAlreadyExists(card)) {
    return 409;
  }

  // add card only if the card do not already exists
  int status = addNewCard(cardId);
  debugger.logToSerial("processCard method -> addNewCard status = " + String(status));

  return status;
}

int removeCard() {
  debugger.logToSerial("Scan the card that you want to remove...");

  waitForCard(ledIndicator.indicatorType.CARD_REMOVED);

  String cardId = cardIdRead();

  debugger.logToSerial("Trying to remove card ID: " + cardId);

  CardsInfoObject card = getCardById(cardId);

  if(card.cardID.equalsIgnoreCase("") || !isCardAlreadyExists(card)) {
    debugger.logToSerial("Card ID: " + card.cardID + " does not exists to be removed!");

    return 204;
  }

  if(card.isMaster){
    return 400;
  }

  removeCard(card);
  
  return 202;  
}

void removeCard(CardsInfoObject card) {
  debugger.logToSerial("Cards BEFORE removing");
  logCards();
  
  for(size_t i = 0; i < cards_size; i++) {
    if(cards[i].cardID == card.cardID) {
      debugger.logToSerial("Card to be removed. Card id = " + card.cardID);
      
      removeCardFromArray(i);

      break;
    }
  }
  debugger.logToSerial("Cards AFTER removing");
  logCards();
}

int addNewCard(String cardId) {
  CardsInfoObject newCard;
  newCard.cardID = cardId;

  debugger.logToSerial("addNewCard - card = " + newCard.cardID);
  String isMasterStr = newCard.isMaster ? "Yes" : "No";
  debugger.logToSerial("is card master? " + isMasterStr);

  bool isCardSaved = saveCard(newCard);

  if(isCardSaved) {
    return 200;
  }

  return 500;
}

CardsInfoObject getCardById(String cardId) {
  for(CardsInfoObject card : cards) {
    if(card.cardID == cardId) { // save master card only if it not exists previously 
      return card;
    }
  }
  
  CardsInfoObject emptyCard;
  emptyCard.cardID = "";
  
  return emptyCard;
}

void openTheDoor() {
  digitalWrite(RELAY_DOOR_PIN, HIGH);
  // Using this electric lock: intelbras fx 2000 and the guide
  // recommends to keep on for around 1 second (1000ms)
  delay(1100); 
}

void keepDoorClosed() {
  digitalWrite(RELAY_DOOR_PIN, LOW);
}
