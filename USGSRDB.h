#include <Vector.h>

const int ELEMENT_COUNT_MAX = 75;

class StationReading {
  public:
    StationReading(int timeColumn, int tempColumn, int stageColumn, int flowColumn) {
      this->timeStr = timeColumn;
      this->temp    = tempColumn;
      this->stage   = stageColumn;
      this->flow    = flowColumn;
    };
    StationReading() {};
    virtual ~StationReading(){this->clear();}
    void clear();
    void serialPrint();

    String  line;
    String  timeStr;
    float   temp;
    int     flow;
    float   stage;

  
};

class USGSStation {
  public:
    USGSStation(String siteId);
    ~USGSStation() {this->clear();};

    bool fetch();
   
    void clear();
    void serialPrint();
    StationReading* getLastReading();
 
  private:
    void tokenize(char* line, int length);
    void processHeading(Vector <String> tokens);
    void processReading(Vector <String> tokens);
    
  private:
    char buffer[1000];
    String siteId;

    StationReading reading;
    int timeColumn;
    int tempColumn;
    int stageColumn;
    int flowColumn;
};
