/*
   -----------------------------------------------------------
                Jeti Altitude Sensor v 1.3
   -----------------------------------------------------------

    Tero Salminen RC-Thoughts.com (c) 2017 www.rc-thoughts.com

  -----------------------------------------------------------

    Simple altitude sensor for Jeti EX telemetry with cheap
    BMP085 or BMP180 breakout-board and Arduino Pro Mini
    
    Thanks for "Max" to Rainer Nottbusch!

  -----------------------------------------------------------
    Shared under MIT-license by Tero Salminen (c) 2017
  -----------------------------------------------------------
*/

#include <EEPROM.h>
#include <stdlib.h>
#include <SoftwareSerialJeti.h>
#include <JETI_EX_SENSOR.h>
#include "Wire.h"
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

#define prog_char  char PROGMEM
#define GETCHAR_TIMEOUT_ms 20

#ifndef JETI_RX
#define JETI_RX 3
#endif

#ifndef JETI_TX
#define JETI_TX 4
#endif

#define ITEMNAME_1 F("Altitude")
#define ITEMTYPE_1 F("m")
#define ITEMVAL_1 &uAltitude

#define ITEMNAME_2 F("Temperature")
#define ITEMTYPE_2 F("\xB0\x43")
#define ITEMVAL_2 &uTemperature

#define ABOUT_1 F(" RCT Jeti Tools")
#define ABOUT_2 F(" Jeti AltMeter")

SoftwareSerial JetiSerial(JETI_RX, JETI_TX);

void JetiUartInit()
{
  JetiSerial.begin(9700);
}

void JetiTransmitByte(unsigned char data, boolean setBit9)
{
  JetiSerial.set9bit = setBit9;
  JetiSerial.write(data);
  JetiSerial.set9bit = 0;
}

unsigned char JetiGetChar(void)
{
  unsigned long time = millis();
  while ( JetiSerial.available()  == 0 )
  {
    if (millis() - time >  GETCHAR_TIMEOUT_ms)
      return 0;
  }
  int read = -1;
  if (JetiSerial.available() > 0 )
  { read = JetiSerial.read();
  }
  long wait = (millis() - time) - GETCHAR_TIMEOUT_ms;
  if (wait > 0)
    delay(wait);
  return read;
}

char * floatToString(char * outstr, float value, int places, int minwidth = 0) {
  int digit;
  float tens = 0.1;
  int tenscount = 0;
  int i;
  float tempfloat = value;
  int c = 0;
  int charcount = 1;
  int extra = 0;
  float d = 0.5;
  if (value < 0)
    d *= -1.0;
  for (i = 0; i < places; i++)
    d /= 10.0;
  tempfloat +=  d;
  if (value < 0)
    tempfloat *= -1.0;
  while ((tens * 10.0) <= tempfloat) {
    tens *= 10.0;
    tenscount += 1;
  }
  if (tenscount > 0)
    charcount += tenscount;
  else
    charcount += 1;
  if (value < 0)
    charcount += 1;
  charcount += 1 + places;
  minwidth += 1;
  if (minwidth > charcount) {
    extra = minwidth - charcount;
    charcount = minwidth;
  }
  if (value < 0)
    outstr[c++] = '-';
  if (tenscount == 0)
    outstr[c++] = '0';
  for (i = 0; i < tenscount; i++) {
    digit = (int) (tempfloat / tens);
    itoa(digit, &outstr[c++], 10);
    tempfloat = tempfloat - ((float)digit * tens);
    tens /= 10.0;
  }
  if (places > 0)
    outstr[c++] = '.';
  for (i = 0; i < places; i++) {
    tempfloat *= 10.0;
    digit = (int) tempfloat;
    itoa(digit, &outstr[c++], 10);
    tempfloat = tempfloat - (float) digit;
  }
  if (extra > 0 ) {
    for (int i = 0; i < extra; i++) {
      outstr[c++] = ' ';
    }
  }
  outstr[c++] = '\0';
  return outstr;
}

