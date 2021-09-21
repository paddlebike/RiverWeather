#include <Arduino.h>
#include <TinyXML.h>


#define HYDROGRAPH_COUNT_MAX 15


class RiverStatus {
  public:
    RiverStatus() {};

    virtual~RiverStatus() {
      dateTime.clear();
      stage.clear();
      flow.clear();
    } 
    void print();

    String dateTime;
    String stage;
    String flow;
};

class Hydrograph {
  public:
    Hydrograph(const String site, XMLcallback xml_callback);

    virtual ~Hydrograph() {clear();}

  
    bool fetch();
    void clear();
    void clearRiverStatus(RiverStatus* rs);
    void print();
    void printForecast();
    void printRiverStatus(RiverStatus* rs);
    void processXML(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* stuff, uint16_t dataLen);

    String siteName;
    String generationTime;
    String forecastIssued;
    
    int last_observed;
    int last_forecast;
    RiverStatus observed_array[HYDROGRAPH_COUNT_MAX];
    RiverStatus forecast_array[HYDROGRAPH_COUNT_MAX];
    

  private:
    void processSite(uint8_t statusflags, char* tagName, char* stuff);
    //void clearRiverStatus(RiverStatus* rs);
    RiverStatus* nextObserved();
    RiverStatus* nextForecast();

    String siteCode;
    char   currentTag[255];
    TinyXML    xml;
    char buffer[1000];

};
