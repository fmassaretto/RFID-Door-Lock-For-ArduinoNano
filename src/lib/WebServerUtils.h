#ifndef WebServerUtils_h
#define WebServerUtils_h

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

class WebServerUtils
{
private:
    String serverHostAPUrl;
    WiFiClient wifiClient;
    HTTPClient http;
    void beginWifiClient(String path);

public:
    String sendGetRequest(String path);
    WebServerUtils();
};

#endif