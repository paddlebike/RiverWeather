
//  Example from OpenWeather library: https://github.com/Bodmer/OpenWeather
//  Adapted by Bodmer to use the TFT_eSPI library:  https://github.com/Bodmer/TFT_eSPI

//  This sketch is compatible with the ESP8266 and ESP32

//                           >>>  IMPORTANT  <<<
//         Modify setup in All_Settings.h tab to configure your location etc

//                >>>  EVEN MORE IMPORTANT TO PREVENT CRASHES <<<
//>>>>>>  For ESP8266 set SPIFFS to at least 2Mbytes before uploading files  <<<<<<

//  ESP8266/ESP32 pin connections to the TFT are defined in the TFT_eSPI library.

//  Original by Daniel Eichhorn, see license at end of file.

//#define SERIAL_MESSAGES // For serial output weather reports
//#define SCREEN_SERVER   // For dumping screen shots from TFT
//#define RANDOM_LOCATION // Test only, selects random weather location every refresh
//#define FORMAT_SPIFFS   // Wipe SPIFFS and all files!

// This sketch uses font files created from the Noto family of fonts as bitmaps
// generated from these fonts may be freely distributed:
// https://www.google.com/get/noto/

// A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
// https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font
// New fonts can be generated to include language specific characters. The Noto family
// of fonts has an extensive character set coverage.

// Json streaming parser (do not use IDE library manager version) to use is here:
// https://github.com/Bodmer/JSON_Decoder

#define AA_FONT_SMALL "fonts/NotoSansBold15" // 15 point sans serif bold
#define AA_FONT_LARGE "fonts/NotoSansBold36" // 36 point sans serif bold
/***************************************************************************************
**                          Load the libraries and settings
***************************************************************************************/
#include <Arduino.h>
#include <Wire.h>
#include "FT62XXTouchScreen.h"
#include <SPI.h>

//
// Define these in User_Setup.h in the TFT_eSPI
//
//#define ST7796_DRIVER 1
//#define TFT_WIDTH  480
//#define TFT_HEIGHT 320
//
//#define DISPLAY_WIDTH  480
//#define DISPLAY_HEIGHT 320
//
//#define USE_HSPI_PORT 1
//#define PIN_SDA 18
//#define PIN_SCL 19
//#define TFT_MISO 12
//#define TFT_MOSI 13
//#define TFT_SCLK 14
//#define TFT_CS   15
//#define TFT_DC   21
//#define TFT_RST  22
//#define TFT_BL   23

//#define TOUCH_CS PIN_D2     // Chip select pin (T_CS) of touch screen
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

// Additional functions
#include "GfxUi.h"          // Attached to this sketch
#include "SPIFFS_Support.h" // Attached to this sketch
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager


// check All_Settings.h for adapting to your needs
//#define FREDERICKSBURG
#include "All_Settings.h"
#include <OpenWeatherOneCall.h>

#include <AceTime.h>
#include <AceTimeClock.h>
#define _TASK_SLEEP_ON_IDLE_RUN
#include <TaskScheduler.h>

#include "hydrograph.h"
#include "USGSRDB.h"
#include "utils.h"

// #define FORMAT_SPIFFS 1

#define BLACK 0x0000
#define WHITE 0xFFFF

#define TFT_GREY 0x5AEB

#define SHOW_FORECAST 0
#define SHOW_CURRENT  1
#define SHOW_GRAPH    2

#define SERIAL_MESSAGES 1
#define MAX_DAYS 5

#define WEATHER_START_Y 130
#define LABEL_X 8
/***************************************************************************************
**                          Define the globals and class instances
***************************************************************************************/

using namespace ace_time;
using namespace ace_time::clock;


#define GAUGE_TEXT_START 150
#define METRIC_WEATHER 0

TFT_eSPI tft = TFT_eSPI();             // Invoke custom library
FT62XXTouchScreen touchScreen = FT62XXTouchScreen(DISPLAY_HEIGHT, PIN_SDA, PIN_SCL);

boolean booted = true;

