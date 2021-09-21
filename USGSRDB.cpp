#include "USGSRDB.h"
#include <HTTPClient.h>

const int TOKEN_COUNT_MAX = 15;
typedef Vector<String> Tokens;

void StationReading::clear(){
//  delete(this->line);
//  delete(this->timeStr);
}


void StationReading::serialPrint() {
  Serial.printf("%s\t%2.1f\t%d\t%2.2f\n", this->timeStr.c_str(), this->temp, this->flow,  this->stage);
}


StationReading* USGSStation::getLastReading(){
  return &this->reading;
 }

USGSStation::USGSStation(String siteId) { 
  this->siteId = siteId; 
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
  Serial.printf("[HTTP] GET... code: %d\n", httpCode);
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    if (httpCode == HTTP_CODE_OK) {
      // get lenght of document (is -1 when Server sends no Content-Length header)
      delay(100);
      int len = http.getSize();
      this->reading.clear();
      uint32_t t = 0;
      byte     c;
      WiFiClient* stream = http.getStreamPtr();
      while(http.connected() && (len > 0 || len == -1)) {
          // get available data size
          size_t size = stream->available();
          if(size) {
              int c =  stream->readBytesUntil('\n', buffer, sizeof(buffer));
              if(len > 0) {
                  len -= c;
              }
              buffer[c] = 0x00;
              tokenize(buffer, c);
          }
          delay(1);
      }
    }
  } else {
    Serial.print("What do I do when I get a negative response?");
  }
  http.end();
  
}

void USGSStation::tokenize(char* line, int len) {
    //Serial.printf("RDB String %s\n", line);
    // Vector of string to save tokens 
    String token_array[TOKEN_COUNT_MAX];
    Tokens tokens;
    tokens.setStorage(token_array);
    
    char* pch = strtok(line,"\t");
    while (pch != NULL)
    {
      tokens.push_back(pch);
      pch = strtok (NULL,"\t");
    }

    if (tokens.size() > 0) {
      if ( !tokens[0].compareTo("agency_cd") ) {
        processHeading(tokens);
      } else if (!tokens[0].compareTo("USGS")) {
        processReading(tokens);
      } /* else {
        Serial.printf("Unknown token %s\n",  tokens[0].c_str());
      } */
    } else {
      Serial.println("No tokens!");
    }
}

void USGSStation::processHeading(Vector <String> tokens) {
  for(int i = 0; i < tokens.size(); i++) {
    if (tokens[i].indexOf("_cd") == -1) {
        Serial.printf("Token %d : %s\n", i, tokens[i].c_str());
        if (tokens[i].indexOf("_00010") != -1) {
          this->tempColumn = i;
        } else if (tokens[i].indexOf("_00060") != -1) {
          this->flowColumn = i;
        } else if (tokens[i].indexOf("_00065") != -1) {
          this->stageColumn = i;
        } else if (tokens[i].indexOf("datetime") != -1) {
          this->timeColumn = i;
        }
      }
   }
}


void USGSStation::processReading(Vector <String> tokens) {
  int ts = tokens.size();
  if (this->timeColumn && ts >= this->timeColumn  ) this->reading.timeStr = tokens[this->timeColumn];
  if (this->tempColumn && ts >= this->tempColumn  ) this->reading.temp = tokens[this->tempColumn].toFloat();
  if (this->flowColumn && ts >= this->flowColumn  ) this->reading.flow = tokens[this->flowColumn].toFloat();
  if (this->stageColumn && ts >= this->stageColumn) this->reading.stage = tokens[this->stageColumn].toFloat(); 
}


void USGSStation::clear() {
  this->reading.clear();
}


void USGSStation::serialPrint() {
    this->reading.serialPrint();
}
