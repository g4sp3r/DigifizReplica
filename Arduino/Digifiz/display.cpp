#include "display.h"

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, 2);
MD_MAX72XX mx2 = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN2, CLK_PIN2, CS_PIN2, 3);

extern digifiz_pars digifiz_parameters;

#define EMULATE_RTC
#ifdef EMULATE_RTC
extern RTC_Millis myRTC;
#else
extern RTC_DS3231 myRTC;
#endif

extern DateTime startTime;
int mRPMData = 4000;
//int mHour = 0;
//int mMinute = 0;

void initDisplay()
{
    mx.begin();
    mx2.begin();
    mx.clear();
    mx2.clear();
    pinMode(MFA1_PIN,OUTPUT);
    pinMode(MFA2_PIN,OUTPUT);
}

void setRPM(int rpmdata)
{
    mRPMData=rpmdata;
}

void blinking()
{
    // Uses the test function of the MAX72xx to blink the display on and off.
    int  nDelay = 1000;

    PRINTS("\nBlinking");
    mx.clear();

    for (uint8_t col=0; col<12; col++)
    {
    mx.setColumn(col, 0xff);
    delay(100);
    mx.setColumn(col, 0x00);
    delay(100);
    }
}

void blinking2()
{
    // Uses the test function of the MAX72xx to blink the display on and off.
    int  nDelay = 1000;

    PRINTS("\nBlinking");
    mx2.clear();

    for (uint8_t col=0; col<24; col++)
    {
    mx2.setColumn(col, 0xff);
    delay(100);
    mx2.setColumn(col, 0x00);
    delay(100);
    }
}

void setMFABlock(uint8_t block)
{
  if (block&0x1)
  {
      digitalWrite(MFA1_PIN,HIGH);
      digitalWrite(MFA2_PIN,LOW);
  }
  else
  {
      digitalWrite(MFA1_PIN,LOW);
      digitalWrite(MFA2_PIN,HIGH);
  }
}

void displayMFAType(uint8_t mfaType)
{
    DateTime newTime = myRTC.now();
    int hour = newTime.hour();
    int minute = newTime.minute();
    TimeSpan sinceStart = newTime - startTime;
    digifiz_parameters.duration = sinceStart.totalseconds()/60;
    switch(digifiz_parameters.mfaState)
    {
        case MFA_STATE_TRIP_DURATION:
            
            setMFAClockData(sinceStart.hours(),sinceStart.minutes());
            break;
        case MFA_STATE_TRIP_DISTANCE:
            //digifiz_parameters.daily_mileage = 0;
            setMFADisplayedNumber(digifiz_parameters.daily_mileage);
            setFloatDot(false);
            break;
        case MFA_STATE_TRIP_L100KM:
            setMFADisplayedNumber(digifiz_parameters.averageConsumption*100);
            setFloatDot(true);
            break;
        case MFA_STATE_TRIP_MEAN_SPEED:
            setMFADisplayedNumber((uint16_t)fabs(digifiz_parameters.averageSpeed*10));
            setFloatDot(true);
            break;
        case MFA_STATE_OIL_TEMP:
            setMFADisplayedNumber(getRawOilTemperature()*100);
            setFloatDot(true);
            break;
        case MFA_STATE_AIR_TEMP:
            setMFADisplayedNumber(getRawAmbientTemperature()*100);
            setFloatDot(true);
            break;
        default:
            break;
    }
}

void setMFAType(uint8_t type)
{
  if (type>6)
  {
      mx.setColumn(0,0);
      mx.setColumn(1,0);
      return;
  }
  uint8_t mfa1_led[6]={0b00000000,0b00000000,0b00001110,0b00110000,0b11000000,0b00000001};
  uint8_t mfa2_led[6]={0b00001000,0b00000100,0b00000000,0b00000000,0b00000000,0b00000010};
  mx.setColumn(0, mfa1_led[type]);
  mx.setColumn(1, mfa2_led[type]);
}