GfxUi ui = GfxUi(&tft); // Jpeg and bmpDraw functions TODO: pull outside of a class

OpenWeatherOneCall OWOC;    // Invoke Weather Library

long lastDownloadUpdate = millis();

void XML_callback(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* data, uint16_t dataLen);
static Hydrograph hydrograph(NWIS_STATION, &XML_callback);

void XML_callback(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* data, uint16_t dataLen) {
  hydrograph.processXML(statusflags, tagName, tagNameLen, data, dataLen);
}
static BasicZoneProcessor timeZoneProcessor;
static NtpClock ntpClock;
static SystemClockLoop systemClock(nullptr /*reference*/, nullptr /*backup*/);
static USGSStation* usgs = new USGSStation(USGS_STATION);

int currentRiverDisplay = SHOW_FORECAST;
Scheduler runner;


void fetchUSGSStation();
void fetchHydrograph();
void fetchWeather();
void updateSystemTime();
void displayTime();
void checkTouch();

// Tasks
Task fetchUSGSStationTask(20 * 60 * 1000, TASK_FOREVER, &fetchUSGSStation, &runner, true);
Task fetchHydrographTask(15 * 60 * 1000, TASK_FOREVER, &fetchHydrograph, &runner, true);
Task fetchWeatherTask(30 * 60 * 1000, TASK_FOREVER, &fetchWeather, &runner, true);
Task updateSystemTimeTask(24 * 60 * 60 * 1000, TASK_FOREVER, &updateSystemTime, &runner, true);
Task displayTimeTask(1000, TASK_FOREVER, &displayTime,  &runner, true);
Task checkTouchTask(100, TASK_FOREVER, &checkTouch, &runner, true);

/***************************************************************************************
**                          Declare prototypes
***************************************************************************************/
void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawCurrentWeather();
void drawForecast();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
const char* getMeteoconIcon(uint16_t id, bool today);
void drawAstronomy();
void drawSeparator(uint16_t y);
void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour);
String strDate(time_t unixTime);
String strTime(time_t unixTime);
void printWeather(void);
int leftOffset(String text, String sub);
int rightOffset(String text, String sub);
int splitIndex(String text);

void drawHydrograph();
void drawUSGSStationReading(StationReading* sr);


void WIFISetUp(void)
{
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  
  Serial.println("Setting up Wifi");
  bool res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
  if(!res) {
      ESP.restart();
  } 
    
  delay(100);

  byte count = 0;
  while(WiFi.status() != WL_CONNECTED && count < 10)
  {
    count ++;
    delay(500);
    Serial.println("Connecting to WiFi");
    
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connect to WiFi    ");
  }
  else
  {
    
    Serial.println("Connection Failed");
    while(1);
  }
  Serial.println("WiFi Setup Done");
  delay(500);
}


/***************************************************************************************
**                          Draw the current weather
***************************************************************************************/
void drawCurrentWeather() {
  if (!OWOC.current) {
    Serial.println("Weather returned no current data");
    return;
  }
  String wind[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW" };
  drawSeparator(100);
  tft.setTextPadding(0); 
  tft.setCursor(LABEL_X,WEATHER_START_Y,2);
  String weatherIcon = "";
  OpenWeatherOneCall::nowData* current = OWOC.current;
  String currentSummary = current->main;
  currentSummary.toLowerCase();

  String weatherText = "None";
  //weatherIcon = getMeteoconIcon(current->id, true);
  //ui.drawBmp("/icon50/" + weatherIcon + ".bmp", 8, WEATHER_START_Y);
  
  

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("Currently:" ,LABEL_X , WEATHER_START_Y + 0);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(current->main,100 , WEATHER_START_Y + 0);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("Temperature:" ,LABEL_X , WEATHER_START_Y + 20);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  weatherText = String(current->temperature) + " F";
  tft.drawString(weatherText, 100, WEATHER_START_Y + 20);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("Wind:" ,LABEL_X , WEATHER_START_Y + 40);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int windAngle = (current->windBearing + 22.5) / 45;
  if (windAngle > 7) windAngle = 0;
  weatherText = wind[windAngle] + " " + (uint16_t)current->windSpeed + " mph";
  tft.drawString(weatherText ,100 , WEATHER_START_Y + 40);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("Barometer:" ,LABEL_X , WEATHER_START_Y + 60);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(current->pressure) ,100 , WEATHER_START_Y + 60);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("Humidity:" ,LABEL_X , WEATHER_START_Y + 80);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(current->humidity) + "%" ,100 , WEATHER_START_Y + 80);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("Clouds:" ,LABEL_X , WEATHER_START_Y + 100);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(current->cloudCover) + "%",100 , WEATHER_START_Y + 100);
    

  //ui.drawBmp("/wind/" + wind[windAngle] + ".bmp", 101, WEATHER_START_Y);

 
  tft.setTextDatum(TL_DATUM); // Reset datum to normal
  tft.setTextPadding(0);      // Reset padding width to none
  
}

