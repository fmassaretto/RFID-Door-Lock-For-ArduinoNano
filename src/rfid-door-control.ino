#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include "../EnvVariables.h" // Ensure MASTER_CARD_ID is defined here as: const char* MASTER_CARD_ID = "YOUR_HEX_ID";
#include "../lib/Debugger/Debugger.h"
#include "../lib/LedIndicator/LedIndicator.h"
#include "../lib/PinoutsBoards/ArduinoNanoPinouts.h"  // You can choose pinouts definition for your board

bool isDebugEnabled = false;
bool clearCardsInEEPROMExceptMaster = false;

// RFID reader setup
MFRC522 mfrc522(SS_PIN, RST_PIN);

// LED and debugger setup
LedIndicator ledIndicator(LED_GREEN, LED_RED);
Debugger debugger(isDebugEnabled);

// Card storage configuration
const uint8_t maxCards = 16;
uint8_t cards_size = 0;  // Tracks number of cards in the array

// Constants for EEPROM storage
const int EEPROM_HEADER_ADDR = 0;      // Address for the header (version, count)
const int EEPROM_DATA_START = 4;       // Starting address for card data
// *** IMPORTANT: Set CARD_ID_LENGTH to the desired *maximum* number of HEX characters to store for a card ID.
// Common UID lengths are 4 bytes (8 hex chars) or 7 bytes (14 hex chars).
// If you set this too low (like the original 10), longer IDs will be truncated.
const int CARD_ID_LENGTH = 14;         // Max length of HEX card ID to store (e.g., 14 for 7-byte UID)
const int BYTES_PER_CARD = CARD_ID_LENGTH + 1; // cardID (fixed length) + isMaster flag
const byte EEPROM_VERSION = 1;         // Version of the storage format
const unsigned long CARD_WAIT_TIMEOUT = 30000; // 30 seconds timeout for card wait

// Constants for Reading Card ID
const int MAX_UID_BYTES = 7; // Max expected UID size in bytes from reader
const int MAX_HEX_UID_LENGTH = MAX_UID_BYTES * 2; // Max length of hex representation
const int CARD_READ_BUFFER_SIZE = MAX_HEX_UID_LENGTH + 1; // Buffer for reading full hex ID + null terminator

// Card structure definition
struct CardsInfoObject {
  char cardID[CARD_ID_LENGTH + 1]; // Stores the (potentially truncated) card ID as C-string
  bool isMaster = false;
};

// Card storage array
CardsInfoObject cards[maxCards];

// Function prototypes
void saveCardsToEEPROM();
void loadCardsFromEEPROM();
void printCardInfo();
bool saveCard(const CardsInfoObject& card); // Pass by const reference
bool processSavingCard(const CardsInfoObject& card);
void removeCardFromArray(int index);
void logCards();
void logMasterCardSave(bool isCardSaved);
void rfidInit();
bool isCardPresent();
void cardIdRead(char* buffer, size_t bufferSize); // Reads ID into buffer
bool isCardAllowed(const char* cardId);
bool cardsMatches(const char* cardIdOne, const char* cardIdTwo);
bool isMasterCard(const char* cardId);
bool waitForCard(Indicator::type indicatorType, unsigned long startTime);
int processCard();
int removeCard(const char* cardId); // Action function
int processRemoveCard();
bool processWaitingForCard(Indicator::type ledIndicatorType);
int addNewCard(const char* cardId);
CardsInfoObject getCardById(const char* cardId);
void openTheDoor();
void keepDoorClosed();
void saveMasterCardToMemory();
void showCardsInMemory();
void cardStatusLedIndicator(int status);

void setup() {
  // Initialize debugger and serial communication
  debugger.init();
  debugger.logToSerialLn(F("Serial Initialized"));

  // Initialize hardware pins
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(RELAY_DOOR_PIN, OUTPUT);
  debugger.logToSerialLn(F("Pins Initialized"));

  // Ensure door is closed at startup
  keepDoorClosed();

  // Load existing cards from EEPROM
  loadCardsFromEEPROM();

  // Print the loaded cards
  debugger.logToSerialLn(F("Loaded cards:"));
  printCardInfo();
  debugger.logToSerial(F("Cards count = "));
  debugger.logToSerialLn(cards_size);

  // Ensure master card exists in memory
  saveMasterCardToMemory();


  if(clearCardsInEEPROMExceptMaster) {
    // clearAllNonMasterCards();
  }

  // Initialize RFID reader
  rfidInit();

  debugger.logToSerialLn(F("Setup complete - system ready"));
}

