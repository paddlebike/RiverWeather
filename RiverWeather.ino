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
#include <AceTime.h>
#include <Wire.h>
#include "FT62XXTouchScreen.h"
#include <SPI.h>
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

// Additional functions
#include "GfxUi.h"          // Attached to this sketch
#include "SPIFFS_Support.h" // Attached to this sketch
//#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager


// check All_Settings.h for adapting to your needs
#include "All_Settings.h"

#include <JSON_Decoder.h> // https://github.com/Bodmer/JSON_Decoder
#include <OpenWeather.h>  // Latest here: https://github.com/Bodmer/OpenWeather

#include "NTP_Time.h"     // Attached to this sketch, see that tab for library needs

#include "hydrograph.h"

#include "USGSRDB.h"

//
// Define these in User_Setup.h in the TFT_eSPI
//
#define ST7796_DRIVER 1
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320

#define USE_HSPI_PORT 1
#define PIN_SDA 18
#define PIN_SCL 19
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   21
#define TFT_RST  22
#define TFT_BL   23

//#define TOUCH_CS PIN_D2     // Chip select pin (T_CS) of touch screen

#define BLACK 0x0000
#define WHITE 0xFFFF

#define TFT_GREY 0x5AEB

#define SHOW_FORECAST 0
#define SHOW_CURRENT  1
#define SHOW_GRAPH    2

#define SERIAL_MESSAGES 1
/***************************************************************************************
**                          Define the globals and class instances
***************************************************************************************/

TFT_eSPI tft = TFT_eSPI();             // Invoke custom library
FT62XXTouchScreen touchScreen = FT62XXTouchScreen(DISPLAY_HEIGHT, PIN_SDA, PIN_SCL);

OW_Weather ow;      // Weather forecast library instance

OW_current *current; // Pointers to structs that temporarily holds weather data
OW_hourly  *hourly;  // Not used
OW_daily   *daily;

boolean booted = true;

GfxUi ui = GfxUi(&tft); // Jpeg and bmpDraw functions TODO: pull outside of a class

long lastDownloadUpdate = millis();

static WiFiClient client;
void XML_callback(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* data, uint16_t dataLen);
static Hydrograph hydrograph("brkm2", &XML_callback);

void XML_callback(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* data, uint16_t dataLen) {
  hydrograph.processXML(statusflags, tagName, tagNameLen, data, dataLen);
}
static ace_time::BasicZoneProcessor timeZoneProcessor;
static USGSStation* usgs = new USGSStation("01646500");

int currentRiverDisplay = SHOW_FORECAST;


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

bool fetchHydrograph();
void drawHydrograph();

bool fetchUSGSStation();
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
  tft.drawString("Adapted by: Bodmer", 120, 280);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  delay(2000);

  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);

  tft.drawString("Connecting to WiFi", 120, 240);
  tft.setTextPadding(240); // Pad next drawString() text to full width to over-write old text
#if 0
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
#endif
  WIFISetUp();


  tft.setTextDatum(BC_DATUM);
  tft.setTextPadding(240); // Pad next drawString() text to full width to over-write old text
  tft.drawString(" ", 120, 220);  // Clear line above using set padding width
  tft.drawString("Fetching weather data...", 120, 240);

  // Fetch the time
  udp.begin(localPort);
  syncTime();

  tft.unloadFont();

  fetchHydrograph();

  ow.partialDataSet(true); // Collect a subset of the data available
}

/***************************************************************************************
**                          Loop
***************************************************************************************/
void loop() {

  // Check if we should update weather information
  if (booted || (millis() - lastDownloadUpdate > 1000UL * UPDATE_INTERVAL_SECS))
  {
    updateData();
    lastDownloadUpdate = millis();
  }

  // If minute has changed then request new time from NTP server
  if (booted || minute() != lastMinute)
  {
    // Update displayed time first as we may have to wait for a response
    drawTime();
    lastMinute = minute();

    // Request and synchronise the local clock
    syncTime();

    #ifdef SCREEN_SERVER
      screenServer();
    #endif
  }

  booted = false;

  // This will report a LOT of events when touched
  TouchPoint p = touchScreen.read();
  if (p.touched) {
    Serial.printf("You touched me at %d x %d\n", p.xPos, p.yPos);
    if (currentRiverDisplay == SHOW_FORECAST) {
      fetchUSGSStation();
      currentRiverDisplay = SHOW_CURRENT;
    } else if (currentRiverDisplay == SHOW_CURRENT) {
      currentRiverDisplay = SHOW_FORECAST;
      drawHydrograph();
    }
  }
}

