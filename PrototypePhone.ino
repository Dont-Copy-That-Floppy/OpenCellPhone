/******************************************************************************************************

                Coded and written by Pusalieth aka Jake Pring
                          Licensed under the GPLv2

    This code running on an Arduino combined with the GPRS shield from Seeedstudio, and the
  4.3" tft touch screen from Adafrit, using the RA8875 driver from Adafruit, it will produce
  a functioning phone

  CURRENT FEATURES:
  =================
  Place calls, receive calls, hang up calls, ignore calls
  Dialing numbers and displayed on-screen
  In call DTMF Tones
  Read incoming data from SIM Module, and provide functions and executions based on data
  Display prompt for incoming texts
  Display Time in a human readable format including AM/PM and updated at 00 seconds
  Seperate and Defined Screens with functions and touch values per screen (ie. Phone vs SMS)
  Read individual texts defined/sorted by index
  Delete SMS
  Caller ID
  Display Missed Calls
  Passcode along with 5 second lapse before start
  Notification LEDs
  Display has Timeout sleep based on Touch Inactivity
  Automatic Power On for SIM Module

*******************************************************************************************************/






//==================================================================================================================================================

//   DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================


#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

// Define pins used for RA8875 driver
#define RA8875_INT 5
#define RA8875_RESET 6
#define RA8875_CS 9

// mew object tft to use RA8875
Adafruit_RA8875 tft = Adafruit_RA8875(RA8875_CS, RA8875_RESET);

// Initialize touch screen place values
uint16_t tx = 0, ty = 0;

// Keep track of time since Last Touched event
unsigned long int lastInteract = 0;

// Call Buffer
char callCharBuf[20] = {'A', 'T', 'D'};

// Place value of number dialed sequence
int place = 3;

// Text Messaging array, store from bufGPRS, start and finish of text message read from GPRS
char sendCharText[11] = {'A', 'T', '+', 'C', 'M', 'G', 'R', '=', '0', '1', '\r'};
String textBuf = "";
boolean viewMessage = false;
boolean textReadStarted = false;
boolean textReady = false;
boolean newMessage;

// resultStr defines the functions of the phone
String resultStr = "";
boolean resultStrChange = false;

// Store incoming data from GPRS shield
String bufGPRS = "";

// Keep track of whether in call or not
boolean inCall = false;

// Keep track of missed Calls and new Messages
int countRings = 0;
char missedCalls = '0';
char newMessages = '0';

// Keep track of time elapsed since last query for time (defined per 60 seconds)
unsigned long int next60 = 60;
unsigned long int every60 = 0;
boolean displayTime = false;

// Define where on the phone the User is and what part of the phone functions to jump to
boolean atHomeScreen = false;
boolean atPhoneScreen = false;
boolean atTextMessageScreen = false;
boolean atAnswerScreen = false;
boolean atAnsweredScreen = false;
boolean atPasscodeScreen = true;
boolean atSendTextScreen = false;

// Display State
boolean displaySleep = false;
unsigned int displayTimeout = 45 * 1000;

// realPasscode is your passcode, displays 8 characters correctly but technically can be as long as desired
String realPasscode = "1234";
String passcode = "";
int passcodePlace = 0;
unsigned long int startPasscodeLock = 0;
unsigned int passcodeTimeout = 10 * 1000;

// Notification LEDs
int missedCallsLED = 10;
int messageLED = 11;

// Power Button
int powerButton = 13;

// Delay for write on Serial1 baud, bitDepth is CPU/MCU/MPU bits
double bitDepth = 8;
double buadRateGPRS = 19200;
unsigned int delayForUart = (buadRateGPRS / (bitDepth + 2.0));

// Keep track of in call time
unsigned int prevCallTime = 0;
unsigned int callTime = 0;

//==================================================================================================================================================

//   END OF DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================









void setup() {
  //==================================================================================================================================================

  //   MAIN SETUP

  //==================================================================================================================================================

  // Power on the Sim Module
  pinMode(26, INPUT);
  pinMode(3, OUTPUT);
  boolean powerState = false;
  powerState = digitalRead(26);
  if (!powerState) {
    digitalWrite(3, HIGH);
    delay(3000);
    digitalWrite(3, LOW);
  }

  // Start serial from Arduino to USB
  Serial.begin(19200);

  // Start serial from GPRS to Arduino
  Serial1.begin(buadRateGPRS);

  // Initialize the RA8875 driver
  Serial.println("RA8875 start");
  while (!tft.begin(RA8875_480x272)) {
    Serial.println("RA8875 Not Found!");
  }
  Serial.println("Found RA8875");

  // Turn display backlight and PWM output from driver
  tft.displayOn(true);
  tft.GPIOX(true);
  tft.PWM1config(true, RA8875_PWM_CLK_DIV1024);
  tft.PWM1out(255);

  // Set pin for touch resistance of tft display
  pinMode(RA8875_INT, INPUT_PULLUP);
  digitalWrite(RA8875_INT, HIGH);
  tft.touchEnable(true);
  delay(20);

  // Flip the horizontal scanning and text
  tft.scanH_flip(true);
  tft.textRotate(true);

  // Set Screen
  atPasscodeScreen = true;
  drawPasscodeScreen();

  // Update time from network
  Serial1.write("AT+CLTS=1\r");
  delay((10 / delayForUart) * 1000);

  // Make sure Caller ID is on
  Serial1.write("AT+CLIP=1\r");
  delay((10 / delayForUart) * 1000);

  // Enable Sleep Mode on SIM900 (wake-up on serial)
  //Serial1.write("AT+CSCLK=2\r");
  //delay((11 / delayForUart) * 1000);

  // If Sim Module is currently powered on, check time
  if (powerState) {
    Serial1.write("AT+CCLK?\r");
    delay((11 / delayForUart) * 1000);
  }

  lastInteract = millis();
  startPasscodeLock = millis();

  pinMode(powerButton, INPUT_PULLUP);
  digitalWrite(powerButton, HIGH);
}
//==================================================================================================================================================

//   END OF SETUP

//==================================================================================================================================================









