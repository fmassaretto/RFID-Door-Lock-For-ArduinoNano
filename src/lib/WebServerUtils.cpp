#include "WebServerUtils.h"

WebServerUtils::WebServerUtils()
{
    serverHostAPUrl = "http://192.168.4.1";
}

void WebServerUtils::beginWifiClient(String path)
{
    String serverPath = serverHostAPUrl + path;

    Serial.println("WebServerUtils::beginWifiClient -> path: " + serverPath);

    // Your Domain name with URL path or IP address with path
    http.begin(wifiClient, serverPath.c_str());

    // If you need Node-RED/server authentication, insert user and password below
    // http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
}

String WebServerUtils::sendGetRequest(String path)
{
    String responseEntity = "";
    WebServerUtils::beginWifiClient(path);

    DynamicJsonDocument doc(1024);

    doc["statusCode"] = http.GET();
    doc["data"] = http.getString();

    serializeJson(doc, responseEntity);

    Serial.println("ResponseEntity: " + responseEntity);
    http.end();

    return responseEntity;
}