/***************************************************************************************
**                          Fetch the weather data  and update screen
***************************************************************************************/
// Update the Internet based information and update screen
void updateData() {
  // booted = true;  // Test only
  // booted = false; // Test only

  tft.loadFont(AA_FONT_SMALL);

  if (booted) drawProgress(20, "Updating time...");
  else fillSegment(22, 22, 0, (int) (20 * 3.6), 16, TFT_NAVY);

  if (booted) drawProgress(50, "Updating conditions...");
  else fillSegment(22, 22, 0, (int) (50 * 3.6), 16, TFT_NAVY);

  // Create the structures that hold the retrieved weather
  current = new OW_current;
  daily =   new OW_daily;
  hourly =  new OW_hourly;


  bool parsed = ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language);

  printWeather(); // For debug, turn on output with #define SERIAL_MESSAGES

  if (booted)
  {
    drawProgress(100, "Done...");
    delay(2000);
    tft.fillScreen(TFT_BLACK);
  }
  else
  {
    fillSegment(22, 22, 0, 360, 16, TFT_NAVY);
    fillSegment(22, 22, 0, 360, 22, TFT_BLACK);
  }

  if (parsed)
  {
    drawCurrentWeather();
    drawForecast();
    drawAstronomy();

    tft.unloadFont();

    // Update the temperature here so we don't need to keep
    // loading and unloading font which takes time
    tft.loadFont(AA_FONT_LARGE);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

    // Font ASCII code 0xB0 is a degree symbol, but o used instead in small font
    tft.setTextPadding(tft.textWidth(" -88")); // Max width of values

    String weatherText = "";
    weatherText = (int16_t) current->temp;  // Make it integer temperature
    tft.drawString(weatherText, 215, 95); //  + "°" symbol is big... use o in small font
  }
  else
  {
    Serial.println("Failed to get weather");
  }

  // Delete to free up space
  delete current;
  delete hourly;
  delete daily;
  tft.unloadFont();

  //if (usgsStation.fetch()) usgsStation.serialPrint();
  if (fetchHydrograph()) drawHydrograph();
}

/***************************************************************************************
**                          Update progress bar
***************************************************************************************/
void drawProgress(uint8_t percentage, String text) {
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(240);
  tft.drawString(text, 120, 260);

  ui.drawProgressBar(10, 269, 240 - 20, 15, percentage, TFT_WHITE, TFT_BLUE);

  tft.setTextPadding(0);
}

/***************************************************************************************
**                          Draw the clock digits
***************************************************************************************/
void drawTime() {
  tft.loadFont(AA_FONT_LARGE);

  // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);

  String timeNow = "";

  if (hour(local_time) < 10) timeNow += "0";
  timeNow += hour(local_time);
  timeNow += ":";
  if (minute(local_time) < 10) timeNow += "0";
  timeNow += minute(local_time);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 44:44 "));  // String width + margin
  tft.drawString(timeNow, 120, 53);

  drawSeparator(51);

  tft.setTextPadding(0);

  tft.unloadFont();
}

/***************************************************************************************
**                          Draw the current weather
***************************************************************************************/
void drawCurrentWeather() {
  /*
  String date = "Updated: " + strDate(current->dt);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Updated: Mmm 44 44:44 "));  // String width + margin
  tft.drawString(date, 120, 16);
  */
  String weatherIcon = "";

  String currentSummary = current->main;
  currentSummary.toLowerCase();

  String weatherText = "None";
  weatherIcon = getMeteoconIcon(current->id, true);
  ui.drawBmp("/icon/" + weatherIcon + ".bmp", 10, 63);
  
  // Weather Text
  if (language == "en")
    weatherText = current->main;
  else
    weatherText = current->description;

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);

  int splitPoint = 0;
  int xpos = 23;
  tft.drawString(weatherText.c_str(), 40, 120);
  /*
  splitPoint =  splitIndex(weatherText);

  tft.setTextPadding(xpos - 100);  // xpos - icon width
  if (splitPoint) tft.drawString(weatherText.substring(0, splitPoint), xpos, 69);
  else tft.drawString(" ", xpos, 69);
  tft.drawString(weatherText.substring(splitPoint), xpos, 86);
*/
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(0);
  if (units == "metric") tft.drawString("oC", 237, 95);
  else  tft.drawString("oF", 237, 95);

  //Temperature large digits added in updateData() to save swapping font here
 
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  weatherText = (uint16_t)current->wind_speed;

  if (units == "metric") weatherText += " m/s";
  else weatherText += " mph";

  tft.setTextDatum(TC_DATUM);
  tft.setTextPadding(tft.textWidth("888 m/s")); // Max string length?
  tft.drawString(weatherText, 124, 136);

  if (units == "imperial")
  {
    weatherText = current->pressure;
    weatherText += " in";
  }
  else
  {
    weatherText = (uint16_t)current->pressure;
    weatherText += " hPa";
  }

  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth(" 8888hPa")); // Max string length?
  tft.drawString(weatherText, 230, 136);

  int windAngle = (current->wind_deg + 22.5) / 45;
  if (windAngle > 7) windAngle = 0;
  String wind[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW" };
  ui.drawBmp("/wind/" + wind[windAngle] + ".bmp", 101, 86);

  drawSeparator(153);

  tft.setTextDatum(TL_DATUM); // Reset datum to normal
  tft.setTextPadding(0);      // Reset padding width to none
}