void loop() {
  // Check if a card is present
  if (!isCardPresent()) {
    return;
  }

  // Buffer for the card ID read from the card
  char currentCardId[CARD_READ_BUFFER_SIZE];
  cardIdRead(currentCardId, sizeof(currentCardId)); // Read into buffer

  if (strlen(currentCardId) == 0) {
      debugger.logToSerialLn(F("Error reading card ID in loop"));
      mfrc522.PICC_HaltA(); // Halt card even on read error
      delay(500); // Short delay before next attempt
      return;
  }

  debugger.logToSerial(F("Card detected: "));
  debugger.logToSerialLn(currentCardId);

  // Process the card based on whether it's a master card or not
  if (isMasterCard(currentCardId)) {
    // Master card - enter admin mode
    int status = processCard(); // processCard handles its own card reading for add/remove
    cardStatusLedIndicator(status);

    debugger.logToSerial(F("Debugger => processCard() - status: "));
    debugger.logToSerialLn(status);
  } else {
    // Regular card - check if it's allowed
    debugger.logToSerial(F("Debugger => loop() - Checking Card ID: "));
    debugger.logToSerialLn(currentCardId);

    if (isCardAllowed(currentCardId)) {
      ledIndicator.indicate(Indicator::SUCCESS_TAP);
      openTheDoor();
    } else {
      ledIndicator.indicate(Indicator::FAILED_TAP);
    }
  }

  // Ensure door is closed after processing
  keepDoorClosed();

  // Delay to prevent multiple reads (Consider making non-blocking later)
  delay(1000);

  // Display card count
  showCardsInMemory();

  // Halt the card and prepare for next read
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1(); // Recommended after halt
  debugger.logToSerialLn(F("RFID sensor is ready to read a card!"));
}

// RFID Functions
// --------------

void rfidInit() {
  SPI.begin();                        // Init SPI bus
  mfrc522.PCD_Init();                 // Init MFRC522
  delay(4);                           // Optional delay for initialization
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader
  debugger.logToSerialLn(F("RFID reader initialized"));
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

// Reads the card ID into the provided buffer as a null-terminated hex string.
void cardIdRead(char* buffer, size_t bufferSize) {
  buffer[0] = '\0'; // Initialize buffer as empty string
  size_t bufferIndex = 0;

  // Convert UID bytes to hexadecimal string
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Ensure we don't overflow the buffer (2 hex chars per byte + 1 null)
    if (bufferIndex + 2 < bufferSize) {
      // Format each byte as two lowercase hexadecimal characters
      sprintf(buffer + bufferIndex, "%02x", mfrc522.uid.uidByte[i]);
      bufferIndex += 2;
    } else {
      // Buffer too small, stop conversion
      debugger.logToSerialLn(F("Error: cardIdRead buffer too small!"));
      break;
    }
  }
  buffer[bufferIndex] = '\0'; // Null-terminate the string
}

// Card Management Functions
// ------------------------

bool processSavingCard(const CardsInfoObject& card) {
  bool isCardSaved = saveCard(card);

  if(isCardSaved) {
    // Save to EEPROM
    saveCardsToEEPROM();
  }

  printCardInfo();

  return isCardSaved;
}

// Saves the provided card info (passed by reference) to the array and EEPROM.
bool saveCard(const CardsInfoObject& card) {
  debugger.logToSerial(F("Saving card: "));
  debugger.logToSerialLn(card.cardID);

  // Check if we have space for a new card
  if (cards_size < maxCards) {
    // Copy the struct content (including the char array)
    cards[cards_size] = card; // Struct assignment copies members
    cards_size++;

    debugger.logToSerialLn(F("Card saved successfully"));

    return true;
  }

  debugger.logToSerialLn(F("Card not saved - array full!"));
  return false;
}

