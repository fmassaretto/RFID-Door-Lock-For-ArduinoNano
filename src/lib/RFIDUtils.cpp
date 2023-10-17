#include <Arduino.h>
#include "RFIDUtils.h"
#include "WebServerUtils.h"
#include "FireTimer.h"
#include <ArduinoJson.h>
#include <EEPROM.h>

WebServerUtils webServerUtils;

struct Cards
{
    const char *cardNumberId;
};

const int maxCards = 10;
Cards cards[maxCards];

RFIDUtils::RFIDUtils()
{
    cards[0].cardNumberId = "B1EE2C2B";
    cards[1].cardNumberId = "2644C832";
    cards[2].cardNumberId = "F5766A6C";

    EEPROM.begin(sizeof(cards));

    Serial.println("Writing to EEPROM");
    EEPROM.put(0, cards);

    boolean ok = EEPROM.commit();
    Serial.println((ok) ? "Commit OK" : "Commit failed");

        // msTimer.begin(600000); // 10 min
    msTimer.begin(7200000); // 2 hours
}

bool RFIDUtils::isCardIdAllowed(String cardIdParam)
{
    if (cardsIdList.isEmpty())
    {
        Serial.println("RFIDUtils::isCardIdAllowed => Cards ID list is empty or not fill up yet!");
        return false;
    }

    for (int i = 0; i < cardsIdList.getSize(); i++)
    {
        if (cardsIdList.getValue(i).equalsIgnoreCase(cardIdParam))
        {
            Serial.println("RFIDUtils::isCardIdAllowed => Cards ID is allowed!");
            if (_isDebuggerEnable())
            {
                Serial.println("Debugger => RFIDUtils::isCardIdAllowed => Card ID: " + cardIdParam);
            }
            return true;
        }
    }
    Serial.println("RFIDUtils::isCardIdAllowed => Card ID is NOT allowed!");
    return false;
}

void RFIDUtils::sendMessageToServer(String message)
{
    Serial.println("RFIDUtils::sendMessageToServer => Sending message to server...");
    message.replace(" ", "%20");
    String path = "/displaymessagetosever?message=" + message;
    Serial.println("RFIDUtils::sendMessageToServer => path: " + path);
    webServerUtils.sendGetRequest(path);
}

void RFIDUtils::updateCardsIdListOnSetup()
{
    updateCardsIdList();
}

void RFIDUtils::updateCardsIdListOnTime()
{
    if (msTimer.fire())
    {
        updateCardsIdList();
    }
}

void RFIDUtils::updateCardsIdList()
{
    Serial.println("RFIDUtils::updateCardsIdList => Starting to get all cards id from Database...");
    DynamicJsonDocument doc(1024);

    String path = "/allcardsid";
    List<String> cardsIdListCopy;

    // backup all values from cardsIdList
    cardsIdListCopy.addAll(cardsIdList);

    // clear the array to avoid keeping non desire value inside
    cardsIdList.clear();

    String reponse = webServerUtils.sendGetRequest(path);

    DynamicJsonDocument docRequest(1024);

    DeserializationError error = deserializeJson(docRequest, reponse);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    int statusCode = docRequest["statusCode"];
    const char *data = docRequest["data"];

    if (_isDebuggerEnable())
    {
        Serial.println("Debugger => StatusCode: " + String(statusCode));
        Serial.println("Debugger => Data: " + String(data));
    }

    if (statusCode == 200)
    {
        Serial.println("RFIDUtils::updateCardsIdList => Success! Request to API [" + path + "]");
        Serial.print("RFIDUtils::updateCardsIdList => HTTP Response returns code: ");
        Serial.println(statusCode);

        String payload = data;

        if (_isDebuggerEnable())
        {
            Serial.print("debugger => payload: ");
            Serial.println(payload);
        }

        deserializeJson(doc, payload);

        JsonArray cards = doc["cards"];

        for (JsonObject card : cards)
        {
            cardsIdList.add(card["card_id"].as<String>());
        }

        // egde case: when status is 200 but the list is empty
        if (cards.isNull() || cards.size() <= 0)
        {
            Serial.println("RFIDUtils::updateCardsIdList => Copying from cardsIdListCopy to cardsIdList");
            cardsIdList.addAll(cardsIdListCopy);
            Serial.println("RFIDUtils::updateCardsIdList => Calling this method again...");
            updateCardsIdList();
        }
    }
    else
    {
        Serial.print("RFIDUtils::updateCardsIdList => Error! Response code = ");
        Serial.println(statusCode);
        Serial.println("RFIDUtils::updateCardsIdList => Error! Backuping from cardsIdListCopy list");
        cardsIdList.addAll(cardsIdListCopy);
    }
    Serial.println("RFIDUtilsCalss::updateCardsIdList =>  Valid Card Id count = " + String(cardsIdList.getSize()) + " cards.");

    Cards cards2[maxCards];
    EEPROM.get(0, cards2);
    Serial.println("Card read from EEPROM: " + String(cards2[0].cardNumberId));
}