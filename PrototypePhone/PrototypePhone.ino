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
  Display time in a human readable format including AM/PM and updated at 00 seconds
  Seperate and Defined Screens with functions and touch values per screen (ie. Phone vs SMS)
  Read individual texts defined/sorted by index
  Delete SMS
  Caller ID
  Display Missed Calls
  Passcode along with 5 second lapse before start
  Notification LEDs
  Display has timeout sleep based on Touch Inactivity
  Automatic Power On for SIM Module
  Screen Darkening clockTime before Sleep mode

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
uint16_t touchValue_X = 0, touchValue_Y = 0;

// Touch Event
boolean touched = false;

// Keep track of clockTime since Last Touched event
unsigned long lastInteract = 0;

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
char old_missedCalls = '0';
char newMessages = '0';
char old_newMessages = '0';

// Battery Condition updated with clockTime mintues
boolean displayBattery = false;
boolean charging = false;
char battLevel[2];
char battVoltage[4];

// Keep track of clockTime elapsed since last query for clockTime (defined per 60 seconds)
unsigned long int next60 = 60;
unsigned long int every60 = 0;
boolean displayclockTime = false;
char clockTime[20];

// Define where on the phone the User is and what part of the phone functions to jump to
boolean atHomeScreen = false;
boolean atPhoneScreen = false;
boolean atTextMessageScreen = false;
boolean atAnswerScreen = false;
boolean atAnsweredScreen = false;
boolean atPasscodeScreen = true;
boolean atSendTextScreen = false;

// Display State and clockTimes
boolean displaySleep = false;
unsigned int displayclockTimeout = 60 * 1000;
unsigned int sleepDarken = 10 * 1000;

// realPasscode is your passcode, displays 8 characters correctly but technically can be as long as desired
String realPasscode = "1234";
String passcode = "";
int passcodePlace = 0;
unsigned long int startPasscodeLock = 0;
unsigned int passcodeclockTimeout = 10 * 1000;

// Notification LEDs
int missedCallsLED = 10;
int messageLED = 11;

// Power Button
int powerButton = 13;

// Delay for write on Serial1 baud, bitDepth is CPU/MCU/MPU bits
double bitDepth = 8;
double buadRateGPRS = 38400;
unsigned int delayForUart = (buadRateGPRS / (bitDepth + 2.0));

// Keep track of in call clockTime
unsigned int prevCallclockTime = 0;
unsigned int callclockTime = 0;

