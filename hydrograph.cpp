#include "hydrograph.h"

#define READ_ATTEMPTS 5

void RiverStatus::print() {
  Serial.printf("%s %s %s\n", dateTime.c_str(), stage.c_str(), flow.c_str());
}

Hydrograph::Hydrograph(std::string site, XMLcallback xml_callback) {
  siteCode = site;
  xml.init((uint8_t *)buffer, sizeof(buffer), xml_callback);
};
/*
  //
  // Status flags
  //
  #define STATUS_START_TAG 0x01
  #define STATUS_TAG_TEXT  0x02
  #define STATUS_ATTR_TEXT 0x04
  #define STATUS_END_TAG   0x08
  #define STATUS_ERROR     0x10

*/
void Hydrograph::processXML(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* stuff, uint16_t dataLen) {
  static RiverStatus* inForecast = NULL;
  static RiverStatus* inObesrvered = NULL;
  static std::string currentTag;
  if (statusflags == STATUS_ERROR) {
    //Serial.printf("XML_callback error statusflags 0x%2x tagName %s\n", statusflags, tagName);
    return;
  }

  if (statusflags == STATUS_START_TAG) {
    currentTag = tagName;
  } 

  if (!currentTag.compare("/site")) {
    processSite(statusflags, tagName, stuff);
  } 

  if (!strcasecmp(tagName, "/site/observed/datum")) {
    if (statusflags == STATUS_START_TAG) {
      inObesrvered = new(RiverStatus);
    } else if (statusflags == STATUS_END_TAG) {
      observed.push_back(inObesrvered);
      inObesrvered = NULL;
    }
  } else if (!strcasecmp(tagName, "/site/forecast/datum")) {
    if (statusflags == STATUS_START_TAG) {
      inForecast = new(RiverStatus);
    } else if (statusflags == STATUS_END_TAG) {
      forecast.push_back(inForecast);
      inForecast = NULL;
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
    currentTag.clear();
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
    this->clear();

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      uint32_t t = 0;
      byte     c;
      WiFiClient* wc = http.getStreamPtr();
      if (wc) {
        memset(buffer, sizeof(buffer), 0);
        xml.reset();
        delay(100);
        int attempts = 0;
        while (attempts < READ_ATTEMPTS) {
          if ((wc->read(&c, 1)) == 1) {
            attempts = 0;
            xml.processChar(c);
            t++;
          } else {
            attempts++;
            //Serial.printf("Attempt %d after reading %d bytes\n", attempts, t); 
            delay(100);
          }
        }
        Serial.printf("Finished stream after reading %d bytes\n",t);
      }
      client.stop();
    }

  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return (forecast.size() > 0);
}


void Hydrograph::clear() {
  siteName.clear();
  generationTime.clear();

  for (auto & element : observed) {
    delete(element);
  }
  observed.clear();
  for (auto & element : forecast) {
    delete(element);
  }
  forecast.clear();
}

void Hydrograph::print() {
  
  Serial.println(generationTime.c_str());

  Serial.println("Observations");
  for (auto& element : observed) {
    element->print();
  }
  Serial.println("Forecast");
  for (auto& element : forecast) {
    element->print();
  }

}


void Hydrograph::printForecast() {
  Serial.println(generationTime.c_str());
  Serial.println("Forecast");
  for (auto& element : forecast) {
    element->print();
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
