#ifndef _RIVER_WEATHER_UTILS_H_FILE
#define _RIVER_WEATHER_UTILS_H_FILE
#include <OpenWeatherOneCall.h>
String strTime(time_t unixTime);
String strDate(time_t unixTime);
void printWeatherCurrent(OpenWeatherOneCall::nowData *current);
void printWeatherForecast(OpenWeatherOneCall::futureData *forecast);

#endif