// Seconds of time
unsigned int = 0;

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

  // Update clockTime from network
  Serial1.write("AT+CLTS=1\r");
  delay((10 / delayForUart) * 1000);

  // Make sure Caller ID is on
  Serial1.write("AT+CLIP=1\r");
  delay((10 / delayForUart) * 1000);

  // Make sure Text Mode is readable (not PDU)
  Serial1.write("AT+CMGF=1\r");
  delay((10 / delayForUart) * 1000);

  // Enable Sleep Mode on SIM900 (wake-up on serial)
  //Serial1.write("AT+CSCLK=2\r");
  //delay((11 / delayForUart) * 1000);

  // If Sim Module is currently powered on, check clockTime
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
  //     Time in Seconds
  //=========================
  seconds = seconds;


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
      displayclockTime = true;
      every60 = seconds;
    }
    else if (bufGPRS.substring(0, 5) == "+CBC:")
      displayBattery = true;
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
    else if (bufGPRS == "Call Ready\r\n") {
      Serial1.write("AT+CCLK?\r");
      delay((11 / delayForUart) * 1000);
      resultStr = "Call Ready";
    }
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
    if (newMessages > old_newMessages) {
      old_newMessages = newMessages;
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
      delay(2);

      // Display Index Number
      tft.textSetCursor(45, 100);
      tft.writeCommand(RA8875_MRWC);
      for (int i = 8; i <= 9; i++) {
        tft.writeData(sendCharText[i]);
        delay(1);
      }

      // Define the length of Caller Number to display on-screen
      int receivingNumStart = textBuf.indexOf(',');
      if (receivingNumStart + 2 == '+') {
        receivingNumStart += 3;
      } else {
        receivingNumStart += 2;
      }
      int receivingNumEnd = (textBuf.indexOf(',', receivingNumStart + 1));
      receivingNumEnd -= 1;

      // Display Caller Number for New Message
      tft.textEnlarge(0);
      delay(2);
      tft.textSetCursor(130, 60);
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

      // Needs more intelligence, only clear if all messages are read
      clearNotifications();
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
  //   Battery Display
  //==========================================================
  if (displayBattery) {
    displayBattery = false;
    charging = (bufGPRS.charAt(bufGPRS.indexOf(',') - 1)) - 48;
    battLevel[0] = bufGPRS.charAt(bufGPRS.indexOf(',') + 1);
    battLevel[1] = bufGPRS.charAt(bufGPRS.indexOf(',') + 2);
    battVoltage[0] = bufGPRS.charAt(bufGPRS.indexOf(',') + 4);
    battVoltage[1] = bufGPRS.charAt(bufGPRS.indexOf(',') + 5);
    battVoltage[2] = bufGPRS.charAt(bufGPRS.indexOf(',') + 6);
    battVoltage[3] = bufGPRS.charAt(bufGPRS.indexOf(',') + 7);
    batteryDisplay();
  }

  //==========================================================
  //      clockTime DISPLAY
  //==========================================================
  if (displayclockTime) {
    printClockTime();
    char seconds[2];
    seconds[0] = bufGPRS.charAt(bufGPRS.indexOf(':', 22) + 1);
    seconds[1] = bufGPRS.charAt(bufGPRS.indexOf('-') - 1);
    next60 = (seconds[0] - 48) * 10;
    next60 += (seconds[1] - 48);
    next60 = 60 - next60;

    // Update Battery Indicator
    Serial1.write("AT+CBC\r");
    delay((7 / delayForUart) * 1000);
  }
  else if (seconds >= next60 + every60) {
    every60 = seconds;
    Serial1.write("AT+CCLK?\r");
    delay((9 / delayForUart) * 1000);
  }

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
  if (missedCalls >= '1' && missedCalls != old_missedCalls) {
    old_missedCalls = missedCalls;
    analogWrite(missedCallsLED, 255);
    tft.textMode();
    tft.textColor(RA8875_RED, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(2);
    tft.textSetCursor(0, 0);
    tft.textWrite("!");
  }

  if (newMessages >= '1' && newMessages != old_newMessages) {
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
  }

  //==========================================================
  //  DISPLAY clockTimeOUT SLEEP
  //==========================================================
  if (!displaySleep && millis() >= (lastInteract + displayclockTimeout)) {
    startPasscodeLock = millis();
    lastInteract = millis();
    tft.displayOn(false);
    tft.PWM1out(0);
    tft.touchEnable(false);
    displaySleep = true;
    delay(20);
  }
  else if (!displaySleep && millis() >= (lastInteract + (displayclockTimeout - sleepDarken))) {
    tft.PWM1out(65);
    delay(20);
  }

  //==========================================================
  //  PASSCODE LOCK SCREEN
  //==========================================================
  if (!atPasscodeScreen && displaySleep && millis() >= (startPasscodeLock + passcodeclockTimeout)) {
    drawPasscodeScreen();
    atHomeScreen = false;
    atPhoneScreen = false;
    atTextMessageScreen = false;
    atAnswerScreen = false;
    atPasscodeScreen = true;
  }

  //==========================================================
  //      IN CALL DURATION
  //==========================================================
  if (inCall) {
    unsigned int temp = seconds;
    if (temp >= prevCallclockTime + 1) {
      prevCallclockTime = temp;
      char callclockTimeChar[5];
      callclockTimeChar[2] = 10;
      callclockTime++;
      if (callclockTime >= 60) {
        callclockTimeChar[4] = (callclockTime / 60) / 10;
        callclockTimeChar[3] = (callclockTime / 60) % 10;
        callclockTimeChar[1] = (callclockTime % 60) / 10;
        callclockTimeChar[0] = (callclockTime % 60) % 10;
      }
      else {
        callclockTimeChar[4] = 0;
        callclockTimeChar[3] = 0;
        callclockTimeChar[1] = callclockTime / 10;
        callclockTimeChar[0] = callclockTime % 10;
      }
      tft.graphicsMode();
      tft.fillRect(425, 0, 54, 185, RA8875_BLACK);
      tft.textMode();
      tft.textEnlarge(2);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textSetCursor(425, 15);
      tft.writeCommand(RA8875_MRWC);
      for (int i = 4; i >= 0; i--) {
        tft.writeData(callclockTimeChar[i] + 48);
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
  //float xScale = 1024.0F / tft.width();
  //float yScale = 1024.0F / tft.height();

  //==========================================================
  //      TOUCH ALGORITHUM
  //==========================================================
  while (!digitalRead(RA8875_INT) && tft.touched()) {
    tft.PWM1out(255);
    delay(20);
    touchValue_X = 0;
    touchValue_Y = 0;
    unsigned int averageValue = 1;
    uint16_t tx = 0, ty = 0;
    tft.touchRead(&tx, &ty);
    touched = true;
    touchValue_X += tx;
    touchValue_Y += ty;
    while (!digitalRead(RA8875_INT) || tft.touched()) {
      tft.touchRead(&tx, &ty);
      touchValue_X += tx;
      touchValue_Y += ty;
      averageValue++;
    }
    touchValue_X /= averageValue;
    touchValue_Y /= averageValue;
    printTouchValues();
    lastInteract = millis();
  }

  //==========================================================
  //      TOUCH FUNCTIONS
  //==========================================================
  if (touched) {
    touched = false;
    if (!atPasscodeScreen) {
      //================================================================================================
      //  HOME SCREEN TOUCH
      //================================================================================================
      if (atHomeScreen) {

        //=========================
        //      PHONE BUTTON
        //=========================
        if (touchValue_X > 128 && touchValue_X < 300 && touchValue_Y > 204 && touchValue_Y < 440) {
          touchValue_X = 0;
          touchValue_Y = 0;
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
        else if (touchValue_X > 128 && touchValue_X < 300 && touchValue_Y > 580 && touchValue_Y < 815) {
          touchValue_X = 0;
          touchValue_Y = 0;
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

        //=========================
        //    VIEW NEW MESSAGE
        //=========================
        else if (viewMessage && touchValue_X > 490 && touchValue_X < 576 && touchValue_Y > 163 && touchValue_Y < 756) {
          touchValue_X = 0;
          touchValue_Y = 0;
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

      //================================================================================================
      //  TEXT MESSAGE SCREEN TOUCH
      //================================================================================================
      else if (atTextMessageScreen) {

        //=========================
        //    New Message(SEND)
        //=========================
        if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 130 && touchValue_Y < 353) {
          touchValue_X = 0;
          touchValue_Y = 0;
          delay(2);
          drawMessagePad();
          atSendTextScreen = true;
          atHomeScreen = false;
          atPhoneScreen = false;
          atTextMessageScreen = false;
          atAnswerScreen = false;
          printClockTime();
        }

        //=========================
        //      HOME BUTTON
        //=========================
        else if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 383 && touchValue_Y < 604) {
          touchValue_X = 0;
          touchValue_Y = 0;
          drawHomeScreen();
          atHomeScreen = true;
          atPhoneScreen = false;
          atTextMessageScreen = false;
          atAnswerScreen = false;
        }

        //=========================
        //        DELETE
        //=========================
        else if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 671 && touchValue_Y < 881) {
          touchValue_X = 0;
          touchValue_Y = 0;
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
        else if (touchValue_X > 733 && touchValue_X < 853 && touchValue_Y > 530 && touchValue_Y < 690) {
          touchValue_X = 0;
          touchValue_Y = 0;
          drawTextMessageScreen();
          if (sendCharText[9] == '9') {
            sendCharText[8] += 1;
            sendCharText[9] = '0';
          } else {
            sendCharText[9] += 1;
          }
          Serial1.write(sendCharText);
          delay((11 / delayForUart) * 1000);
        }

        //=========================
        //    PREVIOUS BUTTON
        //=========================
        else if (touchValue_X > 733 && touchValue_X < 853 && touchValue_Y > 700 && touchValue_Y < 863) {
          touchValue_X = 0;
          touchValue_Y = 0;
          if (sendCharText[8] <= '0' && sendCharText[9] <= '1') {
            // (Optional) Display To User, we're already at index 1
          }
          else {
            drawTextMessageScreen();
            if (sendCharText[9] == '0') {
              sendCharText[8] -= 1;
              sendCharText[9] = '9';
            } else {
              sendCharText[9] -= 1;
            }
            Serial1.write(sendCharText);
            delay((11 / delayForUart) * 1000);
          }
        }

        //=========================
        //      RESET INDEX
        //=========================
        else if (touchValue_X > 846 && touchValue_X < 907 && touchValue_Y > 470 && touchValue_Y < 592) {
          touchValue_X = 0;
          touchValue_Y = 0;
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
        if (touchValue_X > 847 && touchValue_X < 1024 && touchValue_Y > 786 && touchValue_Y < 1024) {
          touchValue_X = 0;
          touchValue_Y = 0;
        }
        //=========================
        //      BACK BUTTON
        //=========================
        else if (touchValue_X > 0 && touchValue_X < 170 && touchValue_Y > 786 && touchValue_Y < 1024) {
          touchValue_X = 0;
          touchValue_Y = 0;
          tft.scanV_flip(true);
          tft.textRotate(false);
          delay(2);
          Serial1.write(sendCharText);
          delay((11 / delayForUart) * 1000);
          clearFullScreen();
          drawTextMessageScreen();
          atTextMessageScreen = true;
          atSendTextScreen = false;
          atHomeScreen = false;
          atAnswerScreen = false;
          atPhoneScreen = false;
          printClockTime();
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
      if (touchValue_X > 119 && touchValue_X < 311 && touchValue_Y > 196 && touchValue_Y < 456) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
        atAnsweredScreen = true;
        countRings = 0;
        callclockTime = 0;
        prevCallclockTime = seconds;
      }

      //=========================
      //      IGNORE BUTTON
      //=========================
      else if (touchValue_X > 119 && touchValue_X < 311 && touchValue_Y > 568 && touchValue_Y < 826) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
        if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 130 && touchValue_Y < 353) {
          touchValue_X = 0;
          touchValue_Y = 0;
          callCharBuf[place] = ';';
          callCharBuf[place + 1] = '\r';
          Serial1.write(callCharBuf);
          delay((16 / delayForUart) * 1000);
          inCall = true;
          callclockTime = 0;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(2);
          delay(2);
          tft.textSetCursor(40, 20);
          tft.textWrite("Calling...");
          delay(2000);
          lastInteract = millis();
          tft.graphicsMode();
          clearScreen();
          drawPhoneScreen();
          place = 3;
        }

        //=========================
        //      HOME BUTTON
        //=========================
        else if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 383 && touchValue_Y < 604) {
          touchValue_X = 0;
          touchValue_Y = 0;
          drawHomeScreen();
          atHomeScreen = true;
          atPhoneScreen = false;
          atTextMessageScreen = false;
          atAnswerScreen = false;
          place = 3;
        }
      }

      //=========================
      //       END/HANG UP
      //=========================
      if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 671 && touchValue_Y < 881) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
          atHomeScreen = true;
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
      if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 671 && touchValue_Y < 881) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
          atHomeScreen = true;
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
      if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > topRow_xMin && touchValue_X < topRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > secondRow_xMin && touchValue_X < secondRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > thirdRow_xMin && touchValue_X < thirdRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > leftColumn_yMin && touchValue_Y < leftColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > middleColumn_yMin && touchValue_Y < middleColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
      else if (touchValue_X > bottomRow_xMin && touchValue_X < bottomRow_xMax && touchValue_Y > rightColumn_yMin && touchValue_Y < rightColumn_yMax) {
        touchValue_X = 0;
        touchValue_Y = 0;
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
  int x_Position = 40;

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
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(2);
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
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(2);
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
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(2);
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
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(2);
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
  tft.textEnlarge(0);
  delay(2);
  tft.textSetCursor(130, 0);
  tft.textWrite("From #");
  tft.textEnlarge(1);
  delay(2);
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
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(2);
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
  tft.scanV_flip(true);
  tft.textRotate(false);
  delay(2);
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

  tft.graphicsMode();
  clearFullScreen();
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
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(2);
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
void printClockTime() {
  if (displayclockTime) {
    displayclockTime = false;
    // Determine the start and finish of body for clockTime format number
    int clockTime_Start = bufGPRS.indexOf('"');

    //=========================
    //      HOUR FORMAT
    //=========================
    clockTime[0] = bufGPRS.charAt(clockTime_Start + 10);
    clockTime[1] = bufGPRS.charAt(clockTime_Start + 11);
    unsigned int am_pm = 0;
    am_pm += (clockTime[0] - 48) * 10;
    am_pm += clockTime[1] - 48;

    //=========================
    //      MINUTE FORMAT
    //=========================
    clockTime[2] = ':';
    clockTime[3] = bufGPRS.charAt(clockTime_Start + 13);
    clockTime[4] = bufGPRS.charAt(clockTime_Start + 14);

    //=========================
    //        AM & PM
    //=========================
    if (am_pm < 12) {
      clockTime[5] = 'a';
      clockTime[6] = 'm';
      if (clockTime[1] == '0') {
        clockTime[0] = '1';
        clockTime[1] = '2';
      }
    }
    else {
      clockTime[5] = 'p';
      clockTime[6] = 'm';
      if (clockTime[0] == '2' && clockTime[1] < '2') {
        clockTime[0] = '0';
        clockTime[1] += 8;
      }
      else {
        clockTime[0] -= 1;
        clockTime[1] -= 2;
      }
    }

    int first_Letter = 7;
    int second_Letter = 8;
    int third_Letter = 9;

    //=========================
    //      MONTH FORMAT
    //=========================
    // (default year:month:day hour:minute:second)
    if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '1') {
      clockTime[first_Letter] = 'J';
      clockTime[second_Letter] = 'a';
      clockTime[third_Letter] = 'n';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '2') {
      clockTime[first_Letter] = 'F';
      clockTime[second_Letter] = 'e';
      clockTime[third_Letter] = 'b';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '3') {
      clockTime[first_Letter] = 'M';
      clockTime[second_Letter] = 'a';
      clockTime[third_Letter] = 'r';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '4') {
      clockTime[first_Letter] = 'A';
      clockTime[second_Letter] = 'p';
      clockTime[third_Letter] = 'r';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '5') {
      clockTime[first_Letter] = 'M';
      clockTime[second_Letter] = 'a';
      clockTime[third_Letter] = 'y';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '6') {
      clockTime[first_Letter] = 'J';
      clockTime[second_Letter] = 'u';
      clockTime[third_Letter] = 'n';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '7') {
      clockTime[first_Letter] = 'J';
      clockTime[second_Letter] = 'u';
      clockTime[third_Letter] = 'l';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '8') {
      clockTime[first_Letter] = 'A';
      clockTime[second_Letter] = 'u';
      clockTime[third_Letter] = 'g';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '0' && bufGPRS.charAt(clockTime_Start + 5) == '9') {
      clockTime[first_Letter] = 'S';
      clockTime[second_Letter] = 'e';
      clockTime[third_Letter] = 'p';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '1' && bufGPRS.charAt(clockTime_Start + 5) == '0') {
      clockTime[first_Letter] = 'O';
      clockTime[second_Letter] = 'c';
      clockTime[third_Letter] = 't';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '1' && bufGPRS.charAt(clockTime_Start + 5) == '1') {
      clockTime[first_Letter] = 'N';
      clockTime[second_Letter] = 'o';
      clockTime[third_Letter] = 'v';
    }
    else if (bufGPRS.charAt(clockTime_Start + 4) == '1' && bufGPRS.charAt(clockTime_Start + 5) == '2') {
      clockTime[first_Letter] = 'D';
      clockTime[second_Letter] = 'e';
      clockTime[third_Letter] = 'c';
    }
    clockTime[third_Letter + 1] = ' ';

    //=========================
    //      Day FORMAT
    //=========================
    clockTime[third_Letter + 2] = bufGPRS.charAt(clockTime_Start + 7);
    clockTime[third_Letter + 3] = bufGPRS.charAt(clockTime_Start + 8);
    clockTime[third_Letter + 4] = ',';

    //=========================
    //      YEAR FORMAT
    //=========================
    clockTime[third_Letter + 5] = '2';
    clockTime[third_Letter + 6] = '0';
    clockTime[third_Letter + 7] = bufGPRS.charAt(clockTime_Start + 1);
    clockTime[third_Letter + 8] = bufGPRS.charAt(clockTime_Start + 2);
  }

  //==========================
  //  WRITE clockTime TO SCREEN
  //==========================
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);

  // clockTime text placement changes for the send text massage screen
  if (!atSendTextScreen) {
    tft.scanV_flip(false);
    tft.textRotate(true);
    delay(2);
    tft.textSetCursor(15, 90);
  }
  else {
    tft.scanV_flip(true);
    tft.textRotate(false);
    delay(2);
    tft.textSetCursor(210, 10);
  }
  tft.textEnlarge(0);
  delay(2);
  tft.writeCommand(RA8875_MRWC);
  // Write Time
  for (int i = 0; i < 7; i++) {
    tft.writeData(clockTime[i]);
    delay(1);
  }

  // Date text placement changes for the send text massage screen
  if (!atSendTextScreen) {
    tft.textSetCursor(15, 160);
  }
  else {
    tft.textSetCursor(285, 10);
  }
  tft.textEnlarge(0);
  delay(2);

  // Write Date
  tft.writeCommand(RA8875_MRWC);
  for (int i = 7; i < 18; i++) {
    if (i == 14)
      i += 2;
    tft.writeData(clockTime[i]);
    delay(1);
  }

  // Print battery indicator everytime time is displayed
  batteryDisplay();

}
void batteryDisplay() {
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);

  // Battey Level Indicator text placement changes for the send text massage screen
  if (!atSendTextScreen) {
    tft.scanV_flip(false);
    delay(2);
    tft.textRotate(true);
    delay(2);
    tft.textSetCursor(15, 248);
  }
  else {
    tft.scanV_flip(true);
    delay(2);
    tft.textRotate(false);
    delay(2);
    tft.textSetCursor(370, 10);
  }

  tft.textEnlarge(0);
  delay(2);
  tft.writeCommand(RA8875_MRWC);
  for (int i = 0; i < 2; i++) {
    tft.writeData(battLevel[i]);
    delay(2);
  }
  tft.writeData('%');
  delay(2);
}
void clearScreen() {
  tft.fillRect(32, 0, 448, 279, RA8875_BLACK);
}
void clearFullScreen() {
  tft.fillRect(0, 0, 479, 279, RA8875_BLACK);
}
void clearNotifications() {
  tft.fillRect(0, 0, 32, 50, RA8875_BLACK);
}
void printTouchValues() {
  if (touchValue_X > 50 && touchValue_Y > 50) {
    Serial.print("___________________________\r\n");
    Serial.print("Touched:\r\n");
    Serial.print("         touchValue_X position is |");
    Serial.print(touchValue_X);
    Serial.print("|\r\n");
    Serial.print("         touchValue_Y position is |");
    Serial.print(touchValue_Y);
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
