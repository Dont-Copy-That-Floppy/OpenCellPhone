/******************************************************************************************************

                Coded and written by Pusalieth aka Jake Pring
                          Licensed under the GPLv2

    This code running on an Arduino combined with the GPRS shield from Seeedstudio, and the
  4.3" tft touch screen from Adafrit, using the RA8875 driver from Adafruit, it will produce
  a functioning phone

  CURRENT FEATURES:
  =================
  Place calls, receive calls, hang up calls
  Dialing numbers and displayed on-screen
  In call DTMF Tones
  Read incoming data from GPRS shield, and provide functions and executions based on data
  Display prompt for incoming texts
  Display Time in a human readable format including AM/PM
  Seperate and Defined Screens with functions and touch values per screen (ie. Phone vs SMS)
  Read individual texts defined/sorted by index
  Delete SMS
  Caller ID
  Display Missed Calls
  Passcode along with 5 second lapse before start
  Notification LEDs
  Display Timeout for sleep based on Inactivity


*******************************************************************************************************/






//==================================================================================================================================================

//   DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================


#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

// Define pins used for RA8875 driver
#define RA8875_INT 3
#define RA8875_CS 9
#define RA8875_RESET 10

// function call tft to use RA8875
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
char sendCharText[13] = {'A', 'T', '+', 'C', 'M', 'G', 'R', '=', '0', '1', '\r', '\n'};
String textBuf = "";
boolean viewMessage = false;
boolean textReadStarted = false;
boolean textReady = false;

// resultStr defines the functions of the phone
String resultStr = "";
String oldResultStr = "";

// Store incoming data from GPRS shield
String bufGPRS = "";

// Keep track of whether in call or not
boolean inCall = false;

// Keep track of missed Calls and new Messages
int countRings = 0;
char missedCalls = '0';
char newMessages = '0';

// Keep track of time elapsed since last call for time (defined per 60 seconds)
unsigned long int every60 = 0;
boolean displayTime = false;

// Define where on the phone the User is and what part of the phone functions to jump to
String whereAmI = "";
boolean atHomeScreen = false;
boolean atPhoneScreen = false;
boolean atTextMessageScreen = false;
boolean atAnswerScreen = false;
boolean atStartUpScreen = false;
boolean atPasscodeScreen = true;
boolean atTextKeyboardScreen = false;

// Display State
boolean displaySleep = false;
int displayTimeout = 20 * 1000;

// realPasscode is your passcode, displays 8 characters correctly but technically can be as long as desired
String realPasscode = "1234";
String passcode = "";
int passcodePlace = 0;
unsigned long int startPasscodeLock = 0;
int passcodeTimeout = 5 * 1000;

// Notification LEDs
int missedCallsLED = 5;
int messageLED = 6;

// Power Button
int powerButton = 12;


//==================================================================================================================================================

//   END OF DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================









