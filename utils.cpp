#include "utils.h"

#include <AceTime.h>
using namespace ace_time;
static BasicZoneProcessor timeZoneProcessor;
/***************************************************************************************
**             Convert Unix time to a "local time" time string "12:34"
***************************************************************************************/
String strTime(time_t unixTime)
{
  auto localTz = TimeZone::forZoneInfo(&zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  acetime_t nowSeconds = unixTime;
  ZonedDateTime dateTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);

  String localTime = "";
  char timeChar[40] = {};
  snprintf(timeChar, sizeof(timeChar), "%02d:%02d", dateTime.hour(), dateTime.minute());
  
  return timeChar;
}

/***************************************************************************************
**  Convert Unix time to a local date + time string "Oct 16 17:18", ends with newline
***************************************************************************************/
String strDate(time_t unixTime)
{
  auto localTz = TimeZone::forZoneInfo(&zonedb::kZoneAmerica_New_York, &timeZoneProcessor);
  acetime_t nowSeconds = unixTime;
  ZonedDateTime dateTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);
  char timeChar[40] = {};
  snprintf(timeChar, sizeof(timeChar), "%d/%d %02d:%02d", dateTime.month(), dateTime.day(), dateTime.hour(), dateTime.minute());
  String localDate = timeChar;

  return localDate;
}


void printWeatherCurrent(OpenWeatherOneCall::nowData *current){
    Serial.println("Weather from OpenWeather\n");

  Serial.println("############### Current weather ###############\n");
  Serial.print("dt (time)          : "); Serial.println(strDate(current->dayTime));
  Serial.print("sunrise            : "); Serial.println(strTime(current->sunriseTime));
  Serial.print("sunset             : "); Serial.println(strTime(current->sunsetTime));
  Serial.print("main               : "); Serial.println(current->main);
  Serial.print("temp               : "); Serial.println(current->temperature);
  Serial.print("humidity           : "); Serial.println(current->humidity);
  Serial.print("pressure           : "); Serial.println(current->pressure);
  Serial.print("wind_speed         : "); Serial.println(current->windSpeed);
  Serial.print("wind_deg           : "); Serial.println(current->windBearing);
  Serial.print("clouds             : "); Serial.println(current->cloudCover);
  Serial.print("id                 : "); Serial.println(current->id);
  Serial.println();

  Serial.println("###############  Daily weather  ###############\n");
  Serial.println();

}
void printWeatherForecast(OpenWeatherOneCall::futureData *forecast){
  
  for (int i = 0; i < 5; i++)
  {
    Serial.print("dt (time)          : "); Serial.println(strDate(forecast[i].dayTime));
    Serial.print("id                 : "); Serial.println(forecast[i].id);
    Serial.print("temp_max           : "); Serial.println(forecast[i].temperatureHigh);
    Serial.print("temp_min           : "); Serial.println(forecast[i].temperatureLow);
    Serial.println();
  }
}
