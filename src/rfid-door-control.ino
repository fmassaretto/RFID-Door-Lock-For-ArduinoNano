#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include "../EnvVariables.h"
#include "../lib/Debugger/Debugger.h"
#include "../lib/LedIndicator/LedIndicator.h"
#include "../lib/PinoutsBoards/ArduinoNanoPinouts.h"  // You can choose pinouts definition for your board

// RFID reader setup
MFRC522 mfrc522(SS_PIN, RST_PIN);

// LED and debugger setup
LedIndicator ledIndicator(LED_GREEN, LED_RED);
Debugger debugger(true);

// Card storage configuration
const uint8_t maxCards = 16;
uint8_t cards_size = 0;  // Tracks number of cards in the array

// Constants for EEPROM storage
const int EEPROM_HEADER_ADDR = 0;      // Address for the header (version, count)
const int EEPROM_DATA_START = 4;       // Starting address for card data
const int CARD_ID_LENGTH = 10;         // Fixed length for card IDs
const int BYTES_PER_CARD = CARD_ID_LENGTH + 1; // cardID + isMaster flag
const byte EEPROM_VERSION = 1;         // Version of the storage format
const unsigned long CARD_WAIT_TIMEOUT = 30000; // 30 seconds timeout for card wait

// Card structure definition
struct CardsInfoObject {
  String cardID;
  bool isMaster = false;
};

// Card storage array
CardsInfoObject cards[maxCards];

// Function prototypes
void saveCardsToEEPROM();
void loadCardsFromEEPROM();
void printCardInfo();
bool saveCard(CardsInfoObject card);
void removeCardFromArray(int index);
void logCards();
void logMasterCardSave(bool isCardSaved);
void rfidInit();
bool isCardPresent();
String cardIdRead();
bool isCardAllowed(const String& cardId);
bool isMasterCard(const String& cardId);
bool waitForCard(Indicator::type indicatorType);
int processCard();
int removeCard();
void removeCard(const String& cardId);
int addNewCard(const String& cardId);
CardsInfoObject getCardById(const String& cardId);
void openTheDoor();
void keepDoorClosed();
void saveMasterCardToMemory();
void showCardsInMemory();
void addNewCardIndicator(int status);

void setup() {
  // Initialize debugger and serial communication
  debugger.init();
  debugger.logToSerial("Serial initializing");
  
  // Initialize hardware pins
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(RELAY_DOOR_PIN, OUTPUT);
  
  // Ensure door is closed at startup
  keepDoorClosed();
  
  // Load existing cards from EEPROM
  loadCardsFromEEPROM();
  
  // Print the loaded cards
  debugger.logToSerial(F("Loaded cards:"));
  printCardInfo();
  debugger.logToSerial("Cards count = " + String(cards_size));
  
  // Ensure master card exists in memory
  saveMasterCardToMemory();
  
  // Initialize RFID reader
  rfidInit();
  
  debugger.logToSerial("Setup complete - system ready");
}

void loop() {
  // Check if a card is present
  if (!isCardPresent()) {
    return;
  }

  // Read the card ID
  String cardId = cardIdRead();
  debugger.logToSerial("Card detected: " + cardId);

  // Process the card based on whether it's a master card or not
  if (isMasterCard(cardId)) {
    // Master card - enter admin mode
    int status = processCard();
    debugger.logToSerial("Debugger => processCard() - status: " + String(status));
    addNewCardIndicator(status);
  } else {
    // Regular card - check if it's allowed
    debugger.logToSerial("Debugger => loop() - Card ID: " + cardId);

    if (isCardAllowed(cardId)) {
      ledIndicator.indicate(ledIndicator.indicatorType.SUCCESS_TAP);
      openTheDoor();
    } else {
      ledIndicator.indicate(ledIndicator.indicatorType.FAILED_TAP);
    }
  }

  // Ensure door is closed after processing
  keepDoorClosed();
  
  // Delay to prevent multiple reads
  delay(1000);
  
  // Display card count
  showCardsInMemory();
  
  // Halt the card and prepare for next read
  mfrc522.PICC_HaltA(); 
  debugger.logToSerial("RFID sensor is ready to read a card!");
}