void setup() {
  //==================================================================================================================================================

  //   MAIN SETUP

  //==================================================================================================================================================

  // Start serial from Arduino to USB
  Serial.begin(9600);
  // Start serial from GPRS to Arduino
  Serial1.begin(2400);

  // Initialize the RA8875 driver
  Serial.println("RA8875 start");
  if (!tft.begin(RA8875_480x272)) {
    Serial.println("RA8875 Not Found!");
    while (1);
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

  // Flip the vertical and horizontal scanning
  tft.scanV_flip(true);
  tft.scanH_flip(true);

  // Display Boot screen until phone ready
  atStartUpScreen = true;
  tft.textMode();
  tft.textRotate(true);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(5);
  tft.textSetCursor(75, 20);
  tft.textWrite("Booting...");

  // Update time from network
  Serial1.write("AT+CLTS=1\r\n");
  delay(30);

  // Make sure Caller ID is on
  Serial1.write("AT+CLIP=1\r\n");
  delay(30);

  // Enable Sleep Mode on SIM900 (wake-up on serial)
  //Serial1.write("AT+CSCLK=2\r\n");
  delay(30);

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

  oldResultStr = resultStr;

  //=========================
  //      SERIAL READS
  //=========================
  // When data is coming from GPRS store into bufGPRS
  noInterrupts();
  if (Serial1.available()) {
    bufGPRS += (char)Serial1.read();
  } interrupts();

  // When data is coming from USB write to GPRS shield
  if (Serial.available()) {
    Serial1.write(Serial.read());
  }

  //=========================
  //     GPRS FUNCTIONS
  //=========================
  if ((int)bufGPRS.charAt(0) == 255)
    bufGPRS = "";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "Call Ready" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Call Ready";
  if (bufGPRS.substring(0, 6) == "+CLIP:" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Caller ID";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "NO CARRIER" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "No Carrier";
  if (bufGPRS.substring(0, 6) == "+CMTI:" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Incoming Text";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "NORMAL POWER DOWN" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "NORMAL POWER DOWN";
  if (bufGPRS.substring(0, 6) == "+CCLK:" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Time";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "RING" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n") {
    resultStr = "Ringing";
    atAnswerScreen = true;
    atHomeScreen = false;
    atPhoneScreen = false;
    atTextMessageScreen = false;
  }
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "OK" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "OK";
  if (bufGPRS.substring(0, 6) == "+CMGR:" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Text Read";




  //===========================================================================================================================

  //  Defined functions per screen

  //===========================================================================================================================




  //================================================================================================
  // BOOT SCREEN
  //================================================================================================
  if (atStartUpScreen == true) {

    //==========================================================
    //      CALL READY
    //==========================================================
    if (resultStr == "Call Ready" && oldResultStr != resultStr) {
      // clear number display and number pad
      tft.graphicsMode();
      tft.fillScreen(RA8875_BLACK);

      // Display to user that phone is ready for use
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(2);
      delay(2);
      tft.textSetCursor(75, 40);
      tft.textWrite("Ready!");
      lastInteract = millis();
      startPasscodeLock = millis();
      delay(4000);
      lastInteract = millis();
      startPasscodeLock = millis();
      drawPasscodeScreen();
      atHomeScreen = false;
      atPhoneScreen = false;
      atStartUpScreen = false;
      atTextMessageScreen = false;
      atAnswerScreen = false;
      atPasscodeScreen = true;
    }

    //==========================================================
    //       SIM900 OFF
    //==========================================================
    if (resultStr == "NORMAL POWER DOWN" && oldResultStr != resultStr) {

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

  //================================================================================================
  //   HOME SCREEN
  //================================================================================================
  if (atHomeScreen == true) {

    //==========================================================
    //      TIME DISPLAY
    //==========================================================
    if (millis() >= (every60 + 60000)) {
      Serial1.write("AT+CCLK?\r\n");
      every60 = millis();
      displayTime = true;
    }
    if (resultStr == "Time" && displayTime == true) {

      // Display to User GPRS shield is off
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(2);
      delay(2);
      tft.textSetCursor(150, 80);

      // Determine the start and finish of body for Caller id number
      int time_Start = bufGPRS.indexOf('\"');
      int time_End = bufGPRS.indexOf('-');

      /*
      // Make sure that each time call is at the 00 mark
      if (millis() < 120000) {
      int firstDec = bufGPRS.charAt(time_End - 2)
      int countTo_60 = 0;
      while (bufGPRS.substring(time_End - 2, time_End) < "60") {
      countTo_60++;
      }
      every60 += 60000 - (countTo_60 * 1000);
      }*/

      // store data and input to array
      char time[20];

      //=========================
      //      MONTH FORMAT
      //=========================
      // (default year:month:day hour:minute:second)
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '1') {
        time[0] = 'J';
        time[1] = 'a';
        time[2] = 'n';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '2') {
        time[0] = 'F';
        time[1] = 'e';
        time[2] = 'b';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '3') {
        time[0] = 'M';
        time[1] = 'a';
        time[2] = 'r';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '4') {
        time[0] = 'A';
        time[1] = 'p';
        time[2] = 'r';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '5') {
        time[0] = 'M';
        time[1] = 'a';
        time[2] = 'y';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '6') {
        time[0] = 'J';
        time[1] = 'u';
        time[2] = 'n';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '7') {
        time[0] = 'J';
        time[1] = 'u';
        time[2] = 'l';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '8') {
        time[0] = 'A';
        time[1] = 'u';
        time[2] = 'g';
      }
      if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '9') {
        time[0] = 'S';
        time[1] = 'e';
        time[2] = 'p';
      }
      if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '0') {
        time[0] = 'O';
        time[1] = 'c';
        time[2] = 't';
      }
      if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '1') {
        time[0] = 'N';
        time[1] = 'o';
        time[2] = 'v';
      }
      if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '2') {
        time[0] = 'D';
        time[1] = 'e';
        time[2] = 'c';
      }
      time[3] = ',';

      //=========================
      //      Day FORMAT
      //=========================
      time[4] = bufGPRS.charAt(time_Start + 7);
      time[5] = bufGPRS.charAt(time_Start + 8);
      time[6] = ' ';

      //=========================
      //      YEAR FORMAT
      //=========================
      time[7] = '2';
      time[8] = '0';
      time[9] = bufGPRS.charAt(time_Start + 1);
      time[10] = bufGPRS.charAt(time_Start + 2);
      time[11] = ' ';
      time[12] = ' ';

      //=========================
      //      HOUR FORMAT
      //=========================
      time[13] = bufGPRS.charAt(time_Start + 10);
      time[14] = bufGPRS.charAt(time_Start + 11);

      //=========================
      //        AM & PM
      //=========================
      if (time[13] <= '1' && time[14] <= '2') {
        time[18] = 'a';
        time[19] = 'm';
      }
      else {
        time[18] = 'p';
        time[19] = 'm';
        if (time[13] == '2' && time[14] < '2') {
          time[13] = '0';
          time[14] += 8;
        } else {
          time[13] -= 1;
          time[14] -= 2;
        }
      }

      //=========================
      //      MINUTE FORMAT
      //=========================
      time[15] = bufGPRS.charAt(time_Start + 12);
      time[16] = bufGPRS.charAt(time_Start + 13);
      time[17] = bufGPRS.charAt(time_Start + 14);

      //=========================
      //     WRITE TO SCREEN
      //=========================
      tft.textSetCursor(20, 0);
      tft.writeCommand(RA8875_MRWC);
      for (int i = 0; i < 20; i++) {
        tft.writeData(time[i]);
        delay(1);
      }
      displayTime = false;
    }

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
      if (atPasscodeScreen != true) {
        drawHomeScreen();
        atHomeScreen = true;
        atAnswerScreen = false;
        atPhoneScreen = false;
        atTextMessageScreen = false;
      }

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
  if (atTextMessageScreen == true) {

    //==========================================================
    //      VIEW MESSAGES
    //==========================================================
    if (textReady == true) {
      char textCharRead[textBuf.length()];
      textBuf.toCharArray(textCharRead, textBuf.length());

      // Prepare to display message
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);

      // Display Index Number
      tft.textSetCursor(30, 110);
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
  if (atAnswerScreen == true) {

    //==========================================================
    //  DISPLAY INCOMING CALL
    //==========================================================
    if (resultStr == "Ringing" && oldResultStr != resultStr) {
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
    if (resultStr == "Caller ID" && oldResultStr != resultStr) {
      // Determine the start and finish of body for Caller id number
      int callerID_Start = bufGPRS.indexOf('\"');
      int callerID_End = bufGPRS.indexOf('\"', callerID_Start + 1);

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

    //==========================================================
    //   CALL LOST (Service drop or call terminated)
    //==========================================================
    if (resultStr == "No Carrier" && oldResultStr != resultStr) {
      inCall = false;
      if (atPasscodeScreen == true) {
        atHomeScreen = false;
        atPhoneScreen = false;
        atTextMessageScreen = false;
        atAnswerScreen = false;
        drawPasscodeScreen();
      } else {
        drawHomeScreen();
        atHomeScreen = true;
        atPhoneScreen = false;
        atTextMessageScreen = false;
        atAnswerScreen = false;
      }
    }
  }
  //===========================================================================================================================

  //  END OF SCREEN FUNCTIONS

  //===========================================================================================================================





  //===========================================================================================================================

  //  BACKGROUND TASKS

  //===========================================================================================================================

  //==========================================================
  //   MISSED/NEW MESSAGES COUNT
  //==========================================================
  if (resultStr == "Incoming Text" && oldResultStr != resultStr) {
    newMessages++;
    //=========================
    //    NEW MESSAGE INDEX
    //=========================
    if (bufGPRS.indexOf(',') + 2 == '\r') {
      sendCharText[8] = bufGPRS.charAt(bufGPRS.indexOf(',') + 1);
    } else {
      sendCharText[8] = bufGPRS.charAt(bufGPRS.indexOf(',') + 1);
      sendCharText[9] = bufGPRS.charAt(bufGPRS.indexOf(',') + 2);
    }
  }

  //==========================================================
  //   MISSED CALLS COUNT
  //==========================================================
  if (countRings > 0 && resultStr == "No Carrier" && oldResultStr != resultStr) {
    missedCalls++;
    countRings = 0;
  }

  //==========================================================
  //    NOTIFICATION LEDS
  //==========================================================
  if (missedCalls >= '1') {
    missedCalls = '0';
    analogWrite(missedCallsLED, 255);
  }
  if (newMessages >= '1') {
    newMessages = '0';
    analogWrite(messageLED, 255);
  }

  //==========================================================
  //  DISPLAY POWER BUTTON
  //==========================================================
  if (!digitalRead(powerButton) && millis() >= (lastInteract + 350)) {
    lastInteract = millis();
    if (displaySleep == true) {
      tft.displayOn(true);
      tft.PWM1out(255);
      tft.touchEnable(true);
      displaySleep = false;
      delay(10);
      return;
    } else {
      tft.displayOn(false);
      tft.PWM1out(0);
      tft.touchEnable(false);
      displaySleep = true;
      startPasscodeLock = millis();
      return;
    }
  }

  //==========================================================
  //  DISPLAY TIMEOUT SLEEP
  //==========================================================
  if (displaySleep == false && atStartUpScreen == false && millis() >= (lastInteract + displayTimeout)) {
    startPasscodeLock = millis();
    lastInteract = millis();
    tft.displayOn(false);
    tft.PWM1out(0);
    tft.touchEnable(false);
    displaySleep = true;
  }

  //==========================================================
  //  PASSCODE LOCK SCREEN
  //==========================================================
  if (atPasscodeScreen == false && displaySleep == true && millis() >= (startPasscodeLock + passcodeTimeout)) {
    drawPasscodeScreen();
    atHomeScreen = false;
    atPhoneScreen = false;
    atTextMessageScreen = false;
    atAnswerScreen = false;
    atStartUpScreen = false;
    atPasscodeScreen = true;
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
  float xScale = 1200.0F / tft.width();
  float yScale = 1200.0F / tft.height();


  if (!digitalRead(RA8875_INT) && tft.touched()) {
    tft.touchRead(&tx, &ty);
    printTouchValues();
    if (millis() >= (lastInteract + 260)) {
      lastInteract = millis();

      if (atPasscodeScreen != true) {
        //================================================================================================
        //  HOME SCREEN TOUCH
        //================================================================================================
        if (atHomeScreen == true) {

          //=========================
          //      PHONE BUTTON
          //=========================
          if (tx > 128 && tx < 300 && ty > 204 && ty < 440) {
            tx = 0;
            ty = 0;
            missedCalls = '0';
            newMessages = '0';
            drawPhoneScreen();
            atPhoneScreen = true;
            atHomeScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
          }

          //=========================
          //      SMS BUTTON
          //=========================
          if (tx > 128 && tx < 300 && ty > 580 && ty < 815) {
            tx = 0;
            ty = 0;
            missedCalls = '0';
            newMessages = '0';
            Serial1.write(sendCharText);
            drawTextMessageScreen();
            atTextMessageScreen = true;
            atHomeScreen = false;
            atPhoneScreen = false;
            atAnswerScreen = false;
            viewMessage = false;
          }

          //=========================
          //    VIEW NEW MESSAGE
          //=========================
          if (viewMessage == true) {
            if (tx > 490 && tx < 576 && ty > 163 && ty < 756) {
              tx = 0;
              ty = 0;
              missedCalls = '0';
              newMessages = '0';
              Serial1.write(sendCharText);
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
        if (atTextMessageScreen == true) {

          //=========================
          //          SEND
          //=========================
          if (tx > 56 && tx < 159 && ty > 130 && ty < 353) {
            tx = 0;
            ty = 0;
            drawMessagePad();
            atTextKeyboardScreen = true;
            atHomeScreen = false;
            atPhoneScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
          }

          //=========================
          //      HOME BUTTON
          //=========================
          if (tx > 56 && tx < 159 && ty > 383 && ty < 604) {
            tx = 0;
            ty = 0;
            drawHomeScreen();
            atHomeScreen = true;
            atPhoneScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
            Serial1.write("AT+CCLK?\r\n");
            every60 = millis();
            displayTime = true;
          }

          //=========================
          //        DELETE
          //=========================
          if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
            tx = 0;
            ty = 0;
            sendCharText[6] = 'D';
            Serial1.write(sendCharText);
            sendCharText[6] = 'R';
            drawTextMessageScreen();
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(1);
            delay(2);
            tft.textSetCursor(150, 20);
            tft.textWrite("Message Deleted");
            delay(2000);
            lastInteract = millis();
            drawTextMessageScreen();
            if (sendCharText[9] == '9') {
              sendCharText[8] += 1;
              sendCharText[9] = '0';
            } else {
              sendCharText[9] += 1;
            }
            Serial1.write(sendCharText);
          }

          //=========================
          //      NEXT BUTTON
          //=========================
          if (tx > 860 && tx < 966 && ty > 718 && ty < 843) {
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
          }

          //=========================
          //    PREVIOUS BUTTON
          //=========================
          if (tx > 712 && tx < 807 && ty > 718 && ty < 874) {
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
          }
        }
      }

      //================================================================================================
      //  ANSWER SCREEN TOUCH
      //================================================================================================
      if (atAnswerScreen == true) {

        //=========================
        //      ANSWER BUTTON
        //=========================
        if (tx > 119 && tx < 311 && ty > 196 && ty < 456) {
          tx = 0;
          ty = 0;
          Serial1.write("ATA\r\n");
          inCall = true;
          tft.fillRect(75, 20, 150, 250, RA8875_BLACK);
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(75, 20);
          tft.textWrite("Answered");
          delay(1000);
          lastInteract = millis();
          drawPhoneScreen();
          atHomeScreen = false;
          atPhoneScreen = true;
          atTextMessageScreen = false;
          atAnswerScreen = false;
          atStartUpScreen = false;
          countRings = 0;
        }

        //=========================
        //      IGNORE BUTTON
        //=========================
        if (tx > 119 && tx < 311 && ty > 568 && ty < 826) {
          tx = 0;
          ty = 0;
          inCall = false;
        }
      }

      //================================================================================================
      //  PHONE SCREEN TOUCH
      //================================================================================================
      if (atPhoneScreen == true) {

        //=========================
        //          #1
        //=========================
        if (tx > 682 && tx < 784 && ty > 190 && ty < 305) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=1\r\n");
        }

        //=========================
        //          #2
        //=========================
        if (tx > 682 && tx < 784 && ty > 443 && ty < 577) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=2\r\n");
        }

        //=========================
        //          #3
        //=========================
        if (tx > 682 && tx < 784 && ty > 690 && ty < 872) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=3\r\n");
        }

        //=========================
        //          #4
        //=========================
        if (tx > 523 && tx < 627 && ty > 190 && ty < 305) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=4\r\n");
        }

        //=========================
        //          #5
        //=========================
        if (tx > 523 && tx < 627 && ty > 443 && ty < 577) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=5\r\n");
        }

        //=========================
        //          #6
        //=========================
        if (tx > 523 && tx < 627 && ty > 690 && ty < 872) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=6\r\n");
        }

        //=========================
        //          #7
        //=========================
        if (tx > 348 && tx < 454 && ty > 190 && ty < 305) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=7\r\n");
        }

        //=========================
        //          #8
        //=========================
        if (tx > 348 && tx < 454 && ty > 443 && ty < 577) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=8\r\n");
        }

        //=========================
        //          #9
        //=========================
        if (tx > 348 && tx < 454 && ty > 690 && ty < 872) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=9\r\n");
        }

        //=========================
        //           *
        //=========================
        if (tx > 207 && tx < 284 && ty > 190 && ty < 305) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=*\r\n");
        }

        //=========================
        //          #0
        //=========================
        if (tx > 207 && tx < 284 && ty > 443 && ty < 577) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=0\r\n");
        }

        //=========================
        //           #
        //=========================
        if (tx > 207 && tx < 284 && ty > 690 && ty < 872) {
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
          if (inCall == true)
            Serial1.write("AT+VTS=#\r\n");
        }

        if (atPasscodeScreen != true) {
          //=========================
          //          CALL
          //=========================
          if (tx > 56 && tx < 159 && ty > 130 && ty < 353) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = ';';
            callCharBuf[place + 1] = '\r';
            callCharBuf[place + 2] = '\n';
            Serial1.write(callCharBuf);
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
          if (tx > 56 && tx < 159 && ty > 383 && ty < 604) {
            tx = 0;
            ty = 0;
            drawHomeScreen();
            atHomeScreen = true;
            atPhoneScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
            Serial1.write("AT+CCLK?\r\n");
            every60 = millis();
            displayTime = true;
          }
        }

        //=========================
        //       END/HANG UP
        //=========================
        if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
          tx = 0;
          ty = 0;
          inCall = false;
          Serial1.write("ATH\r\n");
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(20, 15);
          tft.textWrite("Call Ended");
          delay(2000);
          lastInteract = millis();
          tft.graphicsMode();
          tft.fillRect(0, 0, 75, 279, RA8875_BLACK);
          place = 3;
          if (atPasscodeScreen == true) {
            atPhoneScreen = false;
            drawPasscodeScreen();
          }
        }
      }
      //================================================================================================
      //  PASSCODE SCREEN
      //================================================================================================
      if (atPasscodeScreen == true && atPhoneScreen != true) {

        //=========================
        //          #1
        //=========================
        if (tx > 682 && tx < 784 && ty > 190 && ty < 305) {
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
        if (tx > 682 && tx < 784 && ty > 443 && ty < 577) {
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
        if (tx > 682 && tx < 784 && ty > 690 && ty < 872) {
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
        if (tx > 523 && tx < 627 && ty > 190 && ty < 305) {
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
        if (tx > 523 && tx < 627 && ty > 443 && ty < 577) {
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
        if (tx > 523 && tx < 627 && ty > 690 && ty < 872) {
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
        if (tx > 348 && tx < 454 && ty > 190 && ty < 305) {
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
        if (tx > 348 && tx < 454 && ty > 443 && ty < 577) {
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
        if (tx > 348 && tx < 454 && ty > 690 && ty < 872) {
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
        if (tx > 207 && tx < 284 && ty > 170 && ty < 324) {
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
        if (tx > 207 && tx < 284 && ty > 443 && ty < 577) {
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
        if (tx > 207 && tx < 284 && ty > 672 && ty < 894) {
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
            Serial1.write("AT+CCLK?\r\n");
            every60 = millis();
            displayTime = true;
            lastInteract = millis();
            startPasscodeLock = millis();
            passcode = "";
            passcodePlace = 0;
          } else {
            tft.graphicsMode();
            tft.fillRect(20, 20, 80, 200, RA8875_BLACK);
            tft.textMode();
            tft.textEnlarge(1);
            delay(2);
            tft.textColor(RA8875_WHITE, RA8875_RED);
            tft.textSetCursor(30, 25);
            tft.textWrite("Wrong Passcode");
            passcode = "";
            passcodePlace = 0;
            delay(3000);
            lastInteract = millis();
            startPasscodeLock = millis();
            drawPasscodeScreen();
          }
        }
      }
    }
  }

  //================================================================================================
  //  CLEAR CALL BUFFER AT 17 CHARACTERS
  //================================================================================================
  if (inCall != true && place > 17) {
    place = 3;

    // clear number display
    tft.graphicsMode();
    tft.fillScreen(RA8875_BLACK);

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
    else if (resultStr == "OK" && textReadStarted == true) {
      textReady = true;
      textReadStarted = false;
    }
    Serial.print(bufGPRS);
    Serial.flush();
    Serial1.flush();
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
  switch (place) {
    case 3:
      tft.textSetCursor(20, 0);
      break;
    case 4:
      tft.textSetCursor(20, 20);
      break;
    case 5:
      tft.textSetCursor(20, 40);
      break;
    case 6:
      tft.textSetCursor(20, 60);
      break;
    case 7:
      tft.textSetCursor(20, 80);
      break;
    case 8:
      tft.textSetCursor(20, 100);
      break;
    case 9:
      tft.textSetCursor(20, 120);
      break;
    case 10:
      tft.textSetCursor(20, 140);
      break;
    case 11:
      tft.textSetCursor(20, 160);
      break;
    case 12:
      tft.textSetCursor(20, 180);
      break;
    case 13:
      tft.textSetCursor(20, 200);
      break;
    case 14:
      tft.textSetCursor(20, 220);
      break;
    case 15:
      tft.textSetCursor(20, 240);
      break;
    case 16:
      tft.textSetCursor(20, 260);
      break;
  }
}
void drawHomeScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
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
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(100, 25);
  tft.textWrite("1");
  tft.textSetCursor(100, 125);
  tft.textWrite("2");
  tft.textSetCursor(100, 225);
  tft.textWrite("3");
  tft.textSetCursor(185, 25);
  tft.textWrite("4");
  tft.textSetCursor(185, 125);
  tft.textWrite("5");
  tft.textSetCursor(185, 225);
  tft.textWrite("6");
  tft.textSetCursor(270, 25);
  tft.textWrite("7");
  tft.textSetCursor(270, 125);
  tft.textWrite("8");
  tft.textSetCursor(270, 225);
  tft.textWrite("9");
  tft.textSetCursor(350, 30);
  tft.textWrite("*");
  tft.textSetCursor(350, 125);
  tft.textWrite("0");
  tft.textSetCursor(350, 225);
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
void drawTextMessageScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.fillTriangle(60, 205, 15, 230, 60, 255, RA8875_WHITE);
  tft.fillTriangle(85, 205, 130, 230, 85, 255, RA8875_WHITE);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textSetCursor(30, 0);
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
  tft.fillScreen(RA8875_BLACK);
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
  tft.scanV_flip(false);
  tft.scanH_flip(true);
  tft.textRotate(false);
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillRect(0, 215, 70, 60, RA8875_GREEN);
  tft.fillRect(400, 215, 70, 60, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(0);
  delay(2);
  tft.textSetCursor(0, 30);
  tft.textWrite("Q");
  tft.textSetCursor(10, 30);
  tft.textWrite("W");
  tft.textSetCursor(20, 30);
  tft.textWrite("E");
  tft.textSetCursor(30, 30);
  tft.textWrite("R");
  tft.textSetCursor(40, 30);
  tft.textWrite("T");
  tft.textSetCursor(50, 30);
  tft.textWrite("Y");
  tft.textSetCursor(60, 30);
  tft.textWrite("U");
  tft.textSetCursor(70, 30);
  tft.textWrite("I");
  tft.textSetCursor(80, 30);
  tft.textWrite("O");
  tft.textSetCursor(90, 30);
  tft.textWrite("P");
  tft.textSetCursor(0, 40);
  tft.textWrite("A");
  tft.textSetCursor(10, 40);
  tft.textWrite("S");
  tft.textSetCursor(20, 40);
  tft.textWrite("D");
  tft.textSetCursor(30, 40);
  tft.textWrite("F");
  tft.textSetCursor(40, 40);
  tft.textWrite("G");
  tft.textSetCursor(50, 40);
  tft.textWrite("H");
  tft.textSetCursor(60, 40);
  tft.textWrite("J");
  tft.textSetCursor(70, 40);
  tft.textWrite("K");
  tft.textSetCursor(80, 40);
  tft.textWrite("L");
  tft.textSetCursor(90, 40);
  tft.textWrite("; ");
  tft.textSetCursor(0, 50);
  tft.textWrite("Z");
  tft.textSetCursor(10, 50);
  tft.textWrite("X");
  tft.textSetCursor(20, 50);
  tft.textWrite("C");
  tft.textSetCursor(30, 50);
  tft.textWrite("V");
  tft.textSetCursor(40, 50);
  tft.textWrite("B");
  tft.textSetCursor(50, 50);
  tft.textWrite("N");
  tft.textSetCursor(60, 50);
  tft.textWrite("M");
  tft.textSetCursor(70, 50);
  tft.textWrite(", ");
  tft.textSetCursor(80, 50);
  tft.textWrite(".");
  tft.textSetCursor(90, 50);
  tft.textWrite(" / ");
  tft.textRotate(true);
}
void drawPasscodeScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(100, 25);
  tft.textWrite("1");
  tft.textSetCursor(100, 125);
  tft.textWrite("2");
  tft.textSetCursor(100, 225);
  tft.textWrite("3");
  tft.textSetCursor(185, 25);
  tft.textWrite("4");
  tft.textSetCursor(185, 125);
  tft.textWrite("5");
  tft.textSetCursor(185, 225);
  tft.textWrite("6");
  tft.textSetCursor(270, 25);
  tft.textWrite("7");
  tft.textSetCursor(270, 125);
  tft.textWrite("8");
  tft.textSetCursor(270, 225);
  tft.textWrite("9");
  tft.textSetCursor(350, 125);
  tft.textWrite("0");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(350, 25);
  tft.textWrite("<-");
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(350, 200);
  tft.textWrite("OK");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textEnlarge(1);
  delay(2);
  tft.textSetCursor(437, 21);
  tft.textWrite("Enter Passcode");
}
void switchPasscodePlace() {
  switch (passcodePlace) {
    case 0:
      tft.textSetCursor(20, 0);
      break;
    case 1:
      tft.textSetCursor(20, 30);
      break;
    case 2:
      tft.textSetCursor(20, 60);
      break;
    case 3:
      tft.textSetCursor(20, 90);
      break;
    case 4:
      tft.textSetCursor(20, 120);
      break;
    case 5:
      tft.textSetCursor(20, 150);
      break;
    case 6:
      tft.textSetCursor(20, 180);
      break;
    case 7:
      tft.textSetCursor(20, 210);
      break;
    case 8:
      tft.textSetCursor(20, 240);
      break;
  }
}
void printTouchValues() {
  if (tx > 50 && ty > 50) {
    Serial.print("Touched tX position is |");
    Serial.print(tx);
    Serial.print("|\r\n");
    Serial.print("Touched tY position is |");
    Serial.print(ty);
    Serial.print("|\r\n");
  }
}
//==================================================================================================================================================

//   END OF FUNCTION CALLS

//==================================================================================================================================================
