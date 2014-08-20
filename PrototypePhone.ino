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
// Display can only display pixels to 260x 460y don't know why
// Pixel definition is (Y bottom line start, X left starting line, Y top line start, X right ending lines)

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

//GPRS Parameters
unsigned char buffer[64]; // buffer array for data receive over serial port
int count=0;     // counter for buffer array

//Display Parameters MAX 260X 460Y don't know why
#define RA8875_INT 3
#define RA8875_CS 52
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



void setup()
{
  Serial.begin(19200);           // the Serial port of Arduino baud rate
  Serial1.begin(19200);          // the GPRS baud rate
  Serial.println("RA8875 start");

  /* Initialise the display using 'RA8875_480x272' or 'RA8875_800x480' */
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
  
  /*test display
  tft.fillScreen(RA8875_WHITE);
  delay(500);
  tft.fillScreen(RA8875_RED);
  delay(500);
  tft.fillScreen(RA8875_YELLOW);
  delay(500);
  tft.fillScreen(RA8875_GREEN);
  delay(500);
  tft.fillScreen(RA8875_CYAN);
  delay(500);
  tft.fillScreen(RA8875_MAGENTA);
  delay(500);
  tft.fillScreen(RA8875_BLACK);*/
  
  /*draw number pad for testing
  tft.fillRect(10, 10, 300, 250, RA8875_GREEN); // Y bottom line, X left line, Y top line, X right line
  tft.fillRect(10, 10, 200, 250, RA8875_YELLOW);
  tft.fillRect(10, 10, 100, 250, RA8875_WHITE);
  tft.fillRect(10, 10, 300, 170, RA8875_WHITE);
  tft.fillRect(10, 10, 200, 170, RA8875_BLUE);
  tft.fillRect(10, 10, 100, 170, RA8875_YELLOW);
  tft.fillRect(10, 10, 300, 85, RA8875_YELLOW); //this should be 90 but for soom reason when displayed 85 is correct
  tft.fillRect(10, 10, 200, 85, RA8875_RED);
  tft.fillRect(10, 10, 100, 85, RA8875_MAGENTA);*/
  
  //initialize touch capabilities
  tft.touchEnable(true);
  pinMode(RA8875_INT, INPUT);
  digitalWrite(RA8875_INT, HIGH);
  
  //draw number pad background
  tft.fillRect(10, 10, 400, 250, RA8875_WHITE);
  tft.drawLine(10, 85, 400, 85, RA8875_BLACK);
  tft.drawLine(10, 170, 400, 170, RA8875_BLACK);
  tft.drawLine(10, 250, 400, 250, RA8875_BLACK);
  
  //draw numbers on number pad
  tft.textMode();
  tft.textRotate(true);
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(50, 50);
  tft.textWrite("1");
  tft.textSetCursor(75, 75);
  tft.textWrite("4");
  tft.textSetCursor(25, 25);
  tft.textWrite("7");
  tft.textSetCursor(120, 120);
  tft.textWrite("*");
}
 
void loop()
{
  //GPRS
  if (Serial1.available())              // if date is coming from RX1 port ==> data is coming from GPRS shield
  {
    while(Serial1.available())          // reading data into char array 
    {
      buffer[count++]=Serial1.read();     // writing data into array
      if(count == 64)break;
  }
    Serial.write(buffer,count);            // if no data transmission ends, write buffer to hardware serial port
    clearBufferArray();              // call clearBufferArray function to clear the stored data from the array
    count = 0;                       // set counter of while loop to zero
  }
  if (Serial.available())            // if data is available on RX0 port ==> data is coming from USB
    Serial1.write(Serial.read());       // write it to the GPRS shield
    
  //Display
  float xScale = 1024.0F/tft.width();
  float yScale = 1024.0F/tft.height();

  /* Wait around for touch events */
  if (! digitalRead(RA8875_INT)) 
  {
    if (tft.touched()) 
    {
      Serial.print("Touch: "); 
      tft.touchRead(&tx, &ty);
      Serial.print(tx); Serial.print(", "); Serial.println(ty);
      /* Draw a circle */
      tft.fillCircle((uint16_t)(tx/xScale), (uint16_t)(ty/yScale), 4, RA8875_WHITE);
    } 
  }
}
void clearBufferArray()              // function to clear buffer array
{
  for (int i=0; i<count;i++)
    { buffer[i]=NULL;}                  // clear all index of array with command NULL
}
