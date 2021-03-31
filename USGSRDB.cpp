#include "USGSRDB.h"
#include <HTTPClient.h>
#include <string>
#include <iostream>
#include <sstream>

void StationReading::serialPrint() {
  Serial.printf("%s\t%2.1f\t%d\t%2.2f\n", this->timeStr.c_str(), this->temp, this->flow,  this->stage);
}

bool USGSStation::fetch() {
   char host[255] = {};
   snprintf(host, sizeof(host), "https://waterservices.usgs.gov/nwis/iv/?&parameterCd=00065,00060,00010&period=P1D&format=rdb&sites=%s", siteId.c_str());

   bool success = false;

  HTTPClient http;
  Serial.printf("[HTTP] GET to %s\n", host);
  http.begin(host); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      this->readings.clear();
      String payload = http.getString();
      std::string last;
      std::string line;
      std::stringstream ss;
      ss.str(payload.c_str());
      int lineIndex = 0;
      while(std::getline(ss, line)) {
        if (line[0] != '#') {
          tokenize(line);
        }
      }
    }
  }
}

void USGSStation::tokenize(std::string line) {
    // Vector of string to save tokens 
    std::vector <std::string> tokens; 
      
    // stringstream class check1 
    std::stringstream check1(line); 
    std::string intermediate; 
      
    // Tokenizing w.r.t. tab '\t' 
    while(std::getline(check1, intermediate, '\t')) { 
        tokens.push_back(intermediate); 
    } 
      
    // Printing the token vector 
    for(int i = 0; i < tokens.size(); i++) {
        Serial.printf("Token %d : %s\n", i, tokens[i].c_str());
    }
    if ( !tokens[0].compare("agency_cd") ) {
      processHeading(tokens);
    } else if (!tokens[0].compare("USGS")) {
      processReading(tokens);
    } else {
      Serial.printf("Unknown token %s\n",  tokens[0].c_str());
    }
}

void USGSStation::processHeading(std::vector <std::string> tokens) {
  for(int i = 0; i < tokens.size(); i++) {
    if (tokens[i].find("_cd") == std::string::npos) {
        Serial.printf("Token %d : %s\n", i, tokens[i].c_str());
        if (tokens[i].find("_00010") != std::string::npos) {
          this->tempColumn = i;
        } else if (tokens[i].find("_00060") != std::string::npos) {
          this->flowColumn = i;
        } else if (tokens[i].find("_00065") != std::string::npos) {
          this->stageColumn = i;
        } else if (tokens[i].find("datetime") != std::string::npos) {
          this->timeColumn = i;
        }
      }
   }
}

void USGSStation::processReading(std::vector <std::string> tokens) {
  StationReading *sr = new(StationReading);
  int ts = tokens.size();
  if (this->timeColumn && ts >= this->timeColumn  ) sr->timeStr = tokens[this->timeColumn];
  if (this->tempColumn && ts >= this->tempColumn  ) sr->temp = atof(tokens[this->tempColumn].c_str());
  if (this->flowColumn && ts >= this->flowColumn  ) sr->flow = atoi(tokens[this->flowColumn].c_str());
  if (this->stageColumn && ts >= this->stageColumn) sr->stage = atof(tokens[this->stageColumn].c_str());
  this->readings.push_back(sr); 
}


void USGSStation::clear() {
  this->readings.clear();
}


void USGSStation::serialPrint() {
    for(const auto& sr: this->readings) {
      sr->serialPrint();
    }
}
