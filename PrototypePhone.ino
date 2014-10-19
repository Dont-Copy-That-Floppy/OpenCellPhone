/******************************************************************
 This is an example for the Adafruit RA8875 Driver board for TFT displays
         http://www.adafruit.com/products/1590
 The RA8875 is a TFT driver for up to 800x480 dotclock'd displays
 It is tested to work with displays in the Adafruit shop. Other displays
 may need timing adjustments && are not guanteed to work.

 Adafruit invests time && resources providing this open
 source code, please support Adafruit && open-source hardware
 by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries.
 BSD license, check license.txt for more information.
 All text above must be included in any redistribution.
 ******************************************************************/

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

//GPRS Parameters
unsigned char buffer[64];                              // buffer array for data receive over serial port
int count = 0;                                         // counter for buffer array

//Display Parameters
#define RA8875_INT 3
#define RA8875_CS 10
#define RA8875_RESET 9

Adafruit_RA8875 tft = Adafruit_RA8875(RA8875_CS, RA8875_RESET);
uint16_t tx, ty;

/*
//AT Comms
int sendSMS(char* number, char* data);
int readSMS(int messageIndex, char *message, int length);
int deleteSMS(int index);
int callUp(char* number);
int answer(char ATA);
*/

//timer storage for button pushes
long timelast = 0;

// String to Concatenatie start with AT command to dial
char charBuf[16];
char numBuf[13];
String bufStr = "ATD";

// Keep track of what number is touched and at what time
int place = 0;
int oldPlace = 0;

void setup() {

  Serial.begin(9600);                                  // Initialize native Arduino Serial ports (USB Bridge)
  Serial1.begin(9600);                                 // Initialize serial ports to GPRS shield
  Serial.println("RA8875 start");

  // Initialise the display
  if (!tft.begin(RA8875_480x272)) {
    Serial.println("RA8875 Not Found!");
    while (1);
  }
  Serial.println("Found RA8875");

  // Turn Backlight on && setup SPI connection
  tft.displayOn(true);
  tft.GPIOX(true);                                     // Enable TFT - display enable tied to GPIOX
  tft.PWM1config(true, RA8875_PWM_CLK_DIV1024);        // PWM output for backlight
  tft.PWM1out(255);

  // initialize touch capabilities
  pinMode(RA8875_INT, INPUT);
  digitalWrite(RA8875_INT, HIGH);
  tft.touchEnable(true);

  // flip vertical && horizontal scan direction
  tft.scanV_flip(true);
  tft.scanH_flip(true);

  // Set conditions for numberpad
  tft.textMode();
  tft.textRotate(true);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);

  // Call Functions
  tft.textEnlarge(3);
  tft.textSetCursor(0, 0);
  tft.textWrite("C");
  tft.textSetCursor(0, 120);
  tft.textWrite("A");
  tft.textSetCursor(0, 240);
  tft.textWrite("H");

  tft.textEnlarge(2);
  // first row 1,2,3
  tft.textSetCursor(150, 35);
  tft.textWrite("1");
  tft.textSetCursor(150, 125);
  tft.textWrite("2");
  tft.textSetCursor(150, 220);
  tft.textWrite("3");
  // second row 4,5,6
  tft.textSetCursor(235, 35);
  tft.textWrite("4");
  tft.textSetCursor(235, 125);
  tft.textWrite("5");
  tft.textSetCursor(235, 220);
  tft.textWrite("6");
  // third row 7,8,9
  tft.textSetCursor(320, 35);
  tft.textWrite("7");
  tft.textSetCursor(320, 125);
  tft.textWrite("8");
  tft.textSetCursor(320, 220);
  tft.textWrite("9");
  // fourth row *,0,#
  tft.textSetCursor(400, 35);
  tft.textWrite("*");
  tft.textSetCursor(400, 125);
  tft.textWrite("0");
  tft.textSetCursor(400, 220);
  tft.textWrite("#");

}

