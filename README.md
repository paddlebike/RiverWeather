# RiverWeather
 
## An Ardiono ESP32 project to display conditions for the potomac river upstream from Washington DC.

It uses the WT32-SC01 board from seeed studio https://www.seeedstudio.com/ESP32-Development-board-WT32-SC01-p-4735.htm

## Libraries used:
 * TFT_eSPI library:  https://github.com/Bodmer/TFT_eSPI
 * OpenWeather:       https://openweathermap.org/api/one-call-api
 * WiFi Manager:      https://github.com/tzapu/WiFiManager
 * NTP Client:        https://github.com/arduino-libraries/NTPClient
 * ACETime:           https://github.com/bxparks/AceTime
 * TaskScheduler:     https://github.com/arkhipenko/TaskScheduler

## User_Setup.h for TFT_eSPI library

```
// User defined setup
#define ST7796_DRIVER 1
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

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
```
