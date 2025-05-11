#ifndef RFIDUtils_h
#define RFIDUtils_h

#include <Arduino.h>
#include "Debugger.h"
#include "FireTimer.h"
#include <SPI.h>
#include <List.hpp>

class RFIDUtils : public Debugger
{
private:
  List<String> cardsIdList;
  FireTimer msTimer;
  void updateCardsIdList();

public:
  bool isCardIdAllowed(String cardIdParam);
  void updateCardsIdListOnSetup();
  void updateCardsIdListOnTime();
  void sendMessageToServer(String message);
  RFIDUtils();
};
#endif