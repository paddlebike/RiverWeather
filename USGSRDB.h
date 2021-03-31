#include <string>
#include <vector>

class StationReading {
  public:
    StationReading(int timeColumn, int tempColumn, int stageColumn, int flowColumn) {
      this->timeStr = timeColumn;
      this->temp = tempColumn;
      this->stage = stageColumn;
      this->flow = flowColumn;
    };
    StationReading() {};
    ~StationReading() {};
    void serialPrint();

    std::string line;
    std::string timeStr;
    float       temp;
    int         flow;
    float       stage;

  
};

class USGSStation {
  public:
    USGSStation(std::string siteId) { this->siteId = siteId; }
    ~USGSStation() {this->clear();};

    bool fetch();
   
    void clear();
    void serialPrint();
    StationReading* getLastReading(){return this->readings.back();}
    //std::vector<StationReading> getReadings(){return this->readings;}

  private:
    void tokenize(std::string line);
    void processHeading(std::vector <std::string> tokens);
    void processReading(std::vector <std::string> tokens);
    
  private:
    std::string siteId;
    std::vector<StationReading*> readings;
    int timeColumn;
    int tempColumn;
    int stageColumn;
    int flowColumn;
};