// Removes a card from the array at the specified index and updates EEPROM.
bool removeCardFromArray(const char* cardId) {
  int index = -1;

  // Finding the index in array for the cardId passed
  for (uint8_t i = 0; i < cards_size; i++) {
    debugger.logToSerial(F("Removing card -> Comparing card id = "));
    debugger.logToSerial(cards[i].cardID);
    debugger.logToSerial(F(" index = "));
    debugger.logToSerial(i);
    debugger.logToSerial(F(" with cardId tapped = "));
    debugger.logToSerialLn(cardId);
    
    if(cardsMatches(cards[i].cardID, cardId)){
      index = i;
      break;
    }
  }

  // Validate index
  if (index < 0 || index >= cards_size) {
    debugger.logToSerial(F("Invalid index for card removal: "));
    debugger.logToSerialLn(index);
    return false;
  }

  // Shift all elements after the removed one (struct assignment works)
  for (uint8_t i = index; i < cards_size - 1; i++) {
    cards[i] = cards[i+1];
  }

  // Clear the last element and decrement size
  cards[cards_size - 1].cardID[0] = '\0'; // Set C-string to empty
  cards[cards_size - 1].isMaster = false;
  cards_size--;

  debugger.logToSerial(F("Card removed at index: "));
  debugger.logToSerial(index);
  debugger.logToSerial(F(", new size: "));
  debugger.logToSerialLn(cards_size);

  return true;
}

// Checks if the given card ID exists in the 'cards' array.
bool isCardAllowed(const char* cardId) {
  debugger.logToSerial(F("Checking if card is allowed: "));
  debugger.logToSerialLn(cardId);

  return cardsMatches(getCardById(cardId).cardID, cardId);
}

bool cardsMatches(const char* cardIdOne, const char* cardIdTwo) {
  return strcmp(cardIdOne, cardIdTwo) == 0;
}

// Checks if the given card ID matches the MASTER_CARD_ID.
bool isMasterCard(const char* cardId) {
  // Assumes MASTER_CARD_ID is defined in EnvVariables.h as a const char*
  // e.g., const char* MASTER_CARD_ID = "1A2B3C4D"; (Use uppercase hex)
  return cardsMatches(cardId, MASTER_CARD_ID);
}

// Finds a card by ID and returns its struct. Returns an empty card if not found.
CardsInfoObject getCardById(const char* cardId) {
  for (uint8_t i = 0; i < cards_size; i++) {
    if (cardsMatches(cards[i].cardID, cardId)) {
      return cards[i];
    }
  }

  // Return empty card if not found
  CardsInfoObject emptyCard;
  emptyCard.cardID[0] = '\0'; // Set as empty C-string
  emptyCard.isMaster = false;

  return emptyCard;
}

// Ensures the MASTER_CARD_ID is present in the 'cards' array.
void saveMasterCardToMemory() {
  // Check if master card already exists
  bool masterCardAlreadyExists = isCardAllowed(MASTER_CARD_ID);

  // Add master card if it doesn't exist
  if (!masterCardAlreadyExists) {
    debugger.logToSerialLn(F("Master card not found in memory, adding..."));
    CardsInfoObject masterCard;
    // Use strncpy for safety, copy MASTER_CARD_ID into the struct field
    strncpy(masterCard.cardID, MASTER_CARD_ID, CARD_ID_LENGTH);
    masterCard.cardID[CARD_ID_LENGTH] = '\0'; // Ensure null termination
    masterCard.isMaster = true;

    processSavingCard(masterCard);
  } else {
     debugger.logToSerialLn(F("Master card already exists in memory."));
  }
}

bool processWaitingForCard(Indicator::type ledIndicatorType) {
  unsigned long startTime = millis();
  bool isWaitingForCard = false;

  while(!isCardPresent()) {
    isWaitingForCard = waitForCard(ledIndicatorType, startTime);

    if(!isWaitingForCard) {
      break;
    }
  }

  return isWaitingForCard;
}

// Waits for a card to be presented, with a timeout.
bool waitForCard(Indicator::type indicatorType, unsigned long startTime) {
  ledIndicator.indicate(indicatorType);

  // Check for timeout
  if (millis() - startTime > CARD_WAIT_TIMEOUT) {
    debugger.logToSerialLn(F("Card wait timeout"));
    ledIndicator.indicate(Indicator::BACKEND_ERROR); // Indicate timeout
    return false;
  }
  delay(50); // Small delay to prevent busy-waiting too aggressively

  return true;
}

