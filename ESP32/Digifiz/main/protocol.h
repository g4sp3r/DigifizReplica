#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "params.h"
#include "setup.h"

//0..64 normal parameters
//128...128+64 read parameters
//223..225 execute commands

#define PARAMETER_RPMCOEFFICIENT 0
#define PARAMETER_SPEEDCOEEFICIENT 1
#define PARAMETER_COOLANTTHERMISTORB 2
#define PARAMETER_OILTHERMISTORB 3
#define PARAMETER_AIRTHERMISTORB 4
#define PARAMETER_TANKMINRESISTANCE 5
#define PARAMETER_TANKMAXRESISTANCE 6
#define PARAMETER_TAU_COOLANT 7
#define PARAMETER_TAU_OIL 8
#define PARAMETER_TAU_AIR 9
#define PARAMETER_TAU_TANK 10
#define PARAMETER_MILEAGE 11
#define PARAMETER_DAILY_MILEAGE 12
#define PARAMETER_AUTO_BRIGHTNESS 13
#define PARAMETER_BRIGHTNESS_LEVEL 14
#define PARAMETER_TANK_CAPACITY 15
#define PARAMETER_MFA_STATE 16
#define PARAMETER_BUZZER_OFF 17
#define PARAMETER_MAX_RPM 18
#define PARAMETER_DOT_OFF 23
#define PARAMETER_BACKLIGHT_ON 24
#define PARAMETER_M_D_FILTER 25
#define PARAMETER_COOLANT_MAX_R 26
#define PARAMETER_COOLANT_MIN_R 27
#define PARAMETER_COMMAND_MFA_RESET 28
#define PARAMETER_COMMAND_MFA_MODE 29
#define PARAMETER_COMMAND_MFA_BLOCK 30
#define PARAMETER_UPTIME 123
#define PARAMETER_READ_ADDITION 128
#define PARAMETER_SET_HOUR 255
#define PARAMETER_SET_MINUTE 254 
#define PARAMETER_RESET_DAILY_MILEAGE 253
#define PARAMETER_RESET_DIGIFIZ 252

//Data acquisition
#define PARAMETER_GET_ACCUMULATED_UPTIME 251
#define PARAMETER_GET_COOLANT_TEMPERATURE 250
#define PARAMETER_GET_OIL_TEMPERATURE 249
#define PARAMETER_GET_AMBIENT_TEMPERATURE 248
#define PARAMETER_GET_FUEL_IN_TANK 247
#define PARAMETER_GET_SPEED 246
#define PARAMETER_GET_RPM 245
#define PARAMETER_SET_DAY 244
#define PARAMETER_SET_MONTH 243
#define PARAMETER_SET_YEAR 242
#define PARAMETER_GET_DAY 241
#define PARAMETER_GET_MONTH 240
#define PARAMETER_GET_YEAR 239
#define PARAMETER_GET_HOUR 238
#define PARAMETER_GET_MINUTE 237
#define PARAMETER_GET_GPIO_PINS 236

void initComProtocol();
void changeBLEName();
void protocolParse();
void processData(int par, long value);
void processGPIOPinsValue(long value);
void printHelp();
void printAbout();

#endif
