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
// bool debug = true;
Debugger debugger(true);
bool clearAllCardsInMemory = false;

// struct CardsCountObject{
//   int count = 0;
// };

struct CardsInfoObject{
  String cardID;
  bool isMaster = false;
};


// Vector<CardsInfoObject> cards;

CardsInfoObject cards[maxCards];

// CardsCountObject cardsCountObj;
// int cardsCountAddress = 0;
int cardsStorageAddress = sizeof(cards);

void setup() {
  debugger.init();
  // if(debug) Serial.begin(9600);
  debugger.logToSerial("Serial iniciando");
  // if(debug) Serial.println("Serial iniciando");

  // preferences.begin(WORKSPACE_NAME, false);

  // if(clearAllCardsInMemory) preferences.clear(); // To clear all data in memory

  // strcpy(cardsStorageObj.key, "cards");
  // strcpy(cardsStorageObj.value, {});

  // EEPROM.put(cardsCountAddress, cardsCountObj);
  // EEPROM.put(cardsStorageAddress, cards);

  // populateCardsAndCounter();

  // cardsCounter =  cardsCountObj.count;//preferences.getUInt("cardsCount", 0);

  debugger.logToSerial("Cards count = " + cards_size);
  // if(debug) {
  //   Serial.print("Cards count = ");
  //   Serial.println(cards_size);
  // }

  //  for(int i=0; i < sizeof(cardsStorageObj.value); i++) {

  //   Serial.print("For= ");
  //   Serial.println(i);
  //  }
  saveMasterCardToMemory();

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(RELAY_DOOR_PIN, OUTPUT);

  keepDoorClosed();

  rfidInit();
  // preferences.end();
}

void populateCardsAndCounter() {
  // EEPROM.get(cardsCountAddress, cardsCountObj);
  EEPROM.get(cardsStorageAddress, cards);
}

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
  // if(!preferences.isKey(MASTER_CARD_ID)){
  //   preferences.putString(MASTER_CARD_ID, "Master Card");
  //   preferences.putUInt("cardsCount", ++cardsCounter);
  // }
}

bool saveCard(CardsInfoObject card) {
  // if(debug) Serial.println("Saving card...");
  debugger.logToSerial("Saving card...");

  if(cards_size <= maxCards) {
    cards[cards_size++] = card;
    EEPROM.put(cardsStorageAddress, cards);
    // if(debug) Serial.println("Card saved!");
    debugger.logToSerial("Card saved!");
    
    return true;
  }

  // if(debug) Serial.println("Card not saved!");
  debugger.logToSerial("Card not saved!");
  return false;
  // EEPROM.put(cardsCountAddress, (cardsCounter+1));
}

void removeCardFromArray(int index) {
  for(size_t i = index; i < cards_size; i++) {
    cards[i] = cards[i+1];
  }

  cards_size--;
  
  EEPROM.put(cardsStorageAddress, cards);
}

void logCards() {
  debugger.logToSerial("Cards AFTER removing");
  for(CardsInfoObject card : cards) {
    debugger.logToSerial(card.cardID);
  }
}

void logMasterCardSave(bool isCardSaved) {
  if (isCardSaved) {
    debugger.logToSerial("Master card saved! Card count = ");
  } else {
    debugger.logToSerial("Master card CANNOT be saved! Card count = ");
  }
  debugger.logToSerial("" + cards_size);
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

  // preferences.begin(WORKSPACE_NAME, false);
  String cardId = cardIdRead();

  if(isMasterCard(cardId)) {
    int status = processCard();
    
    debugger.logToSerial("Debugger => processCard() - status: " + status);

    addNewCardIndicator(status);
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
  // preferences.end();

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
    debugger.logToSerial("Error: Cannot have status code: " + status);
  }
}

void showCardsInMemory() {
  debugger.logToSerial("-------------------------------");
  debugger.logToSerial("Cards in memory (Including Master Card): " + cards_size);
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
    debugger.logToSerial("isCardAllowed -> from cards array = " + card.cardID);

    if(card.cardID == cardParam.cardID) { // save master card only if it not exists previously 
      return true;
    }
  }
  return false;
  // return cards. preferences.isKey(cardId.c_str());
}

bool isCardAlreadyExists(CardsInfoObject card) {
  // return preferences.isKey(cardId.c_str());
  return isCardAllowed(card);
}

bool isMasterCard(String cardId) {
  return cardId.equals(MASTER_CARD_ID);
}

void waitForCard(Indicator::type indicatorType) {
  while(!isCardPresent()) {
    ledIndicator.indicate(indicatorType);
    // ledIndicator.indicate(indicator.indicatorType.WAITING_CARD);
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
  debugger.logToSerial("processCard method -> addNewCard status = " + status);

  return status;
}

int removeCard() {
  // int cardCount = preferences.getUInt("cardsCount");

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
      // EEPROM.put(cardsCountAddress, (cardsCounter-1));

      debugger.logToSerial("Card to be removed. Card id = " + card.cardID);
      
      removeCardFromArray(i);
      break;
    }
  }
  logCards();
}

int addNewCard(String cardId) {
  // int cardCount = preferences.getUInt("cardsCount");
  // int newCardCount = cardCount + 1;

  // String cardName = "Card " + String(newCardCount);

  // preferences.putString(cardId.c_str(), cardName.c_str());
  // preferences.putUInt("cardsCount", newCardCount);

  // CardsInfoObject card = getCardById(cardId);

  CardsInfoObject newCard;
  newCard.cardID = cardId;

  debugger.logToSerial("addNewCard - card = " + newCard.cardID);
  debugger.logToSerial("is card master? " + newCard.isMaster ? "Yes" : "No");

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
