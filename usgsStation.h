#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <string>
#include <bits/stdc++.h>
#include <cstdlib>

class USGSStation {
  public:
    USGSStation(std::string siteId) { this->siteId = siteId; }
    ~USGSStation() {};

    void setName(char* name) {siteName = name;}
    std::string getName() { return siteName;}

    void setTemperature(char* a) {siteName = a;}
    float getCelsuis() { return strtof(temperature.c_str(), NULL);}
    float getFahrenheit() { return (strtof(temperature.c_str(), NULL) * 9.0) / 5.0 + 32;}

    void setStage(char* a) {stage = a;}
    float getStage() { return strtof(stage.c_str(), NULL);}

    void setFlow(char* a) {flow = a;}
    long getFlow() { return strtol(flow.c_str(), NULL, 10);}

    void setLastUpdate(char* a) {lastUpdate = a;}
    std::string getLastUpdate() { return lastUpdate;}

    bool fetch();
    void serialPrint();

  private:
    bool processJson();
    
  private:
    std::string siteName;
    std::string temperature ;
    std::string stage;
    std::string flow;
    std::string lastUpdate;
    std::string siteId;
};