JETI_Box_class JB;

unsigned char SendFrame()
{
  boolean bit9 = false;
  for (int i = 0 ; i < JB.frameSize ; i++ )
  {
    if (i == 0)
      bit9 = false;
    else if (i == JB.frameSize - 1)
      bit9 = false;
    else if (i == JB.middle_bit9)
      bit9 = false;
    else
      bit9 = true;
    JetiTransmitByte(JB.frame[i], bit9);
  }
}

unsigned char DisplayFrame()
{
  for (int i = 0 ; i < JB.frameSize ; i++ )
  {
  }
}

uint8_t frame[10];
short value = 27;

int startAltitude = 0;
int curPressure = 0;
int curAltitude = 0;
int uLoopCount = 0;
int uAltitude = 0;
int uTemperature = 0;
int uMaxAltitude = 0;
int uMaxTemperature = 0;

const int numReadings = 6;
int readings[numReadings];
int readIndex = 0;
int total = 0;

#define MAX_SCREEN 5
#define MAX_CONFIG 1
#define COND_LES_EQUAL 1
#define COND_MORE_EQUAL 2

void setup()
{
  Serial.begin(9600);
  analogReference(EXTERNAL);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(JETI_RX, OUTPUT);
  JetiUartInit();

  JB.JetiBox(ABOUT_1, ABOUT_2);
  JB.Init(F("RCT"));
  JB.addData(ITEMNAME_1, ITEMTYPE_1);
  JB.addData(ITEMNAME_2, ITEMTYPE_2);
  JB.setValue(1, ITEMVAL_1);
  JB.setValue(2, ITEMVAL_2);

  bmp.begin();
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  
  do {
    JB.createFrame(1);
    SendFrame();
    delay(GETCHAR_TIMEOUT_ms);
  }
  while (sensorFrameName != 0);
  digitalWrite(13, LOW);
}

float alarm_current = 0;

int header = 0;
int lastbtn = 240;
int current_screen = 0;
int current_config = 0;
char temp[LCDMaxPos / 2];
char msg_line1[LCDMaxPos / 2];
char msg_line2[LCDMaxPos / 2];

void process_screens()  
{
  switch (current_screen)
  {
  case 0 : {
        JB.JetiBox(ABOUT_1, ABOUT_2);
        break;
      }
  case 1 : {
        msg_line1[0] = 0; msg_line2[0] = 0;

        strcat_P((char*)&msg_line1, (prog_char*)F("Altitude: "));
        temp[0] = 0;
        floatToString((char*)&temp, uAltitude, 0);
        strcat((char*)&msg_line1, (char*)&temp);        
        strcat_P((char*)&msg_line1, (prog_char*)F("m"));

        strcat_P((char*)&msg_line2, (prog_char*)F("Temp.: "));
        temp[0] = 0;
        floatToString((char*)&temp, uTemperature, 0);
        strcat((char*)&msg_line2, (char*)&temp);
        strcat_P((char*)&msg_line2, (prog_char*)F("\xB0\x43"));

        JB.JetiBox((char*)&msg_line1, (char*)&msg_line2);
        break;
      }
  case 2 : {
        msg_line1[0] = 0; msg_line2[0] = 0;

        strcat_P((char*)&msg_line1, (prog_char*)F("MaxAlt.: "));
        temp[0] = 0;
        floatToString((char*)&temp, uMaxAltitude, 0);
        strcat((char*)&msg_line1, (char*)&temp);        
        strcat_P((char*)&msg_line1, (prog_char*)F("m"));

        strcat_P((char*)&msg_line2, (prog_char*)F("MaxTemp.: "));
        temp[0] = 0;
        floatToString((char*)&temp, uMaxTemperature, 0);
        strcat((char*)&msg_line2, (char*)&temp);
        strcat_P((char*)&msg_line2, (prog_char*)F("\xB0\x43"));

        JB.JetiBox((char*)&msg_line1, (char*)&msg_line2);
        break;
      }
  case 3 : {
        msg_line1[0] = 0; msg_line2[0] = 0;
        strcat_P((char*)&msg_line1, (prog_char*)F("Alt & Max Reset"));
        strcat_P((char*)&msg_line2, (prog_char*)F("Press DOWN"));
        JB.JetiBox((char*)&msg_line1, (char*)&msg_line2);
        break;
      }
  case 99 : {
        msg_line1[0] = 0; msg_line2[0] = 0;
        strcat_P((char*)&msg_line1, (prog_char*)F("Altitude reset!"));
        strcat_P((char*)&msg_line2, (prog_char*)F("Press < to exit"));
        JB.JetiBox((char*)&msg_line1, (char*)&msg_line2);
        break;
      }
  case MAX_SCREEN : {
        JB.JetiBox(ABOUT_1, ABOUT_2);
        break;
      }
  }
}