/***************************************************************************************
**                          Draw the 5 forecast columns
***************************************************************************************/
// draws the three forecast columns
void displayWeatherForecast() {
  if (currentRiverDisplay == SHOW_FORECAST) {
    int8_t dayIndex = 0;
    drawSeparator(100);
    tft.unloadFont();
    tft.setCursor(8,WEATHER_START_Y,2);
    drawForecastDetail(  8, WEATHER_START_Y, dayIndex++);
    drawForecastDetail( 68, WEATHER_START_Y, dayIndex++); 
    drawForecastDetail(128, WEATHER_START_Y, dayIndex++); 
    drawForecastDetail(188, WEATHER_START_Y, dayIndex++); 
    drawForecastDetail(248, WEATHER_START_Y, dayIndex  ); 
    drawSeparator(WEATHER_START_Y + 110);
  }
}



/***************************************************************************************
**                          Draw 1 forecast column at x, y
***************************************************************************************/
// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {

  if (dayIndex >= MAX_DAYS) return;
  OpenWeatherOneCall::futureData *daily = OWOC.forecast;
  String day  = daily[dayIndex].weekDayName;
  day.toUpperCase();

  tft.setTextDatum(BC_DATUM);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("WWW"));
  tft.drawString(day, x + 25, y);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("-88   -88"));
  String highTemp = String(daily[dayIndex].temperatureHigh, 0);
  String lowTemp  = String(daily[dayIndex].temperatureLow, 0);
  tft.drawString(lowTemp + " - " + highTemp, x + 25, y + 27);

  String weatherIcon = getMeteoconIcon(daily[dayIndex].id, false);

  ui.drawBmp("/icon50/" + weatherIcon + ".bmp", x, y + 28);

  tft.setTextPadding(0); // Reset padding width to none
}

/***************************************************************************************
**                          Draw Sun rise/set, Moon, cloud cover and humidity
***************************************************************************************/
void drawAstronomy() {
  if (!OWOC.forecast) {
    Serial.println("No weather forecast!");
    return;
  }
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Last qtr "));

  auto localTz = TimeZone::forZoneInfo(&zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  acetime_t nowSeconds = systemClock.getNow();
  ZonedDateTime dateTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);

  uint16_t y = dateTime.year();
  uint8_t  m = dateTime.month();
  uint8_t  d = dateTime.day();
  uint8_t  h = dateTime.hour();
  int      ip;
  uint8_t icon = moon_phase(y, m, d, h, &ip);

  tft.drawString(moonPhase[ip], 230, 260);
  ui.drawBmp("/moon/moonphase_L" + String(icon) + ".bmp", 210, 180);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(0); // Reset padding width to none
  tft.drawString("Sunrise:", 200, WEATHER_START_Y + 15);
  tft.drawString("Sunset :", 200, WEATHER_START_Y + 30);

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 88:88 "));

  String rising = OWOC.forecast[0].readableSunrise;
  int dt = rightOffset(rising, ":"); // Draw relative to colon to them aligned
  tft.drawString(rising, 260 + dt, WEATHER_START_Y + 15);

  String setting = OWOC.forecast[0].readableSunset;
  dt = rightOffset(setting, ":");
  tft.drawString(setting, 260 + dt, WEATHER_START_Y + 30);

  tft.setTextPadding(0); // Reset padding width to none
}