// RFID Functions
// --------------

void rfidInit() {
  SPI.begin();                        // Init SPI bus
  mfrc522.PCD_Init();                 // Init MFRC522
  delay(4);                           // Optional delay for initialization
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader
  debugger.logToSerial("RFID reader initialized");
}

bool isCardPresent() {
  // Check if a new card is present
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
  String cardId = "";
  // Convert UID bytes to hexadecimal string
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardId += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardId.trim();
  
  return cardId;
}

// Card Management Functions
// ------------------------

bool saveCard(CardsInfoObject card) {
  debugger.logToSerial("Saving card: " + card.cardID);

  // Check if we have space for a new card
  if (cards_size < maxCards) {
    cards[cards_size] = card;
    cards_size++;
    
    // Save to EEPROM
    saveCardsToEEPROM();
    
    debugger.logToSerial("Card saved successfully");
    debugger.logToSerial("Cards after saving:");
    printCardInfo();
    
    return true;
  }

  debugger.logToSerial("Card not saved - array full!");
  return false;
}

void removeCardFromArray(int index) {
  // Validate index
  if (index < 0 || index >= cards_size) {
    debugger.logToSerial("Invalid index for card removal: " + String(index));
    return;
  }
  
  // Shift all elements after the removed one
  for (size_t i = index; i < cards_size - 1; i++) {
    cards[i] = cards[i+1];
  }
  
  // Clear the last element and decrement size
  cards[cards_size - 1].cardID = "";
  cards[cards_size - 1].isMaster = false;
  cards_size--;
  
  // Save changes to EEPROM
  saveCardsToEEPROM();
  
  debugger.logToSerial("Card removed at index " + String(index) + ", new size: " + String(cards_size));
}

void logCards() {
  debugger.logToSerial("Current cards in memory:");
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cards[i].cardID.length() > 0) {
      debugger.logToSerial(String(i) + ": " + cards[i].cardID + 
                          (cards[i].isMaster ? " (Master)" : ""));
    }
  }
}

bool isCardAllowed(const String& cardId) {
  debugger.logToSerial("Checking if card is allowed: " + cardId);
  
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cards[i].cardID.equals(cardId)) {
      return true;
    }
  }
  return false;
}

bool isMasterCard(const String& cardId) {
  return cardId.equals(MASTER_CARD_ID);
}

CardsInfoObject getCardById(const String& cardId) {
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cards[i].cardID.equals(cardId)) {
      return cards[i];
    }
  }
  
  // Return empty card if not found
  CardsInfoObject emptyCard;
  emptyCard.cardID = "";
  
  return emptyCard;
}

void saveMasterCardToMemory() {
  bool masterCardAlreadyExists = false;

  // Check if master card already exists
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cards[i].cardID.equals(MASTER_CARD_ID)) { 
      masterCardAlreadyExists = true;
      break;
    }
  }

  // Add master card if it doesn't exist
  if (!masterCardAlreadyExists) {
    CardsInfoObject masterCard;
    masterCard.cardID = MASTER_CARD_ID;
    masterCard.isMaster = true;

    bool isCardSaved = saveCard(masterCard);
    logMasterCardSave(isCardSaved);
  }
}

void logMasterCardSave(bool isCardSaved) {
  if (isCardSaved) {
    debugger.logToSerial("Master card saved! Card count = " + String(cards_size));
  } else {
    debugger.logToSerial("Master card CANNOT be saved! Card count = " + String(cards_size));
  }
}

// Admin Mode Functions
// -------------------

bool waitForCard(Indicator::type indicatorType) {
  unsigned long startTime = millis();
  
  while (!isCardPresent()) {
    ledIndicator.indicate(indicatorType);
    
    // Check for timeout
    if (millis() - startTime > CARD_WAIT_TIMEOUT) {
      debugger.logToSerial("Card wait timeout");
      return false;
    }
  }
  
  return true;
}