// Handles the admin process of adding or initiating removal of a card.
int processCard() {
  debugger.logToSerialLn(F("Admin mode: Scan card to add or remove..."));

  ledIndicator.indicate(Indicator::ENTER_NEW_CARD_CONDITION);

  // Wait for the *first* card tap in admin mode
  if (!processWaitingForCard(Indicator::WAITING_CARD)) {
    return 500; // Timeout error
  }

  // Buffer to hold the read card ID
  char cardId[CARD_READ_BUFFER_SIZE];
  cardIdRead(cardId, sizeof(cardId));

  if (strlen(cardId) == 0) {
      debugger.logToSerialLn(F("Error reading card ID in processCard"));
      return 500; // Error reading card
  }

  debugger.logToSerial(F("Admin mode - Card 1 read: "));
  debugger.logToSerialLn(cardId);

  // Halt this first card immediately after reading
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  if (isMasterCard(cardId)) {
    // Master card tapped again, enter removal mode
    // The removeCard() function will wait for the *next* card tap
    return processRemoveCard();
  }

  // If it wasn't the master card, proceed to check if it can be added

  // Check if card already exists
  if (isCardAllowed(cardId)) {
     debugger.logToSerial(F("Card already exists: "));
     debugger.logToSerialLn(cardId);
    return 409; // Conflict - card already exists
  }

  // Check if we have space for a new card
  if (cards_size >= maxCards) {
    debugger.logToSerial(F("Cards array full ("));
    debugger.logToSerial(maxCards);
    debugger.logToSerialLn(F(" cards). Remove a card to add more."));
    return 500; // Internal error - no space
  }

  // Add the new card (the one read above)
  return addNewCard(cardId);
}

// Handles the admin process of removing a card.
int processRemoveCard() { // This is the mode trigger, waits for the card TO remove
  debugger.logToSerialLn(F("Admin mode: Scan card TO REMOVE..."));

  // Wait for the *second* card tap (the one to be removed)
  if (!processWaitingForCard(Indicator::CARD_REMOVED)) { // Indicator name might be confusing
    return 500; // Timeout error
  }

  // Buffer for the card ID to remove
  char cardIdToRemove[CARD_READ_BUFFER_SIZE];
  cardIdRead(cardIdToRemove, sizeof(cardIdToRemove)); // Read the card to remove

  if (strlen(cardIdToRemove) == 0) {
      debugger.logToSerialLn(F("Error reading card ID in removeCard"));
      return 500; // Error reading card
  }

  // Halt this second card immediately
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  debugger.logToSerial(F("Trying to remove card ID: "));
  debugger.logToSerialLn(cardIdToRemove);

  // Check if card exists
  if (!isCardAllowed(cardIdToRemove)) {
    debugger.logToSerial(F("Card ID: "));
    debugger.logToSerial(cardIdToRemove);
    debugger.logToSerialLn(F(" does not exist to be removed!"));
    return 204; // No content - card not found
  }

  // Don't allow removing master card
  if (isMasterCard(cardIdToRemove)) {
     debugger.logToSerialLn(F("Attempt to remove master card denied."));
    return 400; // Bad request - can't remove master
  }

  // Remove the card (call the overloaded version)
  int status = removeCard(cardIdToRemove);

  debugger.logToSerial(F("Attempt to remove card: "));
  debugger.logToSerial(cardIdToRemove);
  debugger.logToSerial(F(" with status: "));
  debugger.logToSerialLn(status);

  return status;
}

int removeCard(const char* cardId) {
  debugger.logToSerial(F("Removing card: "));
  debugger.logToSerialLn(cardId);

  bool isCardRemoved = removeCardFromArray(cardId);

  if(!isCardRemoved) {
    debugger.logToSerialLn(F("Card cannot be removed from array!"));
    return 404;
  }

  // Save changes to EEPROM
  saveCardsToEEPROM();

  return 202;  // Accepted - card removed
}

// Adds a new card with the given ID.
int addNewCard(const char* cardId) {
  CardsInfoObject newCard;
  // Use strncpy for safety, truncate if longer than CARD_ID_LENGTH
  strncpy(newCard.cardID, cardId, CARD_ID_LENGTH);
  newCard.cardID[CARD_ID_LENGTH] = '\0'; // Ensure null termination
  newCard.isMaster = false; // New cards are never master initially

  debugger.logToSerial(F("Adding new card (final ID): "));
  debugger.logToSerialLn(newCard.cardID);

  return processSavingCard(newCard) ? 200 : 500; // 200 OK or 500 Internal Error
}

// UI Feedback Functions
// --------------------