void setBrightness(uint8_t levels)
{
    mx.control(MD_MAX72XX::INTENSITY, constrain(levels,0,0xF));
    mx2.control(MD_MAX72XX::INTENSITY, constrain(levels,0,0xF));
}

void setMileage(uint32_t mileage)
{
  uint8_t first_number[10]={0b11101110,0b01000011,0b10110110,0b01110110,0b01011010,0b01111100,0b11111100,0b01000111,0b11111111,0b01111111};
  uint8_t number[10]={0b01111110,0b01000011,0b10110110,0b11100110,0b11001010,0b11101100,0b11111100,0b01000111,0b11111111,0b11101111};
  mx2.setColumn(0, first_number[(mileage / 100000) % 10]); //(mileage / 100000) % 10
  mx2.setColumn(1, number[(mileage / 10000) % 10]); 
  mx2.setColumn(2, number[(mileage / 1000) % 10]); 
  mx2.setColumn(3, number[(mileage / 100) % 10]); 
  mx2.setColumn(4, number[(mileage / 10) % 10]); 
  //if (mileage!=0)
    mx2.setColumn(5, number[(mileage / 1) % 10]); 
  //else
  //  mx2.setColumn(5, 0b01111110); 
}

void setClockData(uint8_t hours,uint8_t minutes)
{
    uint8_t number[10]={0b01111111,0b01000011,0b10110110,0b11100110,0b11001010,0b11101100,0b11111100,0b01000111,0b11111111,0b11101111};
    if (((hours / 10) % 10)!=0)
    {
    mx2.setColumn(8, number[(hours / 10) % 10]); //X-
    mx2.setColumn(9, number[(hours / 1) % 10]); //-X
    }
    else
    {
    mx2.setColumn(8, number[0x00]); //X-
    mx2.setColumn(9, number[(hours / 1) % 10]); //-X
    }

    mx2.setColumn(10, number[(minutes / 10) % 10]); //X-
    mx2.setColumn(11, number[(minutes / 1) % 10]); //-X
}

void setMFAClockData(uint8_t hrs,uint8_t mins)
{
    uint8_t hours = hrs;
    uint8_t minutes = mins;
    if (hours>99)
    {
        hours=99;
        mins=99;
    }
    uint8_t number[10]={0b01111111,0b01000011,0b10110110,0b11100110,0b11001010,0b11101100,0b11111100,0b01000111,0b11111111,0b11101111};
    if (((hours / 10) % 10)!=0)
    {
    mx2.setColumn(12, number[(hours / 10) % 10]); //X-
    mx2.setColumn(13, number[(hours / 1) % 10]); //-X
    }
    else
    {
    mx2.setColumn(12, number[0x00]); //X-
    mx2.setColumn(13, number[(hours / 1) % 10]); //-X
    }

    mx2.setColumn(14, number[(minutes / 10) % 10]); //X-
    mx2.setColumn(15, number[(minutes / 1) % 10]); //-X
}

void setMFADisplayedNumber(uint16_t data)
{   
    uint8_t number[10]={0b01111111,0b01000011,0b10110110,0b11100110,0b11001010,0b11101100,0b11111100,0b01000111,0b11111111,0b11101111};
    if (((data / 1000) % 10)!=0)
    {
    mx2.setColumn(12, number[(data / 1000) % 10]); //X-
    mx2.setColumn(13, number[(data / 100) % 10]); //-X
    }
    else
    {
    mx2.setColumn(12, number[0x00]); //X-
    mx2.setColumn(13, number[(data / 100) % 10]); //-X
    }

    mx2.setColumn(14, number[(data / 10) % 10]); //X-
    mx2.setColumn(15, number[(data / 1) % 10]); //-X
}

void setFuel(uint8_t litres)
{
    uint8_t number[10]={0b01111111,0b01000011,0b10110110,0b11100110,0b11001010,0b11101100,0b11111100,0b01000111,0b11111111,0b11101111};
    if (((litres / 10) % 10)!=0)
    {
    mx2.setColumn(6, number[(litres / 10) % 10]); //X-
    mx2.setColumn(7, number[(litres / 1) % 10]); //-X
    }
    else
    {
    mx2.setColumn(6, number[0x00]); //X-
    mx2.setColumn(7, number[(litres / 1) % 10]); //-X
    }
}

