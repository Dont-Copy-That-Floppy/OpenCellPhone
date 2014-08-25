/******************************************************************
 This is an example for the Adafruit RA8875 Driver board for TFT displays
 ---------------> http://www.adafruit.com/products/1590
 The RA8875 is a TFT driver for up to 800x480 dotclock'd displays
 It is tested to work with displays in the Adafruit shop. Other displays
 may need timing adjustments and are not guanteed to work.

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source hardware
 by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries.
 BSD license, check license.txt for more information.
 All text above must be included in any redistribution.
 ******************************************************************/

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

//GPRS Parameters
unsigned char buffer[64]; // buffer array for data receive over serial port
int count = 0;   // counter for buffer array

//Display Parameters
#define RA8875_INT 3
#define RA8875_CS 10
#define RA8875_RESET 9

Adafruit_RA8875 tft = Adafruit_RA8875(RA8875_CS, RA8875_RESET);
uint16_t tx, ty;

//AT Commands
int sendSMS(char* number, char* data);
int readSMS(int messageIndex, char *message, int length);
int deleteSMS(int index);
int callUp(char* number);
int answer(char ATA);

//numberpad

//timer storage for button pushes
long timelast = 0;

void setup()
{
  Serial.begin(9600);           // the Serial port of Arduino baud rate
  Serial1.begin(9600);          // the GPRS baud rate
  Serial.println("RA8875 start");

  // Initialise the display using 'RA8875_480x272' or 'RA8875_800x480'
  if (!tft.begin(RA8875_480x272))
  {
    Serial.println("RA8875 Not Found!");
    while (1);
  }
  Serial.println("Found RA8875");
  tft.displayOn(true);
  tft.GPIOX(true);      // Enable TFT - display enable tied to GPIOX
  tft.PWM1config(true, RA8875_PWM_CLK_DIV1024); // PWM output for backlight
  tft.PWM1out(255);

  //initialize touch capabilities
  pinMode(RA8875_INT, INPUT);
  digitalWrite(RA8875_INT, HIGH);
  tft.touchEnable(true);

  //flip vertical and horizontal scan direction
  tft.scanV_flip(true);
  tft.scanH_flip(true);

  //draw numbers on number pad
  tft.textMode();
  tft.textRotate(true);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  //first row 1,2,3
  tft.textSetCursor(150, 35);
  tft.textWrite("1");
  tft.textSetCursor(150, 125);
  tft.textWrite("2");
  tft.textSetCursor(150, 220);
  tft.textWrite("3");
  //second row 4,5,6
  tft.textSetCursor(235, 35);
  tft.textWrite("4");
  tft.textSetCursor(235, 125);
  tft.textWrite("5");
  tft.textSetCursor(235, 220);
  tft.textWrite("6");
  //third row 7,8,9
  tft.textSetCursor(320, 35);
  tft.textWrite("7");
  tft.textSetCursor(320, 125);
  tft.textWrite("8");
  tft.textSetCursor(320, 220);
  tft.textWrite("9");
  //fourth row *,0,#
  tft.textSetCursor(400, 35);
  tft.textWrite("*");
  tft.textSetCursor(400, 125);
  tft.textWrite("0");
  tft.textSetCursor(400, 220);
  tft.textWrite("#");
}

void loop()
{
  //GPRS
  if (Serial1.available())              // if date is coming from RX1 port ==> data is coming from GPRS shield
  {
    while (Serial1.available())         // reading data into char array
    {
      buffer[count++] = Serial1.read();   // writing data into array
      if (count == 64)break;
    }
    Serial.write(buffer, count);           // if no data transmission ends, write buffer to hardware serial port
    clearBufferArray();              // call clearBufferArray function to clear the stored data from the array
    count = 0;                       // set counter of while loop to zero
  }
  if (Serial.available())            // if data is available on RX0 port ==> data is coming from USB
    Serial1.write(Serial.read());       // write it to the GPRS shield


  //Touch Capabilities
  tft.graphicsMode();  //must be enable first to receive touch values
  float xScale = 1024.0F / tft.width();
  float yScale = 1024.0F / tft.height();
  if (! digitalRead(RA8875_INT))
  {
    if (tft.touched())
    {
      tft.touchRead(&tx, &ty);
      //Serial.print(tx); Serial.print(", "); Serial.println(ty);
      //centers(boundries)
      //x column = 640(605-675),465(427-502),299(262-336),144(108-180)
      //y column = 245(185-305),510(443-577),782(732-832)
      if (millis() >= (timelast + uint8_t(250)))
      {
        if (tx > 605 and tx < 675 and ty > 185 and ty < 305)
        {
          delay(50);
          Serial.print("1");
          delay(50);
        }
        if (tx > 427 and tx < 502 and ty > 185 and ty < 305)
        {
          delay(50);
          Serial.print("4");
          delay(50);
        }
        if (tx > 262 and tx < 336 and ty > 185 and ty < 305)
        {
          delay(50);
          Serial.print("7");
          delay(50);
        }
        if (tx > 108 and tx < 180 and ty > 185 and ty < 305)
        {
          delay(50);
          Serial.print("*");
          delay(50);
        }
        if (tx > 605 and tx < 675 and ty > 443 and ty < 577)
        {
          delay(50);
          Serial.print("2");
          delay(50);
        }
        if (tx > 427 and tx < 502 and ty > 443 and ty < 577)
        {
          delay(50);
          Serial.print("5");
          delay(50);
        }
        if (tx > 262 and tx < 336 and ty > 443 and ty < 577)
        {
          delay(50);
          Serial.print("8");
          delay(50);
        }
        if (tx > 108 and tx < 180 and ty > 443 and ty < 577)
        {
          delay(50);
          Serial.print("0");
          delay(50);
        }
        if (tx > 605 and tx < 675 and ty > 732 and ty < 832)
        {
          delay(50);
          Serial.print("3");
          delay(50);
        }
        if (tx > 427 and tx < 502 and ty > 732 and ty < 832)
        {
          delay(50);
          Serial.print("6");
          delay(50);
        }
        if (tx > 262 and tx < 336 and ty > 732 and ty < 832)
        {
          delay(50);
          Serial.print("9");
          delay(50);
        }
        if (tx > 108 and tx < 180 and ty > 732 and ty < 832)
        {
          delay(50);
          Serial.print("#");
          delay(50);
        }
        timelast = millis();
      }
    }
  }
}
void clearBufferArray()              // function to clear buffer array
{
  for (int i = 0; i < count; i++)
  {
    buffer[i] = NULL; // clear all index of array with command NULL
  }
}