// Provides LED feedback based on the status code from admin operations.
void cardStatusLedIndicator(int status) {
  switch (status) {
    case 200:
      debugger.logToSerialLn(F("Led Indicator -> Success: Card added!"));
      ledIndicator.indicate(Indicator::CARD_ADDED_SUCCESS);
      break;
    case 202:
      debugger.logToSerialLn(F("Led Indicator -> Success: Card removed!"));
      ledIndicator.indicate(Indicator::CARD_REMOVED);
      break;
    case 204:
      debugger.logToSerialLn(F("Led Indicator -> Failed: Card does not exist!"));
      ledIndicator.indicate(Indicator::CARD_DOES_NOT_EXIST);
      break;
    case 400:
      debugger.logToSerialLn(F("Led Indicator -> Failed: Master card cannot be removed!"));
      ledIndicator.indicate(Indicator::MASTER_CARD_NOT_PERMITTED);
      break;
    case 404:
      debugger.logToSerialLn(F("Led Indicator -> Failed: Card cannot be removed!"));
      ledIndicator.indicate(Indicator::CARD_CANNOT_BE_REMOVED);
      break;
    case 409:
      debugger.logToSerialLn(F("Led Indicator -> Failed: Card already exists!"));
      ledIndicator.indicate(Indicator::CARD_ALREADY_EXISTS);
      break;
    case 500:
      debugger.logToSerialLn(F("Led Indicator -> Failed: System error or timeout!"));
      ledIndicator.indicate(Indicator::BACKEND_ERROR);
      break;
    default:
      debugger.logToSerial(F("Led Indicator -> Error: Unknown status code: "));
      debugger.logToSerialLn(status);
      ledIndicator.indicate(Indicator::BACKEND_ERROR);
      break;
  }
}

// Displays the current card count.
void showCardsInMemory() {
  debugger.logToSerialLn(F("-------------------------------"));
  debugger.logToSerial(F("Cards in memory: "));
  debugger.logToSerial(cards_size);
  debugger.logToSerial(F("/"));
  debugger.logToSerialLn(maxCards);
  debugger.logToSerialLn(F("-------------------------------"));
}

// Door Control Functions
// ---------------------

void openTheDoor() {
  debugger.logToSerialLn(F("Opening door"));
  digitalWrite(RELAY_DOOR_PIN, HIGH);
  // Using this electric lock: intelbras fx 2000 and the guide
  // recommends to keep on for around 1 second (1000ms)
  // Consider making non-blocking later
  delay(1100);
  keepDoorClosed(); // Close immediately after delay
}

void keepDoorClosed() {
  digitalWrite(RELAY_DOOR_PIN, LOW);
}

// EEPROM Functions
// ---------------
// Saves the current 'cards' array to EEPROM using fixed-length storage for IDs.
void saveCardsToEEPROM() {
  debugger.logToSerialLn(F("Saving cards to EEPROM..."));

  // Write header: version, count, and maxCards
  EEPROM.write(EEPROM_HEADER_ADDR, EEPROM_VERSION);
  EEPROM.write(EEPROM_HEADER_ADDR + 1, cards_size);
  EEPROM.put(EEPROM_HEADER_ADDR + 2, (uint16_t)maxCards); // Use EEPROM.put for multi-byte types

  // Write card data
  int addr = EEPROM_DATA_START;
  for (uint8_t i = 0; i < cards_size; i++) {
    // Write card ID (fixed length CARD_ID_LENGTH)
    for (uint8_t j = 0; j < CARD_ID_LENGTH; j++) {
      // Write character if it exists in the source string, otherwise write null terminator ('\\0')
      // This ensures exactly CARD_ID_LENGTH bytes are written for the ID field.
      if (j < strlen(cards[i].cardID)) {
        EEPROM.write(addr++, cards[i].cardID[j]);
      } else {
        EEPROM.write(addr++, '\0'); // Pad with null terminators
      }
    }

    // Write isMaster flag (1 byte)
    EEPROM.write(addr++, cards[i].isMaster ? 1 : 0);
  }

  // Optional: Clear remaining EEPROM slots if needed (e.g., if maxCards decreased or card removed)
  // This ensures old data is not accidentally read later if cards_size decreases.
  // Calculate the address after the last valid card's data
  int endOfValidDataAddr = EEPROM_DATA_START + (cards_size * BYTES_PER_CARD);
  // Calculate the address where the maxCards data would end
  int endOfMaxDataAddr = EEPROM_DATA_START + (maxCards * BYTES_PER_CARD);
  // Clear bytes from the end of valid data up to the maximum possible end address
  // Note: EEPROM.update() only writes if the value is different, saving wear.
  // Use 0xFF or 0x00 as the clear value, depending on preference (0xFF is default unprogrammed state).
  for (int clearAddr = endOfValidDataAddr; clearAddr < endOfMaxDataAddr; clearAddr++) {
      EEPROM.update(clearAddr, 0xFF); // Use update to minimize writes
  }


  debugger.logToSerialLn(F("Cards saved successfully to EEPROM"));
}