int processCard() {
  debugger.logToSerial("Admin mode: Scan card to add or remove...");
  
  ledIndicator.indicate(ledIndicator.indicatorType.ENTER_NEW_CARD_CONDITION);

  if (!waitForCard(ledIndicator.indicatorType.WAITING_CARD)) {
    return 500; // Timeout error
  }

  String cardId = cardIdRead();

  if (isMasterCard(cardId)) {
    // Master card tapped again, enter removal mode
    return removeCard();
  }

  // Check if card already exists
  if (isCardAllowed(cardId)) {
    return 409; // Conflict - card already exists
  }

  // Check if we have space for a new card
  if (cards_size >= maxCards) {
    debugger.logToSerial("Cards array full (" + String(maxCards) + " cards). Remove a card to add more.");
    return 500; // Internal error - no space
  }

  // Add the new card
  return addNewCard(cardId);
}

int removeCard() {
  debugger.logToSerial("Admin mode: Scan card to remove...");

  if (!waitForCard(ledIndicator.indicatorType.CARD_REMOVED)) {
    return 500; // Timeout error
  }

  String cardId = cardIdRead();
  debugger.logToSerial("Trying to remove card ID: " + cardId);

  // Check if card exists
  if (!isCardAllowed(cardId)) {
    debugger.logToSerial("Card ID: " + cardId + " does not exist to be removed!");
    return 204; // No content - card not found
  }

  // Don't allow removing master card
  if (isMasterCard(cardId)) {
    return 400; // Bad request - can't remove master
  }

  // Remove the card
  removeCard(cardId);
  return 202; // Accepted - card removed
}

void removeCard(const String& cardId) {
  debugger.logToSerial("Cards BEFORE removing");
  logCards();
  
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cards[i].cardID.equals(cardId)) {
      debugger.logToSerial("Removing card: " + cardId);
      removeCardFromArray(i);
      break;
    }
  }
  
  debugger.logToSerial("Cards AFTER removing");
  logCards();
}

int addNewCard(const String& cardId) {
  CardsInfoObject newCard;
  newCard.cardID = cardId;

  debugger.logToSerial("Adding new card: " + newCard.cardID);
  
  bool isCardSaved = saveCard(newCard);
  return isCardSaved ? 200 : 500; // 200 OK or 500 Internal Error
}

// UI Feedback Functions
// --------------------

void addNewCardIndicator(int status) {
  switch (status) {
    case 200:
      debugger.logToSerial("Success: Card added!");
      ledIndicator.indicate(ledIndicator.indicatorType.CARD_ADDED_SUCCESS);
      break;
    case 202:
      debugger.logToSerial("Success: Card removed!");
      ledIndicator.indicate(ledIndicator.indicatorType.CARD_REMOVED);
      break;
    case 204:
      debugger.logToSerial("Failed: Card does not exist!");
      ledIndicator.indicate(ledIndicator.indicatorType.CARD_DOES_NOT_EXIST);
      break;
    case 409:
      debugger.logToSerial("Failed: Card already exists!");
      ledIndicator.indicate(ledIndicator.indicatorType.CARD_ALREADY_EXISTS);
      break;
    case 400:
      debugger.logToSerial("Failed: Master card cannot be removed!");
      ledIndicator.indicate(ledIndicator.indicatorType.MASTER_CARD_NOT_PERMITTED);
      break;
    case 500:
      debugger.logToSerial("Failed: System error!");
      ledIndicator.indicate(ledIndicator.indicatorType.BACKEND_ERROR);
      break;
    default:
      debugger.logToSerial("Error: Unknown status code: " + String(status));
      break;
  }
}

void showCardsInMemory() {
  debugger.logToSerial("-------------------------------");
  debugger.logToSerial("Cards in memory: " + String(cards_size) + "/" + String(maxCards));
  debugger.logToSerial("-------------------------------");
}

// Door Control Functions
// ---------------------

