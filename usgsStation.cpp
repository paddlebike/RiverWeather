#include "usgsStation.h"

static StaticJsonDocument<15000> doc;


bool USGSStation::fetch() {
   char host[255] = {};
   snprintf(host, sizeof(host), "https://waterservices.usgs.gov/nwis/iv/?format=json&parameterCd=00065,00060,00010&sites=%s", siteId.c_str());

   bool success = false;

    HTTPClient http;
    
    Serial.printf("[HTTP] GET to %s\n", host);
    http.begin(host);
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            
            
            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                Serial.print(F("deSerializeJson() failed: "));
                Serial.println(error.c_str());
                return success;
            }
            return processJson();
            
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
  
}


bool USGSStation::processJson() {
    JsonArray value_timeSeries = doc["value"]["timeSeries"];
    JsonObject value_timeSeries_0 = value_timeSeries[0];
    JsonObject value_timeSeries_0_sourceInfo = value_timeSeries_0["sourceInfo"];
    siteName = (const char*) value_timeSeries_0_sourceInfo["siteName"];

    temperature = (const char*) value_timeSeries[0]["values"][4]["value"][0]["value"];
    flow        = (const char*) value_timeSeries[1]["values"][0]["value"][0]["value"];
    stage       = (const char*) value_timeSeries[2]["values"][0]["value"][0]["value"];
    lastUpdate  = (const char*) value_timeSeries[2]["values"][0]["value"][0]["dateTime"];

    return true;
}

void USGSStation::serialPrint() {
  Serial.printf("Site ID %s %s\n", siteId.c_str(), siteName.c_str());
  Serial.printf("Stage %2.2f at %s\n", getStage(), lastUpdate.c_str());
  Serial.printf("Flow  %d\n", flow.c_str(), getFlow());
  Serial.printf("Temp Celsuis %2.2f Fahrenheit %2.1f\n", getCelsuis(), getFahrenheit());
}
