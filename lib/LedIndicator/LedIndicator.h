struct Indicator
{
  enum type
  {
    ENTER_NEW_CARD_CONDITION,
    WAITING_CARD,
    MASTER_CARD_NOT_PERMITTED,
    CARD_ALREADY_EXISTS,
    CARD_DOES_NOT_EXIST,
    CARD_ADDED_SUCCESS,
    CARD_REMOVED,
    SUCCESS_TAP,
    FAILED_TAP,
    BACKEND_ERROR
  };
};

class LedIndicator
{
private:
  uint8_t GREEN_LED;
  uint8_t RED_LED;
  void indicatorFactory(Indicator::type type);
  void enterAddCardLedIndicator();
  void waitingNewCardLedIndicator();
  void masterCardNotPermittedLedIndicator();
  void cardAlreadyExistsLedIndicator();
  void cardDoesNotExistLedIndicator();
  void cardAddedSuccessLedIndicator();
  void cardRemovedLedIndicator();
  void cardIsAllowedLedIndicator();
  void cardIsNotAllowedLedIndicator();
  void backendErrorLedIndicator();
  void ledGreenTurnON(int delayTime);
  void ledRedTurnON(int delayTime);
  void ledOrangeTurnON(int delayTime);
  void ledsTurnOFF(int delayTime);

public:
  void indicate(Indicator::type type);
  Indicator indicatorType;

  LedIndicator(uint8_t greenLed, uint8_t redLed)
  {
    GREEN_LED = greenLed;
    RED_LED = redLed;
  }
};

void LedIndicator::indicate(Indicator::type type)
{
  indicatorFactory(type);
}

void LedIndicator::indicatorFactory(Indicator::type type)
{
  switch (type)
  {
  case Indicator::ENTER_NEW_CARD_CONDITION:
    enterAddCardLedIndicator();
    break;
  case Indicator::WAITING_CARD:
    waitingNewCardLedIndicator();
    break;
  case Indicator::MASTER_CARD_NOT_PERMITTED:
    masterCardNotPermittedLedIndicator();
    break;
  case Indicator::CARD_ALREADY_EXISTS:
    cardAlreadyExistsLedIndicator();
    break;
  case Indicator::CARD_DOES_NOT_EXIST:
    cardDoesNotExistLedIndicator();
    break;
  case Indicator::CARD_ADDED_SUCCESS:
    cardAddedSuccessLedIndicator();
    break;
  case Indicator::CARD_REMOVED:
    cardRemovedLedIndicator();
    break;
  case Indicator::SUCCESS_TAP:
    cardIsAllowedLedIndicator();
    break;
  case Indicator::FAILED_TAP:
    cardIsNotAllowedLedIndicator();
    break;
  case Indicator::BACKEND_ERROR:
    backendErrorLedIndicator();
    break;
  }
}

/*
 * Table LED indication meaning:
 * 1 - In the add new card program (only with the master card tapped):
 * * 1.1 - When enter the master card program to add new card will blink: Grenn Led (turn on for 400ms), Orange Led (turn on for 400ms), Red Led (turn on for 400ms)
 * * 1.2 - Waiting a new card to tap on RFID sensor will blink: Green Led (turn on and off for 400ms) until tap a card
 * * 1.3 - If the "new card" tapped is the master card will blink: Orange Led (turn on and off for 400ms) 4 times
 * * 1.4 - If the new card already exists will turn on: Red Led (turn on for 1000ms) 1 time
 * * 1.5 - If the new card was insert with success will: Green Led (turn on for 900ms) and blink 1 time for 100ms
 *
 * 2 - Normal Operational Program
 * * 2.1 - If the card is allowed will: Green Led (turn on for 1000ms) 1 time
 * * 2.2 - If the card is NOT allowed will: Red Led (turn on for 1000ms) 1 time
 * 
 * 3 - Non normal Operational program
 * * 3.1 - General error occurred:  Red Led (turn on for 1400ms) and Orange Led (turn on for 600ms)
 */

// Master Card: add new card
void LedIndicator::enterAddCardLedIndicator()
{
  ledGreenTurnON(400);
  ledsTurnOFF(400);
  ledOrangeTurnON(400);
  ledsTurnOFF(0);
}

void LedIndicator::waitingNewCardLedIndicator()
{
  ledGreenTurnON(400);
  ledsTurnOFF(400);
}

void LedIndicator::masterCardNotPermittedLedIndicator()
{
  ledOrangeTurnON(200);
  ledsTurnOFF(200);
  ledOrangeTurnON(200);
  ledsTurnOFF(200);
  ledOrangeTurnON(200);
  ledsTurnOFF(0);
}

void LedIndicator::cardAlreadyExistsLedIndicator()
{
  ledOrangeTurnON(800);
  ledsTurnOFF(100);
  ledOrangeTurnON(100);
  ledsTurnOFF(100);
  ledOrangeTurnON(100);
  ledsTurnOFF(0);
}

void LedIndicator::cardDoesNotExistLedIndicator()
{
  ledRedTurnON(800);
  ledsTurnOFF(100);
  ledRedTurnON(100);
  ledsTurnOFF(100);
  ledRedTurnON(100);
  ledsTurnOFF(0);
}

void LedIndicator::cardAddedSuccessLedIndicator()
{
  ledGreenTurnON(800);
  ledsTurnOFF(100);
  ledGreenTurnON(100);
  ledsTurnOFF(100);
  ledGreenTurnON(100);
  ledsTurnOFF(100);
  ledGreenTurnON(100);
  ledsTurnOFF(0);
}
// Master Card: add new card

// Master Card: Remove card
void LedIndicator::cardRemovedLedIndicator()
{
  ledRedTurnON(800);
  ledsTurnOFF(100);
  ledRedTurnON(100);
  ledsTurnOFF(100);
  ledRedTurnON(100);
  ledsTurnOFF(100);
  ledRedTurnON(100);
  ledsTurnOFF(0);
}
// Master Card: Remove card

// Normal Operation
void LedIndicator::cardIsAllowedLedIndicator()
{
  ledGreenTurnON(1000);
  ledsTurnOFF(0);
}

void LedIndicator::cardIsNotAllowedLedIndicator()
{
  ledRedTurnON(1000);
  ledsTurnOFF(0);
}
// Normal Operation

// Non Normal Operation
void LedIndicator::backendErrorLedIndicator()
{
  ledRedTurnON(1400);
  ledsTurnOFF(0);
  ledGreenTurnON(600);
  ledsTurnOFF(0);
}
// Non Normal Operation

void LedIndicator::ledGreenTurnON(int delayTime)
{
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  delay(delayTime);
}

void LedIndicator::ledRedTurnON(int delayTime)
{
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  delay(delayTime);
}

void LedIndicator::ledOrangeTurnON(int delayTime)
{
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, HIGH);
  delay(delayTime);
}

void LedIndicator::ledsTurnOFF(int delayTime)
{
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  delay(delayTime);
}