void loop()
{
  // create buffer to display back to USB
  while (Serial1.available()) {           // reading data into char array
    buffer[count++] = Serial1.read();     // writing data into array
    if (count == 64)
      break;
  }
  Serial.write(buffer, count);           // if no data transmission ends, write buffer to hardware serial port
  clearBufferArray();                    // call clearBufferArray function to clear the stored data from the array
  count = 0;                             // set counter of while loop to zero

  // Write data from USB to GPRS Shield
  if (Serial.available())                                   // when data comes from USB
    Serial1.write(Serial.read());                           // write it to the GPRS shield
/*
  if (Serial.Read() == "Call Ready") {
    tft.textMode();
    tft.textRotate(true);
    tft.textColor(RA8875_WHITE, RA8875_BLACK);
    tft.textEnlarge(2);
    tft.textSetCursor(75, 40);
    tft.textWrite("Ready!");
    tft.graphicsMode();
    tft.fillRect(75, 40, 75, 200, RA8875_BLACK);
  }
*/

  //Touch Capabilities
  tft.graphicsMode();                                       // must be enabled first to receive touch values
  float xScale = 1024.0F / tft.width();
  float yScale = 1024.0F / tft.height();
  if (!digitalRead(RA8875_INT))
  {
    if (tft.touched())
    {
      tft.touchRead(&tx, &ty);
      if (millis() >= (timelast + uint8_t(250))) {

        // Initiate Call
        if (tx > 800 && tx < 1024 && ty > 0 && ty < 200) {
          delay(50);
          if (bufStr.length() == 13) {
            bufStr += ";\r\n";
            bufStr.toCharArray(charBuf, 16);
            Serial1.write(charBuf);
            // reset string
            bufStr = "ATD";
            // clear onscreen display
            tft.graphicsMode();
            tft.fillRect(75, 40, 75, 200, RA8875_BLACK);
            place = 0;
          }
          delay(50);
        }
        // Answer Phone
        if (tx > 800 && tx < 1024 && ty > 400 && ty < 600) {
          delay(50);
          Serial1.write("ATA\r\n");
          bufStr = "ATD";
          // clear onscreen display
          tft.graphicsMode();
          tft.fillRect(75, 40, 75, 200, RA8875_BLACK);
          place = 0;
          delay(50);
        }
        // Hang Up
        if (tx > 800 && tx < 1024 && ty > 800 && ty < 1024) {
          delay(50);
          Serial1.write("ATH\r\n");
          bufStr = "ATD";
          // clear onscreen display
          tft.graphicsMode();
          tft.fillRect(75, 40, 75, 200, RA8875_BLACK);
          place = 0;
          delay(50);
        }
        if (tx > 605 && tx < 675 && ty > 185 && ty < 305) {
          delay(50);
          bufStr += "1";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("1");
          delay(50);
        }
        if (tx > 427 && tx < 502 && ty > 185 && ty < 305) {
          delay(50);
          bufStr += + "4";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("4");
          delay(50);
        }
        if (tx > 262 && tx < 336 && ty > 185 && ty < 305) {
          delay(50);
          bufStr += "7";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("7");
          delay(50);
        }
        if (tx > 108 && tx < 180 && ty > 185 && ty < 305) {
          delay(50);
          bufStr += "*";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("*");
          delay(50);
        }
        if (tx > 605 && tx < 675 && ty > 443 && ty < 577) {
          delay(50);
          bufStr += "2";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("2");
          delay(50);
        }
        if (tx > 427 && tx < 502 && ty > 443 && ty < 577) {
          delay(50);
          bufStr += "5";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("5");
          delay(50);
        }
        if (tx > 262 && tx < 336 && ty > 443 && ty < 577) {
          delay(50);
          bufStr += "8";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("8");
          delay(50);
        }
        if (tx > 108 && tx < 180 && ty > 443 && ty < 577) {
          delay(50);
          bufStr += "0";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("0");
          delay(50);
        }
        if (tx > 605 && tx < 675 && ty > 732 && ty < 832) {
          delay(50);
          bufStr += "3";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("3");
          delay(50);
        }
        if (tx > 427 && tx < 502 && ty > 732 && ty < 832) {
          delay(50);
          bufStr += "6";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("6");
          delay(50);
        }
        if (tx > 262 && tx < 336 && ty > 732 && ty < 832) {
          delay(50);
          bufStr += "9";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("9");
          delay(50);
        }
        if (tx > 108 && tx < 180 && ty > 732 && ty < 832) {
          delay(50);
          bufStr += "#";
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          switch (place) {
            case 1:
              tft.textSetCursor(75, 40);
              break;
            case 2:
              tft.textSetCursor(75, 60);
              break;
            case 3:
              tft.textSetCursor(75, 80);
              break;
            case 4:
              tft.textSetCursor(75, 100);
              break;
            case 5:
              tft.textSetCursor(75, 120);
              break;
            case 6:
              tft.textSetCursor(75, 140);
              break;
            case 7:
              tft.textSetCursor(75, 160);
              break;
            case 8:
              tft.textSetCursor(75, 180);
              break;
            case 9:
              tft.textSetCursor(75, 200);
              break;
            case 10:
              tft.textSetCursor(75, 220);
              break;
          }
          tft.textWrite("#");
          delay(50);
        }
        timelast = millis();
      }
    }
    if (bufStr.length() > 13) {
      bufStr = "ATD";
      place = 0;
      // clear onscreen display
      tft.graphicsMode();
      tft.fillRect(75, 40, 75, 200, RA8875_BLACK);
    }
  }
}

void clearBufferArray() {               // function to clear buffer array
  for (int i = 0; i < count; i++) {
    buffer[i] = NULL;                   // clear all index of array with command NULL
  }
}