void loop()
{ 
  curAltitude = bmp.readAltitude();
  uTemperature = bmp.readTemperature();

  if (uTemperature > uMaxTemperature) {
    uMaxTemperature = uTemperature;
  }

  if (uLoopCount == 20) {
    startAltitude = curAltitude;
  }

  total = total - readings[readIndex];
  readings[readIndex] = curAltitude;
  total = total + readings[readIndex];
  readIndex = readIndex + 1;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  uAltitude = (total / numReadings) - startAltitude;

  if (uLoopCount < 21) {
    uLoopCount++;
    uAltitude = 0;
  }
  
  if (uAltitude > uMaxAltitude and uLoopCount > 20) {
    uMaxAltitude = uAltitude;
  }

  /*Serial.print("Altitude = "); // Uncomment these for PC debug
  Serial.print(uAltitude);
  Serial.println(" meters");
  Serial.println();

  Serial.print("Temperature = "); // Uncomment these for PC debug
  Serial.print(uTemperature);
  Serial.println("*C");
  Serial.println();*/

  unsigned long time = millis();
  SendFrame();
  time = millis();
  int read = 0;
  pinMode(JETI_RX, INPUT);
  pinMode(JETI_TX, INPUT_PULLUP);

  JetiSerial.listen();
  JetiSerial.flush();

  while ( JetiSerial.available()  == 0 )
  {

    if (millis() - time >  5)
      break;
  }

  if (JetiSerial.available() > 0 )
  { read = JetiSerial.read();

    if (lastbtn != read)
    {
      lastbtn = read;
      switch (read)
      {
        case 224 : // RIGHT
          if (current_screen != MAX_SCREEN)
          {
            current_screen++;
            if (current_screen == 4) current_screen = 0;
          }
          break;
        case 112 : // LEFT
          if (current_screen != MAX_SCREEN)
            if (current_screen == 99) current_screen = 1;
            else
            {
              current_screen--;
              if (current_screen > MAX_SCREEN) current_screen = 0;
              }
          break;
        case 208 : // UP
          break;
        case 176 : // DOWN
          if (current_screen == 3) {
            startAltitude = curAltitude;
            uMaxAltitude = 0;
            uMaxTemperature = 0; 
            current_screen = 99;
          }
          break;
        case 144 : // UP+DOWN
          break;
        case 96 : // LEFT+RIGHT
          break;
      }
    }
  }

  if (current_screen != MAX_SCREEN)
    current_config = 0;
  process_screens();
  header++;
  if (header >= 5)
  {
    JB.createFrame(1);
    header = 0;
  }
  else
  {
    JB.createFrame(0);
  }

  long wait = GETCHAR_TIMEOUT_ms;
  long milli = millis() - time;
  if (milli > wait)
    wait = 0;
  else
    wait = wait - milli;
  pinMode(JETI_TX, OUTPUT);
}