/***************************************************************************************
**                          Get the icon file name from the index number
***************************************************************************************/
const char* getMeteoconIcon(uint16_t id, bool today)
{
  // if ( today && id/100 == 8 && (OWOC.current->dt < OWOC.current->sunrise || OWOC.current->dt > OWOC.current->sunset)) id += 1000; 

  if (id/100 == 2) return "thunderstorm";
  if (id/100 == 3) return "drizzle";
  if (id/100 == 4) return "unknown";
  if (id == 500) return "lightRain";
  else if (id == 511) return "sleet";
  else if (id/100 == 5) return "rain";
  if (id >= 611 && id <= 616) return "sleet";
  else if (id/100 == 6) return "snow";
  if (id/100 == 7) return "fog";
  if (id == 800) return "clear-day";
  if (id == 801) return "partly-cloudy-day";
  if (id == 802) return "cloudy";
  if (id == 803) return "cloudy";
  if (id == 804) return "cloudy";
  if (id == 1800) return "clear-night";
  if (id == 1801) return "partly-cloudy-night";
  if (id == 1802) return "cloudy";
  if (id == 1803) return "cloudy";
  if (id == 1804) return "cloudy";

  return "unknown";
}

void displayWeatherCurrent() {
      drawCurrentWeather();
      drawAstronomy();
}


/***************************************************************************************
**                          Draw screen section separator line
***************************************************************************************/
// if you don't want separators, comment out the tft-line
void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, 320 - 2 * 10, 0x4228);
}

/***************************************************************************************
**                          Determine place to split a line line
***************************************************************************************/
// determine the "space" split point in a long string
int splitIndex(String text)
{
  int index = 0;
  while ( (text.indexOf(' ', index) >= 0) && ( index <= text.length() / 2 ) ) {
    index = text.indexOf(' ', index) + 1;
  }
  if (index) index--;
  return index;
}

/***************************************************************************************
**                          Right side offset to a character
***************************************************************************************/
// Calculate coord delta from end of text String to start of sub String contained within that text
// Can be used to vertically right align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int rightOffset(String text, String sub)
{
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(index));
}

/***************************************************************************************
**                          Left side offset to a character
***************************************************************************************/
// Calculate coord delta from start of text String to start of sub String contained within that text
// Can be used to vertically left align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int leftOffset(String text, String sub)
{
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(0, index));
}

/***************************************************************************************
**                          Draw circle segment
***************************************************************************************/
// Draw a segment of a circle, centred on x,y with defined start_angle and subtended sub_angle
// Angles are defined in a clockwise direction with 0 at top
// Segment has radius r and it is plotted in defined colour
// Can be used for pie charts etc, in this sketch it is used for wind direction
#define DEG2RAD 0.0174532925 // Degrees to Radians conversion factor
#define INC 2 // Minimum segment subtended angle and plotting angle increment (in degrees)
void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour)
{
  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x1 = sx * r + x;
  uint16_t y1 = sy * r + y;

  // Draw colour blocks every INC degrees
  for (int i = start_angle; i < start_angle + sub_angle; i += INC) {

    // Calculate pair of coordinates for segment end
    int x2 = cos((i + 1 - 90) * DEG2RAD) * r + x;
    int y2 = sin((i + 1 - 90) * DEG2RAD) * r + y;

    tft.fillTriangle(x1, y1, x2, y2, x, y, colour);

    // Copy segment end to segment start for next segment
    x1 = x2;
    y1 = y2;
  }
}
#if 0
/***************************************************************************************
**                          Print the weather info to the Serial Monitor
***************************************************************************************/
void printWeather(void)
{
#ifdef SERIAL_MESSAGES
  Serial.println("Weather from OpenWeather\n");

  Serial.println("############### Current weather ###############\n");
  Serial.print("dt (time)          : "); Serial.println(strDate(current->dt));
  Serial.print("sunrise            : "); Serial.println(strDate(current->sunrise));
  Serial.print("sunset             : "); Serial.println(strDate(current->sunset));
  Serial.print("main               : "); Serial.println(current->main);
  Serial.print("temp               : "); Serial.println(current->temp);
  Serial.print("humidity           : "); Serial.println(current->humidity);
  Serial.print("pressure           : "); Serial.println(current->pressure);
  Serial.print("wind_speed         : "); Serial.println(current->wind_speed);
  Serial.print("wind_deg           : "); Serial.println(current->wind_deg);
  Serial.print("clouds             : "); Serial.println(current->clouds);
  Serial.print("id                 : "); Serial.println(current->id);
  Serial.println();

  Serial.println("###############  Daily weather  ###############\n");
  Serial.println();

  for (int i = 0; i < 5; i++)
  {
    Serial.print("dt (time)          : "); Serial.println(strDate(daily->dt[i]));
    Serial.print("id                 : "); Serial.println(daily->id[i]);
    Serial.print("temp_max           : "); Serial.println(daily->temp_max[i]);
    Serial.print("temp_min           : "); Serial.println(daily->temp_min[i]);
    Serial.println();
  }

#endif
}
#endif