/***************************************************************************************
**                          Draw the 4 forecast columns
***************************************************************************************/
// draws the three forecast columns
void drawForecast() {
  int8_t dayIndex = 1;

  drawForecastDetail(  8, 200, dayIndex++);
  drawForecastDetail( 66, 200, dayIndex++); // was 95
  drawForecastDetail(124, 200, dayIndex++); // was 180
  drawForecastDetail(182, 200, dayIndex  ); // was 180
  drawSeparator(205 + 69);
}



/***************************************************************************************
**                          Draw 1 forecast column at x, y
***************************************************************************************/
// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {

  if (dayIndex >= MAX_DAYS) return;

  String day  = shortDOW[weekday(TIMEZONE.toLocal(daily->dt[dayIndex], &tz1_Code))];
  day.toUpperCase();

  tft.setTextDatum(BC_DATUM);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("WWW"));
  tft.drawString(day, x + 25, y);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("-88   -88"));
  String highTemp = String(daily->temp_max[dayIndex], 0);
  String lowTemp  = String(daily->temp_min[dayIndex], 0);
  tft.drawString(highTemp + " " + lowTemp, x + 25, y + 17);

  String weatherIcon = getMeteoconIcon(daily->id[dayIndex], false);

  ui.drawBmp("/icon50/" + weatherIcon + ".bmp", x, y + 18);

  tft.setTextPadding(0); // Reset padding width to none
}

/***************************************************************************************
**                          Draw Sun rise/set, Moon, cloud cover and humidity
***************************************************************************************/
void drawAstronomy() {

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Last qtr "));

  time_t local_time = TIMEZONE.toLocal(current->dt, &tz1_Code);
  uint16_t y = year(local_time);
  uint8_t  m = month(local_time);
  uint8_t  d = day(local_time);
  uint8_t  h = hour(local_time);
  int      ip;
  uint8_t icon = moon_phase(y, m, d, h, &ip);

  tft.drawString(moonPhase[ip], 270, 280);
  ui.drawBmp("/moon/moonphase_L" + String(icon) + ".bmp", 250, 200);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(0); // Reset padding width to none
  tft.drawString(sunStr, 280, 160);

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 88:88 "));

  String rising = strTime(current->sunrise) + " ";
  int dt = rightOffset(rising, ":"); // Draw relative to colon to them aligned
  tft.drawString(rising, 280 + dt, 175);

  String setting = strTime(current->sunset) + " ";
  dt = rightOffset(setting, ":");
  tft.drawString(setting, 280 + dt, 190);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(cloudStr, 280, 90);

  String cloudCover = "";
  cloudCover += current->clouds;
  cloudCover += "%";

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 100%"));
  tft.drawString(cloudCover, 285, 110);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(humidityStr, 280, 130 - 2);

  String humidity = "";
  humidity += current->humidity;
  humidity += "%";

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("100%"));
  tft.drawString(humidity, 285, 145);

  tft.setTextPadding(0); // Reset padding width to none
}