// Loads cards from EEPROM into the 'cards' array.
void loadCardsFromEEPROM() {
  debugger.logToSerialLn(F("Loading cards from EEPROM..."));

  // Clear the current array first
  for (uint8_t i = 0; i < maxCards; i++) {
    cards[i].cardID[0] = '\0'; // Clear C-string
    cards[i].isMaster = false;
  }
  cards_size = 0; // Reset count before loading

  // Read header
  byte version = EEPROM.read(EEPROM_HEADER_ADDR);
  byte countInEEPROM = EEPROM.read(EEPROM_HEADER_ADDR + 1);
  uint16_t storedMaxCards;
  EEPROM.get(EEPROM_HEADER_ADDR + 2, storedMaxCards);

  // Check if EEPROM has been initialized with our format
  if (version != EEPROM_VERSION) {
    debugger.logToSerialLn(F("EEPROM not initialized or version mismatch. Initializing..."));
    // Initialize EEPROM with current settings (empty list)
    saveCardsToEEPROM();
    return;
  }

  // Check if stored maxCards matches current maxCards
  if (storedMaxCards != maxCards) {
    debugger.logToSerialLn(F("Warning: Stored maxCards doesn't match current maxCards. Data might be inconsistent."));
    // Consider clearing EEPROM or implementing data migration if format changes
  }

  // Read card data
  int addr = EEPROM_DATA_START;
  for (uint8_t i = 0; i < countInEEPROM && i < maxCards; i++) {
    // Read card ID (fixed length CARD_ID_LENGTH)
    for (int j = 0; j < CARD_ID_LENGTH; j++) {
      cards[i].cardID[j] = EEPROM.read(addr++);
    }
    // Ensure null termination AFTER reading the fixed length
    cards[i].cardID[CARD_ID_LENGTH] = '\0';

    // Read isMaster flag
    cards[i].isMaster = EEPROM.read(addr++) == 1;

    // Only increment cards_size if the loaded cardID is not empty and seems valid
    // This prevents issues if EEPROM contains garbage past the actual count
    if (strlen(cards[i].cardID) > 0) {
         cards_size++; // Increment the live count
    } else {
        // If we read an empty string where countInEEPROM expected one, stop loading
        debugger.logToSerialLn(F("Warning: Found empty card ID before expected count reached. Stopping load."));
        break;
    }
  }

  // Verify the loaded count matches the header count
  if (cards_size != countInEEPROM) {
      debugger.logToSerial(F("Warning: Loaded card count ("));
      debugger.logToSerial(cards_size);
      debugger.logToSerial(F(") does not match EEPROM header count ("));
      debugger.logToSerial(countInEEPROM);
      debugger.logToSerialLn(F("). Using loaded count."));
      // Optionally, resave EEPROM to fix the count header
      // saveCardsToEEPROM();
  }

  debugger.logToSerial(F("Loaded "));
  debugger.logToSerial(cards_size);
  debugger.logToSerialLn(F(" cards from EEPROM"));
}

// Prints details of all cards currently in the 'cards' array.
void printCardInfo() {
  int count = 0;
  debugger.logToSerialLn(F("--- Card List Start ---"));
  for (uint8_t i = 0; i < cards_size; i++) {
    if (strlen(cards[i].cardID) > 0) { // Use strlen
      debugger.logToSerial(F("Card "));
      debugger.logToSerial(i);
      debugger.logToSerial(F(": ID="));
      debugger.logToSerial(cards[i].cardID);
      debugger.logToSerial(F(", Master="));
      debugger.logToSerialLn(cards[i].isMaster ? F("Yes") : F("No"));
      count++;
    } else {
      // This shouldn't happen if cards_size is managed correctly
      debugger.logToSerial(F("Card "));
      debugger.logToSerial(i);
      debugger.logToSerialLn(F(": Found empty slot within cards_size range!"));
    }
  }

  if (count == 0) {
    debugger.logToSerialLn(F("No cards stored"));
  }
   debugger.logToSerialLn(F("--- Card List End ---"));
}