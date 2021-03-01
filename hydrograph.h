#include <string>
#include <vector>
#include <HTTPClient.h>
#include <TinyXML.h>


class RiverStatus {
  public:
    std::string dateTime;
    std::string stage;
    std::string flow;

    RiverStatus() {};
    ~RiverStatus() {};

    void print();
};

class Hydrograph {
  public:
    std::string siteName;
    std::string generationTime;
    std::vector<RiverStatus*> observed;
    std::string forecastIssued;
    std::vector<RiverStatus*> forecast;

    Hydrograph(std::string site, XMLcallback xml_callback);
    ~Hydrograph() {clear();}

    bool fetch();
    void clear();
    void print();
    void printForecast();
    void processXML(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* stuff, uint16_t dataLen);

    private:
    std::string siteCode;
    WiFiClient client;
    TinyXML    xml;
    char buffer[1000];

    void processSite(uint8_t statusflags, char* tagName, char* stuff);
};