void loop() {
  //==================================================================================================================================================

  //   MAIN PROGRAM

  //==================================================================================================================================================


  //=========================
  //      SERIAL READS
  //=========================
  // When data is coming from GPRS store into bufGPRS
  while (Serial1.available()) {
    bufGPRS += (char)Serial1.read();
  }

  //When data is coming from USB write to GPRS shield
  while (Serial.available()) {
    Serial1.write(Serial.read());
  }


  //=========================
  //     GPRS FUNCTIONS
  //=========================
  if (bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n") {
    resultStrChange = true;
    while (bufGPRS.substring(0, 2) == "\r\n")
      bufGPRS = bufGPRS.substring(2, bufGPRS.length());
    if (bufGPRS.substring(0, 6) == "+CCLK:") {
      displayTime = true;
      every60 = millis() / 1000;
    }
    else if (bufGPRS.substring(0, 6) == "+CLIP:")
      resultStr = "Caller ID";
    else if (bufGPRS == "RING\r\n") {
      resultStr = "Ringing";
      atAnswerScreen = true;
      atHomeScreen = false;
      atPhoneScreen = false;
      atTextMessageScreen = false;
    }
    else if (bufGPRS.substring(0, 6) == "+CMTI:")
      newMessage = true;
    else if (bufGPRS.substring(0, 6) == "+CMGR:")
      resultStr = "Text Read";
    else if (bufGPRS == "OK\r\n")
      resultStr = "OK";
    else if (bufGPRS == "NO CARRIER\r\n")
      resultStr = "No Carrier";
    else if (bufGPRS == "Call Ready\r\n")
      resultStr = "Call Ready";
    else if (bufGPRS == "NORMAL POWER DOWN\r\n")
      resultStr = "NORMAL POWER DOWN";
  }


  //===========================================================================================================================

  //  Defined functions per screen

  //===========================================================================================================================


  //================================================================================================
  //   HOME SCREEN
  //================================================================================================
  if (atHomeScreen) {

    //==========================================================
    //   MISSED CALLS PROMPT
    //==========================================================
    if (missedCalls > '0') {
      tft.textMode();
      tft.textRotate(true);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(270, 15);
      tft.textWrite("Missed Calls ");
      tft.writeData(missedCalls);
    }

    //==========================================================
    //   NEW MESSAGE PROMPT
    //==========================================================
    if (newMessages > '0') {

      // clear number display and number pad
      tft.graphicsMode();
      tft.fillRect(160, 0, 100, 279, RA8875_BLACK);

      // Prompt User new message received
      tft.textMode();
      tft.textRotate(true);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(160, 20);
      tft.textWrite("New Message ");
      tft.writeCommand(RA8875_MRWC);
      tft.writeData(newMessages);
      delay(1);

      //==========================================================
      //   VIEW MESSAGE PROMPT
      //==========================================================
      tft.textMode();
      tft.textRotate(true);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(210, 40);
      tft.textWrite("View Message");
      viewMessage = true;
    }
  }

  //================================================================================================
  //   TEXT MESSAGE SCREEN
  //================================================================================================
  else if (atTextMessageScreen) {

    //==========================================================
    //      VIEW MESSAGES
    //==========================================================
    if (textReady) {
      char textCharRead[textBuf.length()];
      textBuf.toCharArray(textCharRead, textBuf.length());

      // Prepare to display message
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);

      // Display Index Number
      tft.textSetCursor(45, 110);
      tft.writeCommand(RA8875_MRWC);
      for (int i = 8; i <= 9; i++) {
        tft.writeData(sendCharText[i]);
        delay(1);
      }

      // Define the length number to display on-screen
      int receivingNumStart = textBuf.indexOf(',');
      if (receivingNumStart + 2 == '+') {
        receivingNumStart += 3;
      } else {
        receivingNumStart += 2;
      }
      int receivingNumEnd = (textBuf.indexOf(',', receivingNumStart + 1));
      receivingNumEnd -= 1;

      // Display new message receiving number
      tft.textSetCursor(130, 20);
      tft.writeCommand(RA8875_MRWC);
      for (int i = receivingNumStart; i < receivingNumEnd; i++) {
        tft.writeData(textCharRead[i]);
        delay(1);
      }

      // Define the length of message to display on-screen
      int messageStart = textBuf.indexOf('\r');
      int messageEnd = (textBuf.indexOf('\r', messageStart + 1));

      // Display Message Body
      tft.textSetCursor(175, 0);
      tft.textEnlarge(0);
      delay(2);
      tft.writeCommand(RA8875_MRWC);
      for (int i = messageStart + 2; i < messageEnd; i++) {
        tft.writeData(textCharRead[i]);
        delay(1);
      }
      textReady = false;
      textBuf = "";
    }
  }

  //================================================================================================
  //   ANSWER SCREEN
  //================================================================================================
  else if (atAnswerScreen && resultStrChange) {

    //==========================================================
    //  DISPLAY INCOMING CALL
    //==========================================================
    if (resultStr == "Ringing") {
      resultStrChange = false;
      //=====================
      //  WAKE UP SCREEN
      //=====================
      if (countRings == 0) {
        tft.displayOn(true);
        tft.PWM1out(255);
        tft.touchEnable(true);
        displaySleep = false;
        drawAnswerScreen();
      }
      countRings++;
      lastInteract = millis();
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(75, 20);
      tft.textWrite("Incoming Call");
    }

    //==========================================================
    //    DISPLAY CALLER ID
    //==========================================================
    else if (resultStr == "Caller ID") {
      resultStrChange = false;
      // Determine the start and finish of body for Caller id number
      int callerID_Start = bufGPRS.indexOf('"');
      int callerID_End = bufGPRS.indexOf('"', callerID_Start + 1);

      // store data and input to array
      char callerID[bufGPRS.length()];
      bufGPRS.toCharArray(callerID, bufGPRS.length());

      // Display number on screen
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(125, 20);
      tft.writeCommand(RA8875_MRWC);
      for (int i = callerID_Start + 1; i < callerID_End; i++) {
        tft.writeData(callerID[i]);
        delay(1);
      }
    }
  }

  //==========================================================
  //   CALL LOST (Service drop or call terminated)
  //==========================================================
  if (resultStr == "No Carrier" && resultStrChange) {
    resultStrChange = false;
    inCall = false;
    clearScreen();
    tft.textMode();
    tft.textColor(RA8875_WHITE, RA8875_BLACK);
    tft.textEnlarge(2);
    delay(2);
    tft.textSetCursor(100, 15);
    tft.textWrite("Call Ended");
    delay(2000);
    if (atPasscodeScreen) {
      atHomeScreen = false;
      atPhoneScreen = false;
      atTextMessageScreen = false;
      atAnswerScreen = false;
      atAnsweredScreen = false;
      drawPasscodeScreen();
    } else {
      drawHomeScreen();
      atHomeScreen = true;
      atPhoneScreen = false;
      atTextMessageScreen = false;
      atAnsweredScreen = false;
      atAnswerScreen = false;
    }
  }

  //==========================================================
  //      CALL READY
  //==========================================================
  if (resultStr == "Call Ready" && resultStrChange) {
    resultStrChange = false;
    lastInteract = millis();
    startPasscodeLock = millis();
    drawPasscodeScreen();
    atHomeScreen = false;
    atPhoneScreen = false;
    atTextMessageScreen = false;
    atAnswerScreen = false;
    atPasscodeScreen = true;
    Serial1.write("AT+CCLK?\r");
    delay((9 / delayForUart) * 1000);
  }

  /*
      //==========================================================
      //       SIM900 OFF
      //==========================================================
      if (resultStr == "NORMAL POWER DOWN" && resultStrChange) {

        // Display to User GPRS shield is off
        tft.textMode();
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(2);
        delay(2);
        tft.textSetCursor(250, 50);
        tft.textWrite("SIM900m");
        tft.textSetCursor(300, 50);
        tft.textWrite("POWERED");
        tft.textSetCursor(350, 50);
        tft.textWrite("  OFF");
      }
    }
    */
  //===========================================================================================================================

  //  END OF SCREEN FUNCTIONS

  //===========================================================================================================================





  //===========================================================================================================================

  //  BACKGROUND TASKS

  //===========================================================================================================================

  //==========================================================
  //   MISSED/NEW MESSAGES COUNT
  //==========================================================
  if (newMessage) {
    newMessages++;
    //=========================
    //    NEW MESSAGE INDEX
    //=========================
    if (bufGPRS.indexOf(',') + 2 == '\r') {
      sendCharText[9] = bufGPRS.charAt(bufGPRS.indexOf(',') + 1);
    } else {
      sendCharText[8] = bufGPRS.charAt(bufGPRS.indexOf(',') + 1);
      sendCharText[9] = bufGPRS.charAt(bufGPRS.indexOf(',') + 2);
    }
    newMessage = false;
  }

  //==========================================================
  //   MISSED CALLS COUNT
  //==========================================================
  if (countRings > 0 && resultStr == "No Carrier" && resultStrChange) {
    missedCalls++;
    countRings = 0;
  }

  //==========================================================
  //    NOTIFICATION LEDS
  //==========================================================
  if (missedCalls >= '1') {
    missedCalls = '0';
    analogWrite(missedCallsLED, 255);
    tft.textMode();
    tft.textColor(RA8875_RED, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(2);
    tft.textSetCursor(0, 0);
    tft.textWrite("!");
  }
  if (newMessages >= '1') {
    newMessages = '0';
    analogWrite(messageLED, 255);
    tft.textMode();
    tft.textColor(RA8875_YELLOW, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(2);
    tft.textSetCursor(0, 20);
    tft.textWrite("!");
  }

  //==========================================================
  //  DISPLAY POWER BUTTON
  //==========================================================
  if (!digitalRead(powerButton) && millis() >= (lastInteract + 500)) {
    if (displaySleep) {
      tft.PWM1out(255);
      delay(20);
      tft.displayOn(true);
      tft.touchEnable(true);
      delay(20);
      displaySleep = false;
      lastInteract = millis();
    } else {
      drawPasscodeScreen();
      lastInteract = millis();
      tft.PWM1out(0);
      delay(20);
      tft.displayOn(false);
      tft.touchEnable(false);
      delay(20);
      displaySleep = true;
      atHomeScreen = false;
      atPhoneScreen = false;
      atTextMessageScreen = false;
      atAnswerScreen = false;
      atPasscodeScreen = true;
    }
    return;
  }

  //==========================================================
  //  DISPLAY TIMEOUT SLEEP
  //==========================================================
  if (!displaySleep && millis() >= (lastInteract + displayTimeout)) {
    startPasscodeLock = millis();
    lastInteract = millis();
    tft.displayOn(false);
    tft.PWM1out(0);
    tft.touchEnable(false);
    displaySleep = true;
    delay(10);
  }

  //==========================================================
  //  PASSCODE LOCK SCREEN
  //==========================================================
  if (!atPasscodeScreen && displaySleep && millis() >= (startPasscodeLock + passcodeTimeout)) {
    tft.scanV_flip(false);
    drawPasscodeScreen();
    atHomeScreen = false;
    atPhoneScreen = false;
    atTextMessageScreen = false;
    atAnswerScreen = false;
    atPasscodeScreen = true;
  }

  //==========================================================
  //      TIME DISPLAY
  //==========================================================
  if (displayTime) {
    printTime();
    displayTime = false;
    char seconds[2];
    seconds[0] = bufGPRS.charAt(bufGPRS.indexOf(':', 22) + 1);
    seconds[1] = bufGPRS.charAt(bufGPRS.indexOf('-') - 1);
    next60 = (seconds[0] - 48) * 10;
    next60 += (seconds[1] - 48);
    next60 = 60 - next60;
  }
  else if (millis() / 1000 >= next60 + every60) {
    every60 = millis() / 1000;
    Serial1.write("AT+CCLK?\r");
    delay((9 / delayForUart) * 1000);
  }

  //==========================================================
  //      IN CALL DURATION
  //==========================================================
  if (inCall) {
    int temp = millis() / 1000;
    if (temp >= prevCallTime + 1) {
      prevCallTime = temp;
      char callTimeChar[5];
      callTimeChar[2] = 10;
      callTime++;
      if (callTime >= 60) {
        callTimeChar[4] = (callTime / 60) / 10;
        callTimeChar[3] = (callTime / 60) % 10;
        callTimeChar[1] = (callTime % 60) / 10;
        callTimeChar[0] = (callTime % 60) % 10;
      }
      else {
        callTimeChar[4] = 0;
        callTimeChar[3] = 0;
        callTimeChar[1] = callTime / 10;
        callTimeChar[0] = callTime % 10;
      }
      tft.graphicsMode();
      tft.fillRect(425, 0, 54, 185, RA8875_BLACK);
      tft.textMode();
      tft.textEnlarge(2);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textSetCursor(425, 15);
      tft.writeCommand(RA8875_MRWC);
      for (int i = 4; i >= 0; i--) {
        tft.writeData(callTimeChar[i] + 48);
        delay(1);
      }
    }
  }
  //===========================================================================================================================

  //  END OF BACKGROUND TASKS

  //===========================================================================================================================







  //===========================================================================================================================

  //  TOUCH FUNCTIONS/AREAS DEFINED PER SCREEN

  //===========================================================================================================================

  // Enable graphics mode for touch input
  tft.graphicsMode();

  // Set scale for touch values
  float xScale = 1024.0F / tft.width();
  float yScale = 1024.0F / tft.height();

  if (!digitalRead(RA8875_INT) && tft.touched()) {
    Serial.print(!digitalRead(RA8875_INT));
    tft.touchRead(&tx, &ty);
    delay(150);
    printTouchValues();
    if (millis() >= (lastInteract + 375)) {
      lastInteract = millis();
      printTouchValues();

      if (!atPasscodeScreen) {
        //================================================================================================
        //  HOME SCREEN TOUCH
        //================================================================================================
        if (atHomeScreen) {

          //=========================
          //      PHONE BUTTON
          //=========================
          if (tx > 128 && tx < 300 && ty > 204 && ty < 440) {
            tx = 0;
            ty = 0;
            missedCalls = '0';
            newMessages = '0';
            resultStr = "";
            drawPhoneScreen();
            atPhoneScreen = true;
            atHomeScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
          }

          //=========================
          //      SMS BUTTON
          //=========================
          else if (tx > 128 && tx < 300 && ty > 580 && ty < 815) {
            tx = 0;
            ty = 0;
            missedCalls = '0';
            newMessages = '0';
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);
            drawTextMessageScreen();
            atTextMessageScreen = true;
            atHomeScreen = false;
            atPhoneScreen = false;
            atAnswerScreen = false;
            viewMessage = false;
            // Prepare to display message
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(1);

            // Display Index Number
            tft.textSetCursor(45, 110);
            tft.writeCommand(RA8875_MRWC);
            for (int i = 8; i <= 9; i++) {
              tft.writeData(sendCharText[i]);
              delay(1);
            }
          }

          //=========================
          //    VIEW NEW MESSAGE
          //=========================
          else if (viewMessage) {
            if (tx > 490 && tx < 576 && ty > 163 && ty < 756) {
              tx = 0;
              ty = 0;
              missedCalls = '0';
              newMessages = '0';
              Serial1.write(sendCharText);
              delay((11 / delayForUart) * 1000);
              drawTextMessageScreen();
              atTextMessageScreen = true;
              atHomeScreen = false;
              atPhoneScreen = false;
              atAnswerScreen = false;
              viewMessage = false;
            }
          }
        }

        //================================================================================================
        //  TEXT MESSAGE SCREEN TOUCH
        //================================================================================================
        else if (atTextMessageScreen) {

          //=========================
          //          SEND
          //=========================
          if (tx > 56 && tx < 159 && ty > 130 && ty < 353) {
            tx = 0;
            ty = 0;
            drawMessagePad();
            atSendTextScreen = true;
            atHomeScreen = false;
            atPhoneScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
          }

          //=========================
          //      HOME BUTTON
          //=========================
          else if (tx > 56 && tx < 159 && ty > 383 && ty < 604) {
            tx = 0;
            ty = 0;
            drawHomeScreen();
            atHomeScreen = true;
            atPhoneScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
          }

          //=========================
          //        DELETE
          //=========================
          else if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
            tx = 0;
            ty = 0;
            sendCharText[6] = 'D';
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);
            sendCharText[6] = 'R';
            drawTextMessageScreen();
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(1);
            delay(2);
            tft.textSetCursor(150, 20);
            tft.textWrite("Message Deleted");
            delay(2000);
            drawTextMessageScreen();
            if (sendCharText[9] == '9') {
              sendCharText[8] += 1;
              sendCharText[9] = '0';
            } else {
              sendCharText[9] += 1;
            }
            //Serial1.write(sendCharText);
            //delay((11 / delayForUart) * 1000);
          }

          //=========================
          //      NEXT BUTTON
          //=========================
          else if (tx > 733 && tx < 853 && ty > 530 && ty < 690) {
            tx = 0;
            ty = 0;
            drawTextMessageScreen();
            if (sendCharText[9] == '9') {
              sendCharText[8] += 1;
              sendCharText[9] = '0';
            } else {
              sendCharText[9] += 1;
            }
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);

            // Prepare to display message
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(1);

            // Display Index Number
            tft.textSetCursor(45, 110);
            tft.writeCommand(RA8875_MRWC);
            for (int i = 8; i <= 9; i++) {
              tft.writeData(sendCharText[i]);
              delay(1);
            }
          }

          //=========================
          //    PREVIOUS BUTTON
          //=========================
          else if (tx > 733 && tx < 853 && ty > 700 && ty < 863) {
            tx = 0;
            ty = 0;
            drawTextMessageScreen();
            if (sendCharText[8] > '0' && sendCharText[9] == '0') {
              sendCharText[8] -= 1;
              sendCharText[9] = '9';
            } else {
              sendCharText[9] -= 1;
            }
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);
            // Prepare to display message
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(1);

            // Display Index Number
            tft.textSetCursor(45, 110);
            tft.writeCommand(RA8875_MRWC);
            for (int i = 8; i <= 9; i++) {
              tft.writeData(sendCharText[i]);
              delay(1);
            }
          }

          //=========================
          //      RESET INDEX
          //=========================
          else if (tx > 846 && tx < 907 && ty > 470 && ty < 592) {
            tx = 0;
            ty = 0;
            sendCharText[8] = '0';
            sendCharText[9] = '1';
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);
          }
        }

        //================================================================================================
        //  SEND TEXT SCREEN TOUCH
        //================================================================================================
        else if (atSendTextScreen) {
          //=========================
          //      SEND BUTTON
          //=========================
          if (tx > 847 && tx < 1024 && ty > 786 && ty < 1024) {
            tx = 0;
            ty = 0;
            tft.scanV_flip(false);
          }
          //=========================
          //      BACK BUTTON
          //=========================
          else if (tx > 0 && tx < 170 && ty > 786 && ty < 1024) {
            tx = 0;
            ty = 0;
            tft.scanV_flip(false);
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);
            fullScreenClear();
            drawTextMessageScreen();
            atTextMessageScreen = true;
            atSendTextScreen = false;
            atHomeScreen = false;
            atAnswerScreen = false;
            atPhoneScreen = false;
          }
        }
      }

      //================================================================================================
      //  ANSWER SCREEN TOUCH
      //================================================================================================
      if (atAnswerScreen) {

        //=========================
        //      ANSWER BUTTON
        //=========================
        if (tx > 119 && tx < 311 && ty > 196 && ty < 456) {
          tx = 0;
          ty = 0;
          Serial1.write("ATA\r");
          delay((4 / delayForUart) * 1000);
          inCall = true;
          tft.fillRect(75, 20, 150, 250, RA8875_BLACK);
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(100, 30);
          tft.textWrite("Answered");
          delay(1000);
          resultStr = "";
          drawAnsweredScreen();
          atHomeScreen = false;
          atPhoneScreen = false;
          atTextMessageScreen = false;
          atAnswerScreen = false;
          countRings = 0;
          callTime = 0;
          prevCallTime = millis() / 1000;
        }

        //=========================
        //      IGNORE BUTTON
        //=========================
        else if (tx > 119 && tx < 311 && ty > 568 && ty < 826) {
          tx = 0;
          ty = 0;
          Serial1.write("AT H\r");
          delay((11 / delayForUart) * 1000);
          inCall = false;
          clearScreen();
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(75, 20);
          tft.textWrite("Ignored");
          delay(1000);
          resultStr = "";
          drawHomeScreen();
          atHomeScreen = true;
          atPhoneScreen = false;
          atTextMessageScreen = false;
          atAnswerScreen = false;
          atAnsweredScreen = false;
          countRings = 0;
        }
      }

      //================================================================================================
      //  PHONE SCREEN TOUCH
      //================================================================================================
      else if (atPhoneScreen) {
        int topRow_xMin = 689;
        int topRow_xMax = 778;
        int secondRow_xMin = 529;
        int secondRow_xMax = 627;
        int thirdRow_xMin = 347;
        int thirdRow_xMax = 442;
        int bottomRow_xMin = 194;
        int bottomRow_xMax = 285;

        int leftColumn_yMin = 190;
        int leftColumn_yMax = 305;
        int middleColumn_yMin = 443;
        int middleColumn_yMax = 577;
        int rightColumn_yMin = 690;
        int rightColumn_yMax = 872;

        //=========================
        //          #1
        //=========================
        if (tx > topRow_xMin && tx < topRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '1';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("1");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=1\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #2
        //=========================
        else if (tx > topRow_xMin && tx < topRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '2';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("2");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=2\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #3
        //=========================
        else if (tx > topRow_xMin && tx < topRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '3';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("3");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=3\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #4
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '4';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("4");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=4\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #5
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '5';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("5");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=5\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #6
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '6';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("6");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=6\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #7
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '7';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("7");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=7\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #8
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '8';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("8");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=8\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #9
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '9';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("9");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=9\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //           *
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '*';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("*");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=*\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #0
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '0';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("0");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=0\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //           #
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '#';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("#");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=#\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        if (!atPasscodeScreen) {
          //=========================
          //          CALL
          //=========================
          if (tx > 56 && tx < 159 && ty > 130 && ty < 353) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = ';';
            callCharBuf[place + 1] = '\r';
            Serial1.write(callCharBuf);
            delay((16 / delayForUart) * 1000);
            inCall = true;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            tft.textSetCursor(20, 20);
            tft.textWrite("Calling...");
            delay(4000);
            lastInteract = millis();
            tft.graphicsMode();
            tft.fillRect(0, 0, 75, 279, RA8875_BLACK);
            place = 3;
          }

          //=========================
          //      HOME BUTTON
          //=========================
          else if (tx > 56 && tx < 159 && ty > 383 && ty < 604) {
            tx = 0;
            ty = 0;
            drawHomeScreen();
            atHomeScreen = true;
            atPhoneScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
          }
        }

        //=========================
        //       END/HANG UP
        //=========================
        if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
          tx = 0;
          ty = 0;
          inCall = false;
          Serial1.write("ATH\r");
          delay((4 / delayForUart) * 1000);
          clearScreen();
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(100, 15);
          tft.textWrite("Call Ended");
          delay(2000);
          lastInteract = millis();
          place = 3;
          resultStr = "";
          if (atPasscodeScreen) {
            atPhoneScreen = false;
            drawPasscodeScreen();
          }
          else {
            atAnsweredScreen = false;
            drawHomeScreen();
          }
        }
      }


      //================================================================================================
      //  ANSWERED SCREEN TOUCH
      //================================================================================================
      else if (atAnsweredScreen) {
        int topRow_xMin = 689;
        int topRow_xMax = 778;
        int secondRow_xMin = 529;
        int secondRow_xMax = 627;
        int thirdRow_xMin = 347;
        int thirdRow_xMax = 442;
        int bottomRow_xMin = 194;
        int bottomRow_xMax = 285;

        int leftColumn_yMin = 190;
        int leftColumn_yMax = 305;
        int middleColumn_yMin = 443;
        int middleColumn_yMax = 577;
        int rightColumn_yMin = 690;
        int rightColumn_yMax = 872;

        //=========================
        //          #1
        //=========================
        if (tx > topRow_xMin && tx < topRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '1';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("1");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=1\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #2
        //=========================
        else if (tx > topRow_xMin && tx < topRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '2';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("2");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=2\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #3
        //=========================
        else if (tx > topRow_xMin && tx < topRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '3';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("3");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=3\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #4
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '4';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("4");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=4\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #5
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '5';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("5");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=5\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #6
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '6';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("6");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=6\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #7
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '7';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("7");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=7\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #8
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '8';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("8");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=8\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #9
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '9';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("9");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=9\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //           *
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '*';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("*");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=*\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //          #0
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '0';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("0");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=0\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //           #
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          callCharBuf[place] = '#';
          place++;
          tft.textMode();
          tft.textRotate(true);
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPlace();
          tft.textWrite("#");

          // In call DTMF Tone
          if (inCall) {
            Serial1.write("AT+VTS=#\r");
            delay((9 / delayForUart) * 1000);
          }
        }

        //=========================
        //       END/HANG UP
        //=========================
        if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
          tx = 0;
          ty = 0;
          inCall = false;
          Serial1.write("ATH\r");
          delay((4 / delayForUart) * 1000);
          clearScreen();
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(70, 15);
          tft.textWrite("Call Ended");
          delay(2000);
          lastInteract = millis();
          place = 3;
          resultStr = "";
          if (atPasscodeScreen) {
            atAnsweredScreen = false;
            drawPasscodeScreen();
          }
          else {
            atAnsweredScreen = false;
            drawHomeScreen();
          }
        }
      }

      //================================================================================================
      //  PASSCODE SCREEN
      //================================================================================================
      if (atPasscodeScreen && !atPhoneScreen && !atAnsweredScreen) {
        int topRow_xMin = 647;
        int topRow_xMax = 737;
        int secondRow_xMin = 486;
        int secondRow_xMax = 579;
        int thirdRow_xMin = 303;
        int thirdRow_xMax = 398;
        int bottomRow_xMin = 164;
        int bottomRow_xMax = 246;

        int leftColumn_yMin = 190;
        int leftColumn_yMax = 305;
        int middleColumn_yMin = 443;
        int middleColumn_yMax = 577;
        int rightColumn_yMin = 690;
        int rightColumn_yMax = 872;

        //=========================
        //          #1
        //=========================
        if (tx > topRow_xMin && tx < topRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "1";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #2
        //=========================
        else if (tx > topRow_xMin && tx < topRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "2";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #3
        //=========================
        else if (tx > topRow_xMin && tx < topRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "3";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #4
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "4";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #5
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "5";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #6
        //=========================
        else if (tx > secondRow_xMin && tx < secondRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "6";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #7
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "7";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #8
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "8";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //          #9
        //=========================
        else if (tx > thirdRow_xMin && tx < thirdRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "9";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //           <-
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > leftColumn_yMin && ty < leftColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode = passcode.substring(0, passcode.length() - 1);
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite(" ");
          passcodePlace--;
        }

        //=========================
        //          #0
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > middleColumn_yMin && ty < middleColumn_yMax) {
          tx = 0;
          ty = 0;
          passcode += "0";
          passcodePlace++;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          switchPasscodePlace();
          tft.textWrite("*");
        }

        //=========================
        //           OK
        //=========================
        else if (tx > bottomRow_xMin && tx < bottomRow_xMax && ty > rightColumn_yMin && ty < rightColumn_yMax) {
          tx = 0;
          ty = 0;
          if (passcode == realPasscode) {
            tft.graphicsMode();
            drawHomeScreen();
            atHomeScreen = true;
            atPasscodeScreen = false;
            atTextMessageScreen = false;
            atPhoneScreen = false;
            atAnswerScreen = false;
            passcode = "";
            passcodePlace = 0;
          } else {
            tft.graphicsMode();
            tft.fillRect(50, 20, 80, 200, RA8875_BLACK);
            tft.textMode();
            tft.textEnlarge(1);
            delay(2);
            tft.textColor(RA8875_WHITE, RA8875_RED);
            tft.textSetCursor(75, 30);
            tft.textWrite("Wrong Passcode");
            passcode = "";
            passcodePlace = 0;
            delay(3000);
            drawPasscodeScreen();
          }
        }
      }
    }
  }

  //================================================================================================
  //  CLEAR CALL BUFFER AT 17 CHARACTERS
  //================================================================================================
  if (!inCall && place > 17) {
    place = 3;

    // clear number display
    tft.graphicsMode();
    clearScreen();

    // Alert User of to long of string
    tft.textMode();
    tft.textRotate(true);
    tft.textColor(RA8875_WHITE, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(2);
    tft.textSetCursor(0, 100);
    tft.textWrite("Alert!");
    tft.textSetCursor(35, 20);
    tft.textWrite("17 characters or less");
    delay(3000);
    lastInteract = millis();
    drawPhoneScreen();
  }

  //================================================================================================
  //  PRINT SERIAL DATA AT END OF EACH STATEMENT OR REQUEST
  //================================================================================================
  // Print data coming from GPRS then clear
  if (bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n") {
    if (resultStr == "Text Read") {
      textBuf += bufGPRS;
      textReadStarted = true;
    }
    else if (resultStr == "OK" && textReadStarted) {
      textReady = true;
      textReadStarted = false;
    }
    Serial.print(bufGPRS);
    bufGPRS = "";
  }
}
//==================================================================================================================================================

//   END OF MAIN PROGRAM

//==================================================================================================================================================






//==================================================================================================================================================

//   FUNCTIONS CALLS

//==================================================================================================================================================
void switchPlace() {
  int x_Position = 20;

  switch (place) {
    case 3:
      tft.textSetCursor(x_Position, 0);
      break;
    case 4:
      tft.textSetCursor(x_Position, 20);
      break;
    case 5:
      tft.textSetCursor(x_Position, 40);
      break;
    case 6:
      tft.textSetCursor(x_Position, 60);
      break;
    case 7:
      tft.textSetCursor(x_Position, 80);
      break;
    case 8:
      tft.textSetCursor(x_Position, 100);
      break;
    case 9:
      tft.textSetCursor(x_Position, 120);
      break;
    case 10:
      tft.textSetCursor(x_Position, 140);
      break;
    case 11:
      tft.textSetCursor(x_Position, 160);
      break;
    case 12:
      tft.textSetCursor(x_Position, 180);
      break;
    case 13:
      tft.textSetCursor(x_Position, 200);
      break;
    case 14:
      tft.textSetCursor(x_Position, 220);
      break;
    case 15:
      tft.textSetCursor(x_Position, 240);
      break;
    case 16:
      tft.textSetCursor(x_Position, 260);
      break;
  }
}
void drawHomeScreen() {
  tft.graphicsMode();
  clearScreen();
  tft.fillCircle(400, 70, 45, RA8875_WHITE);
  tft.fillCircle(400, 198, 45, RA8875_WHITE);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(385, 30);
  tft.textWrite("Phone");
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(385, 176);
  tft.textWrite("SMS");
}
void drawPhoneScreen() {
  int left_Column = 25;
  int middle_Column = 125;
  int right_Column = 225;

  int top_Row = 100;
  int second_Row = 185;
  int third_Row = 270;
  int bottom_Row = 350;

  tft.graphicsMode();
  clearScreen();
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(top_Row, left_Column);
  tft.textWrite("1");
  tft.textSetCursor(top_Row, middle_Column);
  tft.textWrite("2");
  tft.textSetCursor(top_Row, right_Column);
  tft.textWrite("3");
  tft.textSetCursor(second_Row, left_Column);
  tft.textWrite("4");
  tft.textSetCursor(second_Row, middle_Column);
  tft.textWrite("5");
  tft.textSetCursor(second_Row, right_Column);
  tft.textWrite("6");
  tft.textSetCursor(third_Row, left_Column);
  tft.textWrite("7");
  tft.textSetCursor(third_Row, middle_Column);
  tft.textWrite("8");
  tft.textSetCursor(third_Row, right_Column);
  tft.textWrite("9");
  tft.textSetCursor(bottom_Row, left_Column);
  tft.textWrite("*");
  tft.textSetCursor(bottom_Row, middle_Column);
  tft.textWrite("0");
  tft.textSetCursor(bottom_Row, right_Column);
  tft.textWrite("#");
  tft.textEnlarge(1);
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(437, 8);
  tft.textWrite("Call");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textSetCursor(437, 104);
  tft.textWrite("Home");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("End");
}
void drawAnsweredScreen() {
  int left_Column = 25;
  int middle_Column = 125;
  int right_Column = 225;

  int top_Row = 100;
  int second_Row = 185;
  int third_Row = 270;
  int bottom_Row = 350;

  tft.graphicsMode();
  clearScreen();
  tft.fillRect(425, 0, 54, 185, RA8875_BLACK);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(top_Row, left_Column);
  tft.textWrite("1");
  tft.textSetCursor(top_Row, middle_Column);
  tft.textWrite("2");
  tft.textSetCursor(top_Row, right_Column);
  tft.textWrite("3");
  tft.textSetCursor(second_Row, left_Column);
  tft.textWrite("4");
  tft.textSetCursor(second_Row, middle_Column);
  tft.textWrite("5");
  tft.textSetCursor(second_Row, right_Column);
  tft.textWrite("6");
  tft.textSetCursor(third_Row, left_Column);
  tft.textWrite("7");
  tft.textSetCursor(third_Row, middle_Column);
  tft.textWrite("8");
  tft.textSetCursor(third_Row, right_Column);
  tft.textWrite("9");
  tft.textSetCursor(bottom_Row, left_Column);
  tft.textWrite("*");
  tft.textSetCursor(bottom_Row, middle_Column);
  tft.textWrite("0");
  tft.textSetCursor(bottom_Row, right_Column);
  tft.textWrite("#");
  tft.textEnlarge(1);
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("End");
}
void drawTextMessageScreen() {
  tft.graphicsMode();
  clearScreen();
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  // Next
  tft.fillTriangle(120, 150, 75, 175, 120, 200, RA8875_WHITE);
  // Previous
  tft.fillTriangle(75, 210, 120, 235, 75, 260, RA8875_WHITE);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textSetCursor(45, 0);
  tft.textWrite("Index ");
  tft.textSetCursor(100, 20);
  tft.textWrite("From #");
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(437, 15);
  tft.textWrite("New");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textSetCursor(437, 104);
  tft.textWrite("Home");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("Del");
}
void drawAnswerScreen () {
  tft.graphicsMode();
  clearScreen();
  tft.fillCircle(400, 70, 55, RA8875_GREEN);
  tft.fillCircle(400, 198, 55, RA8875_RED);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(385, 23);
  tft.textWrite("Answer");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(385, 148);
  tft.textWrite("Ignore");
}
void drawMessagePad() {
  int first_Row = 141;
  int second_Row = 187;
  int third_Row = 233;

  int first_Col = 30;
  int second_Col = 75;
  int third_Col = 120;
  int fourth_Col = 165;
  int fifth_Col = 210;
  int sixth_Col = 255;
  int seventh_Col = 300;
  int eight_Col = 345;
  int ninth_Col = 390;
  int tenth_Col = 435;

  tft.scanV_flip(true);
  tft.textRotate(false);
  tft.graphicsMode();
  clearScreen();
  tft.fillRect(0, 0, 70, 50, RA8875_GREEN);
  tft.fillRect(409, 0, 70, 50, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(18, 18);
  tft.textWrite("SEND");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(427, 18);
  tft.textWrite("BACK");
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(0);
  delay(2);
  tft.textSetCursor(first_Col, first_Row);
  tft.textWrite("Q");
  tft.textSetCursor(second_Col, first_Row);
  tft.textWrite("W");
  tft.textSetCursor(third_Col, first_Row);
  tft.textWrite("E");
  tft.textSetCursor(fourth_Col, first_Row);
  tft.textWrite("R");
  tft.textSetCursor(fifth_Col, first_Row);
  tft.textWrite("T");
  tft.textSetCursor(sixth_Col, first_Row);
  tft.textWrite("Y");
  tft.textSetCursor(seventh_Col, first_Row);
  tft.textWrite("U");
  tft.textSetCursor(eight_Col, first_Row);
  tft.textWrite("I");
  tft.textSetCursor(ninth_Col, first_Row);
  tft.textWrite("O");
  tft.textSetCursor(tenth_Col, first_Row);
  tft.textWrite("P");
  tft.textSetCursor(first_Col, second_Row);
  tft.textWrite("A");
  tft.textSetCursor(second_Col, second_Row);
  tft.textWrite("S");
  tft.textSetCursor(third_Col, second_Row);
  tft.textWrite("D");
  tft.textSetCursor(fourth_Col, second_Row);
  tft.textWrite("F");
  tft.textSetCursor(fifth_Col, second_Row);
  tft.textWrite("G");
  tft.textSetCursor(sixth_Col, second_Row);
  tft.textWrite("H");
  tft.textSetCursor(seventh_Col, second_Row);
  tft.textWrite("J");
  tft.textSetCursor(eight_Col, second_Row);
  tft.textWrite("K");
  tft.textSetCursor(ninth_Col, second_Row);
  tft.textWrite("L");
  tft.textSetCursor(tenth_Col, second_Row);
  tft.textWrite("; ");
  tft.textSetCursor(first_Col, third_Row);
  tft.textWrite("Z");
  tft.textSetCursor(second_Col, third_Row);
  tft.textWrite("X");
  tft.textSetCursor(third_Col, third_Row);
  tft.textWrite("C");
  tft.textSetCursor(fourth_Col, third_Row);
  tft.textWrite("V");
  tft.textSetCursor(fifth_Col, third_Row);
  tft.textWrite("B");
  tft.textSetCursor(sixth_Col, third_Row);
  tft.textWrite("N");
  tft.textSetCursor(seventh_Col, third_Row);
  tft.textWrite("M");
  tft.textSetCursor(eight_Col, third_Row);
  tft.textWrite(", ");
  tft.textSetCursor(ninth_Col, third_Row);
  tft.textWrite(".");
  tft.textSetCursor(tenth_Col, third_Row);
  tft.textWrite(" / ");
  tft.textRotate(true);
}
void drawPasscodeScreen() {
  int left_Column = 25;
  int middle_Column = 125;
  int right_Column = 225;

  int top_Row = 120;
  int second_Row = 205;
  int third_Row = 290;
  int bottom_Row = 370;

  tft.graphicsMode();
  clearScreen();
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(top_Row, left_Column);
  tft.textWrite("1");
  tft.textSetCursor(top_Row, middle_Column);
  tft.textWrite("2");
  tft.textSetCursor(top_Row, right_Column);
  tft.textWrite("3");
  tft.textSetCursor(second_Row, left_Column);
  tft.textWrite("4");
  tft.textSetCursor(second_Row, middle_Column);
  tft.textWrite("5");
  tft.textSetCursor(second_Row, right_Column);
  tft.textWrite("6");
  tft.textSetCursor(third_Row, left_Column);
  tft.textWrite("7");
  tft.textSetCursor(third_Row, middle_Column);
  tft.textWrite("8");
  tft.textSetCursor(third_Row, right_Column);
  tft.textWrite("9");
  tft.textSetCursor(bottom_Row, middle_Column);
  tft.textWrite("0");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(bottom_Row, left_Column);
  tft.textWrite("<-");
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(bottom_Row, 200);
  tft.textWrite("OK");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textEnlarge(1);
  delay(2);
  tft.textSetCursor(440, 24);
  tft.textWrite("Enter Passcode");
}
void switchPasscodePlace() {
  int x_Position = 60;
  switch (passcodePlace) {
    case 0:
      tft.textSetCursor(x_Position, 0);
      break;
    case 1:
      tft.textSetCursor(x_Position, 30);
      break;
    case 2:
      tft.textSetCursor(x_Position, 60);
      break;
    case 3:
      tft.textSetCursor(x_Position, 90);
      break;
    case 4:
      tft.textSetCursor(x_Position, 120);
      break;
    case 5:
      tft.textSetCursor(x_Position, 150);
      break;
    case 6:
      tft.textSetCursor(x_Position, 180);
      break;
    case 7:
      tft.textSetCursor(x_Position, 210);
      break;
    case 8:
      tft.textSetCursor(x_Position, 240);
      break;
  }
}
void printTime() {
  char time[20];

  // Determine the start and finish of body for Time format number
  int time_Start = bufGPRS.indexOf('"');
  int time_End = bufGPRS.indexOf('-');

  //=========================
  //      HOUR FORMAT
  //=========================
  time[0] = bufGPRS.charAt(time_Start + 10);
  time[1] = bufGPRS.charAt(time_Start + 11);
  unsigned int am_pm = 0;
  am_pm += (time[0] - 48) * 10;
  am_pm += time[1] - 48;

  //=========================
  //      MINUTE FORMAT
  //=========================
  time[2] = ':';
  time[3] = bufGPRS.charAt(time_Start + 13);
  time[4] = bufGPRS.charAt(time_Start + 14);

  //=========================
  //        AM & PM
  //=========================
  if (am_pm < 12) {
    time[5] = 'a';
    time[6] = 'm';
    if (time[1] == '0') {
      time[0] = '1';
      time[1] = '2';
    }
  }
  else {
    time[5] = 'p';
    time[6] = 'm';
    if (time[0] == '2' && time[1] < '2') {
      time[0] = '0';
      time[1] += 8;
    }
    else {
      time[0] -= 1;
      time[1] -= 2;
    }
  }

  int first_Letter = 7;
  int second_Letter = 8;
  int third_Letter = 9;

  //=========================
  //      MONTH FORMAT
  //=========================
  // (default year:month:day hour:minute:second)
  if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '1') {
    time[first_Letter] = 'J';
    time[second_Letter] = 'a';
    time[third_Letter] = 'n';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '2') {
    time[first_Letter] = 'F';
    time[second_Letter] = 'e';
    time[third_Letter] = 'b';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '3') {
    time[first_Letter] = 'M';
    time[second_Letter] = 'a';
    time[third_Letter] = 'r';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '4') {
    time[first_Letter] = 'A';
    time[second_Letter] = 'p';
    time[third_Letter] = 'r';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '5') {
    time[first_Letter] = 'M';
    time[second_Letter] = 'a';
    time[third_Letter] = 'y';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '6') {
    time[first_Letter] = 'J';
    time[second_Letter] = 'u';
    time[third_Letter] = 'n';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '7') {
    time[first_Letter] = 'J';
    time[second_Letter] = 'u';
    time[third_Letter] = 'l';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '8') {
    time[first_Letter] = 'A';
    time[second_Letter] = 'u';
    time[third_Letter] = 'g';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '9') {
    time[first_Letter] = 'S';
    time[second_Letter] = 'e';
    time[third_Letter] = 'p';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '0') {
    time[first_Letter] = 'O';
    time[second_Letter] = 'c';
    time[third_Letter] = 't';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '1') {
    time[first_Letter] = 'N';
    time[second_Letter] = 'o';
    time[third_Letter] = 'v';
  }
  else if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '2') {
    time[first_Letter] = 'D';
    time[second_Letter] = 'e';
    time[third_Letter] = 'c';
  }
  time[third_Letter + 1] = ' ';

  //=========================
  //      Day FORMAT
  //=========================
  time[third_Letter + 2] = bufGPRS.charAt(time_Start + 7);
  time[third_Letter + 3] = bufGPRS.charAt(time_Start + 8);
  time[third_Letter + 4] = ',';

  //=========================
  //      YEAR FORMAT
  //=========================
  time[third_Letter + 5] = '2';
  time[third_Letter + 6] = '0';
  time[third_Letter + 7] = bufGPRS.charAt(time_Start + 1);
  time[third_Letter + 8] = bufGPRS.charAt(time_Start + 2);

  //=========================
  //  WRITE TIME TO SCREEN
  //=========================
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textSetCursor(0, 53);
  tft.textEnlarge(1);
  delay(2);
  tft.writeCommand(RA8875_MRWC);
  for (int i = 0; i < first_Letter; i++) {
    tft.writeData(time[i]);
    delay(1);
  }
  tft.textSetCursor(15, 180);
  tft.textEnlarge(0);
  delay(2);
  tft.writeCommand(RA8875_MRWC);
  for (int i = first_Letter; i < third_Letter + 9; i++) {
    tft.writeData(time[i]);
    delay(1);
  }
}
void clearScreen() {
  tft.fillRect(32, 0, 448, 279, RA8875_BLACK);
}
void fullScreenClear() {
  tft.fillRect(0, 0, 479, 279, RA8875_BLACK);
}
void printTouchValues() {
  if (tx > 50 && ty > 50) {
    Serial.print("___________________________\r\n");
    Serial.print("Touched:\r\n");
    Serial.print("         tX position is |");
    Serial.print(tx);
    Serial.print("|\r\n");
    Serial.print("         tY position is |");
    Serial.print(ty);
    Serial.print("|\r\n");
    Serial.print("___________________________\r\n");
  }
}
void print_bufGPRS() {
  Serial.print("___________________________\r\n");
  Serial.print("bufGPRS: ||\r\n");
  Serial.print(bufGPRS);
  Serial.print("||\r\n");
  Serial.print("___________________________\r\n");
}
void testforloops() {
  Serial.print("I'm dying\r\n");
}
//==================================================================================================================================================

//   END OF FUNCTION CALLS

//==================================================================================================================================================