void setRPMData(uint16_t data)
{
    uint8_t number[9]={0b00000000,0b00000010,0b00000110,0b00001110,0b00011110,0b00111110,0b01111110,0b11111110,0b11111111};
    long long leds_lit = data;
    leds_lit*=48;
    leds_lit/=7000;
    //leds_lit=leds_lit;
    int blocks_lit = leds_lit / 8;
    if (blocks_lit>6) 
      blocks_lit=6; 
    //Serial.println(blocks_lit);
    for (uint8_t col=2; col<2+blocks_lit; col++)
    {
    mx.setColumn(col, 0xff);
    }

    //TODO: this is optional and redundant, consider accurate exclusion for 2+blocks_lit
    for (uint8_t col=2+blocks_lit; col<8; col++)
    {
    mx.setColumn(col, 0x00);
    }
    mx.setColumn(2+blocks_lit, number[leds_lit%8+1]);
}

void setSpeedometerData(uint16_t data)
{
    uint8_t number[10]={0b01111111,0b01000011,0b10110110,0b11100110,0b11001010,0b11101100,0b11111100,0b01000111,0b11111111,0b11101111};
    if (((data / 100) % 10)!=0)
    {
    mx2.setColumn(16, number[(data / 100) % 10]); //X--
    mx2.setColumn(17, number[(data / 10) % 10]); //-X-
    mx2.setColumn(18, number[(data / 1) % 10]); //--X
    mx2.setColumn(19, number[(data / 100) % 10]); //Y--
    mx2.setColumn(20, number[(data / 10) % 10]); //-Y-
    mx2.setColumn(21, number[(data / 1) % 10]); //--Y
    }
    else
    {
    if (((data / 10) % 10)!=0)
    {
      mx2.setColumn(16, 0x00); //X--
      mx2.setColumn(17, number[(data / 10) % 10]); //-X-
      mx2.setColumn(18, number[(data / 1) % 10]); //--X
      mx2.setColumn(19, 0x00); //Y--
      mx2.setColumn(20, number[(data / 10) % 10]); //-Y-
      mx2.setColumn(21, number[(data / 1) % 10]); //--Y
    }
    else
    {
      mx2.setColumn(16, 0x00); //X--
      mx2.setColumn(17, 0x00); //-X-
      mx2.setColumn(18, number[(data / 1) % 10]); //--X
      mx2.setColumn(19, 0x00); //Y--
      mx2.setColumn(20, 0x00); //-Y-
      mx2.setColumn(21, number[(data / 1) % 10]); //--Y
    }
    }
}

void setDot(bool value)
{
  //mx.setPoint(8, 0, value);
  //mx.setPoint(8, 1, value);
  mx2.setPoint(0,8,value);
  mx2.setPoint(0,9,value);
  if (digifiz_parameters.mfaState==MFA_STATE_TRIP_DURATION)
  {
    mx2.setPoint(0,12,value);
    mx2.setPoint(0,13,value);
    mx2.setPoint(0,14,value);
  }
}

void setFloatDot(bool value)
{
    mx2.setPoint(0,12,0);
    mx2.setPoint(0,13,0);
    mx2.setPoint(0,14,value);
}

void setCoolantData(uint16_t data)
{
    uint8_t number[5]={0b0000,0b0001,0b0011,0b0111,0b1111};
    //data is from 0..14
    int blocks_lit = data / 4; //DIG22_0...DIG22_4
    blocks_lit = constrain(blocks_lit,0,3);
    for (uint8_t col=8; col<8+blocks_lit; col++)
    {
      mx.setColumn(col, 0xff);
    }
    for (uint8_t col=8+blocks_lit; col<11; col++)
    {
      mx.setColumn(col, 0x00);
    }
    mx.setColumn(8+blocks_lit, number[data%4+1]);
}