void drawHydrograph() { 
  tft.fillRect(0, 280, 320, 480, TFT_BLACK);
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(0);
  tft.drawString(FORECAST_LABEL, 220, 280);
  char dateString[20] = {};
  int startY = 300;
  for (int i = 0; i < hydrograph.last_observed; i++) {
    RiverStatus* rs =  &hydrograph.observed_array[i]; 
    utcDateStringToLocalString(rs->dateTime, dateString, sizeof(dateString));
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextPadding(0);
    tft.drawString(dateString, 100, startY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(rs->stage.c_str(), 150, startY);
    startY += 20;
    if (startY > 460) {
      break;
    }
  }
  startY = 300;
  for (int i = 0; i < hydrograph.last_forecast; i++) {
    RiverStatus* rs =  &hydrograph.forecast_array[i]; 
    utcDateStringToLocalString(rs->dateTime, dateString, sizeof(dateString));
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextPadding(0);
    tft.drawString(dateString, 270, startY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(rs->stage.c_str(), 305, startY);
    startY += 20;
    if (startY > 460) {
      break;
    }
  } 
  tft.unloadFont();
}


/***************************************************************************************
**                          Draw the current stream status
***************************************************************************************/

const char* getPlayString(float level) {
  if (level < 3.5f) return "Attainment";
  if (level < 3.65f) return "Low O-Deck!";
  if (level < 3.8f) return "O-Deck!";
  if (level < 4.03f) return "Tweener";
  if (level < 4.25f) return "Low Rocky";
  if (level < 4.5f) return "Rocky";
  if (level < 4.7f) return "High Rocky";
  if (level < 5.0f) return "Oufut";
  if (level < 5.5f) return "Low Center";
  if (level < 6.5f) return "Center";
  if (level < 7.0f) return "High Center";
  if (level < 7.5f) return "Skull";
}


const unsigned int getPlayColor(float level) {
  if (level < 3.5f) return TFT_YELLOW;
  if (level < 3.65f) return TFT_GREEN;
  if (level < 3.8f) return TFT_GREEN;
  if (level < 4.03f) return TFT_YELLOW;
  if (level < 4.25f) return TFT_GREEN;
  if (level < 4.5f) return TFT_GREEN;
  if (level < 4.7f) return TFT_GREEN;
  if (level < 5.0f) return TFT_YELLOW;
  if (level < 5.5f) return TFT_GREEN;
  if (level < 6.5f) return TFT_GREEN;
  if (level < 7.0f) return TFT_GREEN;
  if (level < 7.5f) return TFT_GREEN;
  return TFT_RED;
}

const unsigned int getTempColor(float tempC) {
  if (tempC < 10.0f) return TFT_BLUE;
  if (tempC < 15.0f) return TFT_GREEN;
  if (tempC < 20.0f) return TFT_YELLOW;
  if (tempC < 25.0f) return TFT_ORANGE;
  return TFT_RED;
}

// draws the current USGS stream data
void drawUSGSStationReading(StationReading* sr) {
  tft.fillRect(0, 270, 320, 480, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  drawSeparator(270);
  int valueOffset = 120;
  char scratch[255] = {};
  
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(0);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(CURRENT_LABEL, 220, 280);
  if (sr) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
    tft.drawString("Updated at:", LABEL_X, 295);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(sr->timeStr.c_str(), valueOffset, 295);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Flow:", LABEL_X, 325);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    sprintf(scratch, "%d", sr->flow);
    tft.drawString(scratch, valueOffset, 325);

    
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Height:", LABEL_X, 340);
    int level_width = (int)roundf(sr->stage * (float)30);
    tft.setTextColor(getPlayColor(sr->stage), TFT_BLACK);
    sprintf(scratch,"%2.2f  %s", sr->stage, getPlayString(sr->stage));
    tft.drawString(scratch, valueOffset, 340);

    tft.fillRoundRect(valueOffset,360,level_width,20,5,getPlayColor(sr->stage));

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Temp:", LABEL_X, 390);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    float fahrenheit = (sr->temp * 9.0) / 5.0 + 32;
    sprintf(scratch, "%2.1fC / %2.1fF", sr->temp, fahrenheit);
    tft.drawString(scratch, valueOffset, 390);

    int temp_width = (int) roundf(sr->temp * 5);
    tft.fillRoundRect(valueOffset,410,temp_width,20,5,getTempColor(sr->temp));
  }
  tft.unloadFont();
}

void utcDateStringToLocalString(String dateTime, char* outString, size_t outStringLen) {
  static auto localTz = ace_time::TimeZone::forZoneInfo(&ace_time::zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  ace_time::ZonedDateTime zdt = ace_time::ZonedDateTime::forDateString(dateTime.c_str()).convertToTimeZone(localTz);
  snprintf(outString, outStringLen,"%02d/%02d %02d:%02d\n", zdt.month(), zdt.day(), zdt.hour(), zdt.minute());
}

/***************************************************************************************
**                          Tasks
***************************************************************************************/
void fetchUSGSStation() {
  Serial.println(ESP.getFreeHeap());
  bool success = usgs->fetch();
  if (success) {
    // usgs->serialPrint();
    Serial.println("Getting the last entry");
    StationReading* sr = usgs->getLastReading();
    if (sr && currentRiverDisplay == SHOW_CURRENT) {
      sr->serialPrint();
      drawUSGSStationReading(sr);
    }
  }
  Serial.println(ESP.getFreeHeap());
}


void fetchHydrograph() {
  Serial.println(ESP.getFreeHeap());
  bool parsed = false;

  for(int i=0; i <5; i++) {
    parsed = hydrograph.fetch();
    if (!parsed) {
      Serial.println("hydrograph fetch failed. forecast is empty Will try again");
      delay(2000);
    } else  if (currentRiverDisplay == SHOW_FORECAST) {
      Serial.println("Displaying forecast");
      hydrograph.printForecast();
      drawHydrograph();
      break;
    }
  }

  Serial.println(ESP.getFreeHeap());
}

void fetchWeather() {
  Serial.println(ESP.getFreeHeap());
  int res = OWOC.parseWeather(ONECALLKEY, NULL, NULL, NULL, METRIC_WEATHER, WEATHER_CITY, EXCL_H + EXCL_M + EXCL_A, 0); //<---------excludes hourly, minutely, historical data 1 day
  Serial.printf("Parsing openweather returned %d\n", res);
  if (0 > res) {
    return;
  }
  switch(currentRiverDisplay) {
    case SHOW_FORECAST:
      displayWeatherForecast();
      break;
    case SHOW_CURRENT:
      displayWeatherCurrent();
      break;
  }
  Serial.println(ESP.getFreeHeap());
}



void updateSystemTime(){
  if (!ntpClock.isSetup()) {
    Serial.println(F("NTPClock setup failed... tring again."));
    ntpClock.setup();
  }
  auto localTz = TimeZone::forZoneInfo(&zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  acetime_t nowSeconds = ntpClock.getNow();
  Serial.printf("ntpClock returned %ul\n", nowSeconds);
  ZonedDateTime dateTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);
  Serial.printf("Unix time is %d %02d:%02d\n", dateTime.toEpochSeconds(), dateTime.hour(), dateTime.minute());
  systemClock.setNow(dateTime.toEpochSeconds());
}


void displayTime(){
  int xpos = 0;
  int ypos = 0; 
  
  auto localTz = TimeZone::forZoneInfo(&zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  acetime_t nowSeconds = systemClock.getNow();
  ZonedDateTime dateTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);

  tft.unloadFont();
  tft.setCursor(5,5,8);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.printf("%02d:%02d", dateTime.hour(), dateTime.minute());
  tft.setCursor(260,5,4);
  tft.printf("%s", DateStrings().dayOfWeekShortString(dateTime.dayOfWeek()));
  tft.setCursor(260,31,4);
  tft.printf("%s", DateStrings().monthShortString(dateTime.month()));
  tft.setCursor(260,56,4);
  tft.printf("%d",dateTime.day());
  tft.unloadFont();
}


void checkTouch(){
  // This will report a LOT of events when touched
  TouchPoint p = touchScreen.read();
  if (p.touched) {
    Serial.printf("You touched me at %d x %d\n", p.xPos, p.yPos);
    tft.fillScreen(TFT_BLACK);
    if (currentRiverDisplay == SHOW_FORECAST) {
      currentRiverDisplay = SHOW_CURRENT;
      displayWeatherCurrent();
      drawUSGSStationReading(usgs->getLastReading());
    } else {
      currentRiverDisplay = SHOW_FORECAST;
      displayWeatherForecast();
      drawHydrograph();
    }
  }
}


/***************************************************************************************
**                          Setup
***************************************************************************************/
void setup() {
  Serial.begin(250000);
  tft.begin();
  
  // Backlight hack...
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 128);

  touchScreen.begin();
  tft.fillScreen(TFT_BLACK);

  SPIFFS.begin();
  listFiles();

  // Enable if you want to erase SPIFFS, this takes some time!
  // then disable and reload sketch to avoid reformatting on every boot!
  #ifdef FORMAT_SPIFFS
    tft.setTextDatum(BC_DATUM); // Bottom Centre datum
    tft.drawString("Formatting SPIFFS, so wait!", 120, 195); SPIFFS.format();
  #endif

  // Draw splash screen
  if (SPIFFS.exists("/splash/OpenWeather.jpg")   == true) ui.drawJpeg("/splash/OpenWeather.jpg",   0, 40);

  delay(2000);

  // Clear bottom section of screen
  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);

  tft.loadFont(AA_FONT_SMALL);
  tft.setTextDatum(BC_DATUM); // Bottom Centre datum
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  tft.drawString("Original by: blog.squix.org", 120, 260);
  tft.drawString("Adapted by: Ken Andrews", 120, 280);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  delay(2000);

  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);
  tft.setCursor(18, 240);
  tft.print("If this screen does not disappear\nFollow the instructions to setup WiFi\n");
  

  WIFISetUp();
  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);

  // Setup the NTP Clock
  ntpClock.setup();
  if (!ntpClock.isSetup()) {
    SERIAL_PORT_MONITOR.println(F("NTPClock setup failed... try again."));
  }

  tft.setTextDatum(BC_DATUM);
  tft.setTextPadding(240); // Pad next drawString() text to full width to over-write old text
  tft.drawString(" ", 120, 220);  // Clear line above using set padding width
  tft.drawString("Fetching weather data...", 120, 240);

  tft.unloadFont();
  tft.fillScreen(TFT_BLACK);
  updateSystemTime();
  //fetchWeather();
  //fetchUSGSStation();
  //fetchHydrograph();
  
  runner.startNow();  // set

}

void loop() {
  runner.execute();

}