void openTheDoor() {
  debugger.logToSerial("Opening door");
  digitalWrite(RELAY_DOOR_PIN, HIGH);
  // Using this electric lock: intelbras fx 2000 and the guide
  // recommends to keep on for around 1 second (1000ms)
  delay(1100); 
}

void keepDoorClosed() {
  digitalWrite(RELAY_DOOR_PIN, LOW);
}

// EEPROM Functions
// ---------------

void saveCardsToEEPROM() {
  debugger.logToSerial(F("Saving cards to EEPROM..."));
  
  // Write header: version, count, and maxCards
  EEPROM.write(EEPROM_HEADER_ADDR, EEPROM_VERSION);
  EEPROM.write(EEPROM_HEADER_ADDR + 1, cards_size);
  EEPROM.write(EEPROM_HEADER_ADDR + 2, maxCards & 0xFF);
  EEPROM.write(EEPROM_HEADER_ADDR + 3, (maxCards >> 8) & 0xFF);
  
  // Write card data
  int addr = EEPROM_DATA_START;
  for (uint8_t i = 0; i < cards_size; i++) {
    // Write card ID (fixed length)
    String padded = cards[i].cardID;
    padded.reserve(CARD_ID_LENGTH);
    
    // Pad or truncate to fixed length
    while (padded.length() < CARD_ID_LENGTH) {
      padded += ' ';
    }
    if (padded.length() > CARD_ID_LENGTH) {
      padded = padded.substring(0, CARD_ID_LENGTH);
    }
    
    // Write each character of the card ID
    for (int j = 0; j < CARD_ID_LENGTH; j++) {
      EEPROM.write(addr++, padded.charAt(j));
    }
    
    // Write isMaster flag
    EEPROM.write(addr++, cards[i].isMaster ? 1 : 0);
  }
  
  debugger.logToSerial(F("Cards saved successfully to EEPROM"));
}

void loadCardsFromEEPROM() {
  debugger.logToSerial(F("Loading cards from EEPROM..."));
  
  // Clear the current array
  for (uint8_t i = 0; i < maxCards; i++) {
    cards[i].cardID = "";
    cards[i].isMaster = false;
  }
  cards_size = 0;
  
  // Read header
  byte version = EEPROM.read(EEPROM_HEADER_ADDR);
  byte count = EEPROM.read(EEPROM_HEADER_ADDR + 1);
  int storedMaxCards = EEPROM.read(EEPROM_HEADER_ADDR + 2) | (EEPROM.read(EEPROM_HEADER_ADDR + 3) << 8);
  
  // Check if EEPROM has been initialized with our format
  if (version != EEPROM_VERSION) {
    debugger.logToSerial(F("EEPROM not initialized or version mismatch"));
    return;
  }
  
  // Check if stored maxCards matches current maxCards
  if (storedMaxCards != maxCards) {
    debugger.logToSerial(F("Warning: Stored maxCards doesn't match current maxCards"));
  }
  
  // Read card data
  int addr = EEPROM_DATA_START;
  for (uint8_t i = 0; i < count && i < maxCards; i++) {
    // Read card ID
    String cardID = "";
    for (int j = 0; j < CARD_ID_LENGTH; j++) {
      char c = EEPROM.read(addr++);
      cardID += c;
    }
    
    // Trim trailing spaces
    cardID.trim();
    
    // Read isMaster flag
    bool isMaster = EEPROM.read(addr++) == 1;
    
    // Store in array
    cards[i].cardID = cardID;
    cards[i].isMaster = isMaster;
    cards_size++;
  }

  debugger.logToSerial("Loaded " + String(cards_size) + " cards from EEPROM");
}

// Print all cards in the array
void printCardInfo() {
  int count = 0;
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cards[i].cardID.length() > 0) {
      debugger.logToSerial("Card " + String(i) + ": ID=" + cards[i].cardID + 
                          ", Master=" + (cards[i].isMaster ? "Yes" : "No"));
      count++;
    }
  }
  
  if (count == 0) {
    debugger.logToSerial(F("No cards stored"));
  }
}
