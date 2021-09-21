#include "hydrograph.h"
#include <HTTPClient.h>
#define READ_ATTEMPTS 5


Hydrograph::Hydrograph(const String site, XMLcallback xml_callback) {

  this->siteCode = site;
  this->xml.init((uint8_t *)buffer, sizeof(buffer), xml_callback);
  this->clear();
}
  //
  // Status flags
  //
  #define STATUS_START_TAG 0x01
  #define STATUS_TAG_TEXT  0x02
  #define STATUS_ATTR_TEXT 0x04
  #define STATUS_END_TAG   0x08
  #define STATUS_ERROR     0x10

void Hydrograph::processXML(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* stuff, uint16_t dataLen) {
  static RiverStatus* inForecast = NULL;
  static RiverStatus* inObesrvered = NULL;
  
  if (statusflags == STATUS_ERROR) {
    Serial.printf("XML_callback error statusflags 0x%2x tagName %s\n", statusflags, tagName);
    return;
  }

  if (statusflags == STATUS_START_TAG) {
    strncpy(currentTag, tagName, sizeof(currentTag));
    //Serial.printf("New current tag is %s\n", tagName);
  } 

 
  if (!strcasecmp(currentTag,"/site")) {
    processSite(statusflags, tagName, stuff);
  } 
  //Serial.printf("XML current tag is %s\n", tagName);
  if (!strcasecmp(tagName, "/site/observed/datum")) {
    if (statusflags == STATUS_START_TAG) {
      inObesrvered = this->nextObserved();
    } 
  } else if (!strcasecmp(tagName, "/site/forecast/datum")) {
    if (statusflags == STATUS_START_TAG) {
      inForecast = this->nextForecast();
    } 
  } 
  if (statusflags == STATUS_TAG_TEXT) {
    if (inObesrvered) {
      if (!strcasecmp(tagName, "/site/observed/datum/primary"))   {
        inObesrvered->stage = stuff;
      }
      else if (!strcasecmp(tagName, "/site/observed/datum/secondary")) {
        inObesrvered->flow = stuff;
      }
      else if (!strcasecmp(tagName, "/site/observed/datum/valid"))     {
        inObesrvered->dateTime = stuff;
      }
    }
    
    else if (inForecast) {
      if (!strcasecmp(tagName, "/site/forecast/datum/primary"))   {
        inForecast->stage = stuff;
      }
      else if (!strcasecmp(tagName, "/site/forecast/datum/secondary")) {
        inForecast->flow = stuff;
      }
      else if (!strcasecmp(tagName, "/site/forecast/datum/valid"))     {
        inForecast->dateTime = stuff;
      }
    }
  }
  if (statusflags == STATUS_END_TAG) {
    currentTag[0] = '\0';
  } 
}

bool Hydrograph::fetch() {

  HTTPClient http;
  char host[255] = {};
  snprintf(host, sizeof(host), "https://water.weather.gov/ahps2/hydrograph_to_xml.php?gage=%s&output=xml", siteCode.c_str());

  Serial.printf("[HTTP] GET to %s\n", host);
  http.begin(host); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      // get lenght of document (is -1 when Server sends no Content-Length header)
      delay(100);
      int len = http.getSize();
      Serial.printf("[HTTP] GET... length: %d\n", len);
      if (len > 0) {
        this->clear();
      }
      byte     c;
      WiFiClient* stream = http.getStreamPtr();
      while(http.connected() && (len > 0 || len == -1)) {
          // get available data size
          size_t size = stream->available();
          if(size) {
              c = stream->read();
              if(c != -1 && c != '\0') {
                  len--;
                  xml.processChar(c);
              } else {
                Serial.printf("[HTTP] Error processing char %d\n",(int) c);
                break;
              }
          } else {
            delay(1);
          }
      }
    }

  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return (this->last_forecast > 0);

}


void Hydrograph::clear() {
  memset(this->buffer, 0 , sizeof(this->buffer));
  xml.reset();
  this->siteName.clear();
  this->generationTime.clear();
  this->last_observed = 0;
  this->last_forecast = 0;
  for(int i = 0; i < HYDROGRAPH_COUNT_MAX; i++) {
    clearRiverStatus(&this->observed_array[i]);
    clearRiverStatus(&this->forecast_array[i]);
  }
}

void Hydrograph::clearRiverStatus(RiverStatus* rs) {
  if (rs){
    rs->dateTime.clear();
    rs->stage.clear();
    rs->flow.clear();
  }
}

RiverStatus* Hydrograph::nextObserved() {
  if (this->last_observed < HYDROGRAPH_COUNT_MAX) {
    return &this->observed_array[this->last_observed++];
  } 
  return NULL;
}

RiverStatus* Hydrograph::nextForecast() {
  if (this->last_forecast < HYDROGRAPH_COUNT_MAX) {
    return &this->forecast_array[this->last_forecast++];
  } 
  return NULL;
}

void Hydrograph::printRiverStatus(RiverStatus* rs) {
  if (rs){
    Serial.printf("%s %s %s\n", rs->dateTime.c_str(), rs->stage.c_str(), rs->flow.c_str());
  }
}

void Hydrograph::print() {
  
  Serial.println(generationTime.c_str());

  Serial.println("Observations");
  for(int i = 0; i < HYDROGRAPH_COUNT_MAX; i++) {
    printRiverStatus(&this->observed_array[i]);
  }
  
  Serial.println("Forecast");
  for(int i = 0; i < HYDROGRAPH_COUNT_MAX; i++) {
    printRiverStatus(&this->forecast_array[i]);
  }

}


void Hydrograph::printForecast() {
  Serial.println(generationTime.c_str());
  Serial.println("Forecast");
  for(int i = 0; i < HYDROGRAPH_COUNT_MAX; i++) {
    printRiverStatus(&this->forecast_array[i]);
  }
}

void Hydrograph::processSite(uint8_t statusflags, char* tagName, char* stuff) {
  if (!strcasecmp(tagName, "generationtime")) {
      Serial.printf("Generation Time = %s\n", stuff);
      this->generationTime = stuff;
    } else if (!strcasecmp(tagName, "name")) {
      Serial.printf("Name = %s\n", stuff);
      this->siteName = stuff;
    }
}