/***************************************************************************************
**                          Get the icon file name from the index number
***************************************************************************************/
const char* getMeteoconIcon(uint16_t id, bool today)
{
  if ( today && id/100 == 8 && (current->dt < current->sunrise || current->dt > current->sunset)) id += 1000; 

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

/***************************************************************************************
**                          Draw screen section separator line
***************************************************************************************/
// if you don't want separators, comment out the tft-line
void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
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
/***************************************************************************************
**             Convert Unix time to a "local time" time string "12:34"
***************************************************************************************/
String strTime(time_t unixTime)
{
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String localTime = "";

  if (hour(local_time) < 10) localTime += "0";
  localTime += hour(local_time);
  localTime += ":";
  if (minute(local_time) < 10) localTime += "0";
  localTime += minute(local_time);

  return localTime;
}

/***************************************************************************************
**  Convert Unix time to a local date + time string "Oct 16 17:18", ends with newline
***************************************************************************************/
String strDate(time_t unixTime)
{
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String localDate = "";

  localDate += monthShortStr(month(local_time));
  localDate += " ";
  localDate += day(local_time);
  localDate += " " + strTime(unixTime);

  return localDate;
}

bool fetchHydrograph() {
  bool parsed = hydrograph.fetch();
  if (!parsed) {
    Serial.println("hydrograph fetch failed. forecast is empty");
    delay(500);
    parsed = hydrograph.fetch();
  }
  if (!parsed) {
    Serial.println("hydrograph re-fetch failed. forecast is empty");
  } else {
    hydrograph.printForecast();
  }
  return parsed;
}

void drawHydrograph() {
  tft.fillRect(0, 280, 320, 480, TFT_BLACK);
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(0);
  tft.drawString("Little Falls Forecast", 220, 280);
  char dateString[20] = {};
  int startY = 300;
  for (auto& element : hydrograph.observed) {
    utcDateStringToLocalString(element->dateTime, dateString, sizeof(dateString));
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextPadding(0);
    tft.drawString(dateString, 100, startY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(element->stage.c_str(), 150, startY);
    startY += 20;
    if (startY > 460) {
      break;
    }
  }
  startY = 300;
  for (auto& element : hydrograph.forecast) {
    utcDateStringToLocalString(element->dateTime, dateString, sizeof(dateString));
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextPadding(0);
    tft.drawString(dateString, 270, startY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(element->stage.c_str(), 305, startY);
    startY += 20;
    if (startY > 460) {
      break;
    }
  }
}

bool fetchUSGSStation() {
  bool success = usgs->fetch();
  if (success) {
    usgs->serialPrint();
    Serial.println("Getting the last entry");
    StationReading* sr = usgs->getLastReading();
    if (sr) {
      sr->serialPrint();
      drawUSGSStationReading(sr);
    }
  }
  return success;
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
}

// draws the current USGS stream data
void drawUSGSStationReading(StationReading* sr) {
  tft.fillRect(0, 280, 320, 480, TFT_BLACK);
  
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(0);
  tft.drawString("Little Falls Conditions\n", 220, 280);
  if (sr) {
    tft.printf("Updated at: %s\n", sr->timeStr.c_str());

    int level_width = (int)roundf(sr->stage * (float)20);
    tft.setTextColor(getPlayColor(sr->stage), TFT_BLACK);

    tft.printf("  Height : %2.2f  %s\n", sr->stage, getPlayString(sr->stage));
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    
    tft.printf("  Flow   : %d\n", sr->flow);
    tft.printf("  Temp   : %2.1fC", sr->temp);
  }
}

void utcDateStringToLocalString(std::string dateTime, char* outString, size_t outStringLen) {
  static auto localTz = ace_time::TimeZone::forZoneInfo(&ace_time::zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  ace_time::ZonedDateTime zdt = ace_time::ZonedDateTime::forDateString(dateTime.c_str()).convertToTimeZone(localTz);
  snprintf(outString, outStringLen,"%02d/%02d %02d:%02d\n", zdt.month(), zdt.day(), zdt.hour(), zdt.minute());
}
/**The MIT License (MIT)
  Copyright (c) 2015 by Daniel Eichhorn
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYBR_DATUM HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  See more at http://blog.squix.ch
*/

//  Changes made by Bodmer:

//  Minor changes to text placement and auto-blanking out old text with background colour padding
//  Moon phase text added (not provided by OpenWeather)
//  Forecast text lines are automatically split onto two lines at a central space (some are long!)
//  Time is printed with colons aligned to tidy display
//  Min and max forecast temperatures spaced out
//  New smart splash startup screen and updated progress messages
//  Display does not need to be blanked between updates
//  Icons nudged about slightly to add wind direction + speed
//  Barometric pressure added

//  Adapted to use the OpenWeather library: https://github.com/Bodmer/OpenWeather
//  Moon phase/rise/set (not provided by OpenWeather) replace with  and cloud cover humidity
//  Created and added new 100x100 and 50x50 pixel weather icons, these are in the
//  sketch data folder, press Ctrl+K to view
//  Add moon icons, eliminate all downloads of icons (may lose server!)
//  Adapted to use anti-aliased fonts, tweaked coords
//  Added forecast for 4th day
//  Added cloud cover and humidity in lieu of Moon rise/set
//  Adapted to be compatible with ESP32
