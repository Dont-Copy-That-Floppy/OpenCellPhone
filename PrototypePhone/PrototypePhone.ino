/******************************************************************************************************

                                   Licensed under the GPLv3

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

// TFT Screen Magic Numbers
// Numpad
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

// test values
const int text_size = 2;
const int delay_input = 2;

// Initialize touch screen callCharBuf_Index values
uint16_t touchValue_X = 0, touchValue_Y = 0;

// Touch Event
bool touched = false;

// Keep track of clockTime since Last Touched event
unsigned long lastInteract = 0;

// Call Buffer
char callCharBuf[20] = {'A', 'T', 'D'};
int callCharBuf_Index = 3;

// Text Messaging array, store from bufGPRS, start and finish of text message read from GPRS
char sendCharText[11] = {'A', 'T', '+', 'C', 'M', 'G', 'R', '=', '0', '1', '\r'};
char textBuf[256] = "";
int textBuf_Index = 0;
bool viewMessage = false;
bool textReadStarted = false;
bool textReady = false;
bool newMessage;

// resultStr defines the functions of the phone
char resultStr[256] = "";
int resultStr_Index = 0;
bool resultStrChange = false;

// Store incoming data from GPRS shield
chat bufGPRS[256] = "";
int bufGPRS_Index = 0;

// Keep track of whether in call or not
bool inCall = false;

// Keep track of missed Calls and new Messages
int countRings = 0;
char missedCalls = 0;
char old_missedCalls = 0;
char newMessages = 0;
char old_newMessages = 0;

// Battery Condition updated with clockTime mintues
bool displayBattery = false;
bool charging = false;
char battLevel[2];
char battVoltage[4];

// Keep track of clockTime elapsed since last query for clockTime (defined per 60 seconds)
unsigned long currentTime = 0;
unsigned long displayTime = 60;
bool displayClockTime = false;
char clockTime[20];

// Define where on the phone the User is and what part of the phone functions to jump to
bool atHomeScreen = false;
bool atPhoneScreen = false;
bool atTextMessageScreen = false;
bool atAnswerScreen = false;
bool atAnsweredScreen = false;
bool atPasscodeScreen = true;
bool atSendTextScreen = false;

// Display State and clockTimes
bool displaySleep = false;
unsigned int displayClockTimeout = 60 * 1000;
unsigned int sleepDarken = 10 * 1000;

// realPasscode is your passcode, displays 8 characters correctly but technically can be as long as desired
const int max_password_length = 16;
char realPasscode[max_password_length] = "1234";
char realPasscode_Index = 0;
char passcode[max_password_length] = "";
int passcode_Index = 0;
unsigned long startPasscodeLock = 0;
unsigned int passcodeClockTimeout = 10 * 1000;
const int x_Position = 60;

// Notification LEDs
const int missedCallsLED = 10;
const int messageLED = 11;

// Power Button
const int powerButton = 13;

// Delay for write on Serial1 baud, bitDepth is CPU/MCU/MPU bits
const double bitDepth = 8;
const double buadRateGPRS = 19200;
unsigned int delayForUart = (buadRateGPRS / (bitDepth + 2.0));

// Keep track of in call clockTime
unsigned int prevCallTime = 0;
unsigned int callTime = 0;


//==================================================================================================================================================

//   END OF DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================









void setup() {
  //==================================================================================================================================================

  //   MAIN SETUP

  //==================================================================================================================================================

  //=========================
  //  Start Serial Monitors
  //=========================

  // Start serial from Arduino to USB
  Serial.begin(19200);

  // Start serial from GPRS to Arduino
  Serial1.begin(buadRateGPRS);

  //=========================
  //    Power on Display
  //=========================

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

  //=========================
  //  Starting Screen
  //=========================

  // Set Screen
  drawPasscodeScreen();

  //=========================
  //   Module Power Check
  //=========================

  // Power on the Sim Module
  pinMode(26, INPUT);
  pinMode(3, OUTPUT);
  bool powerState = digitalRead(26);

  // if module is powered down
  if (!powerState) {
    do {
      // turn module on
      digitalWrite(3, HIGH);
      delay(3000);
      powerState = digitalRead(26);
      delay(200);
    } while (!powerState);
    digitalWrite(3, LOW);

    // wait for module to start sending data
    while (!Serial1.available()) {
    }

    // let module finish data, then print report to serial monitor
    char initChip[1024];
    Serial1.readBytes(initChip, 1024);
    for (int i = 0; i < 1024; i++) {
      Serial.write(initChip[i]);
    }
  }

  //=========================
  //  Module Function Check
  //=========================

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

  // Check clockTime
  Serial1.write("AT+CCLK?\r");
  delay((9 / delayForUart) * 1000);

  currentTime = millis();
  lastInteract = currentTime;
  startPasscodeLock = currentTime;

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
  //          Time
  //=========================
  currentTime = (millis() / 1000);


  //=========================
  //      SERIAL READS
  //=========================
  // When data is coming from GPRS store into bufGPRS
  if (Serial1.available()) {
    bufGPRS[bufGPRS_Index++] = (char)Serial1.read();
  }
  bufGPRS[bufGPRS_Index] = '\0'
  Serial1.flush();

  //When data is coming from USB write to GPRS shield
  if (Serial.available()) {
    Serial1.write(Serial.read());
  }
  Serial.flush();


  //=========================
  //     GPRS FUNCTIONS
  //=========================
  if (strlen(bufGPRS) >= 2 && strcmp(&bufGPRS[strlen(bufGPRS) - 2], "\r\n") == 0) {
    resultStrChange = true;
  
    if (strcmp(bufGPRS, "\r\n") == 0) {
      bufGPRS[0] = '\0'; // Clear buffer
    }
    else if (strncmp(bufGPRS, "+CCLK", 5) == 0) {
      displayClockTime = true;
    }
    else if (strncmp(bufGPRS, "+CBC", 4) == 0) {
      displayBattery = true;
    }
    else if (strncmp(bufGPRS, "+CLIP", 5) == 0) {
      strcpy(resultStr, "Caller ID");
    }
    else if (strcmp(bufGPRS, "RING\r\n") == 0) {
      strcpy(resultStr, "Ringing");
      atAnswerScreen = true;
      atHomeScreen = false;
      atPhoneScreen = false;
      atTextMessageScreen = false;
    }
    else if (strncmp(bufGPRS, "+CMTI", 5) == 0) {
      newMessage = true;
    }
    else if (strncmp(bufGPRS, "+CMGR", 5) == 0) {
      strcpy(resultStr, "Text Read");
    }
    else if (strcmp(bufGPRS, "OK\r\n") == 0) {
      strcpy(resultStr, "OK");
    }
    else if (strcmp(bufGPRS, "NO CARRIER\r\n") == 0) {
      strcpy(resultStr, "No Carrier");
    }
    else if (strcmp(bufGPRS, "Call Ready\r\n") == 0) {
      Serial1.write("AT+CCLK?\r");
      delay((11 / delayForUart) * 1000);
      strcpy(resultStr, "Call Ready");
    }
    else if (strcmp(bufGPRS, "NORMAL POWER DOWN\r\n") == 0) {
      strcpy(resultStr, "NORMAL POWER DOWN");
    }
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
    if (missedCalls > 0) {
      tft.textMode();
      tft.textRotate(true);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(delay_input);
      tft.textSetCursor(270, 15);
      tft.textWrite("Missed Calls ");
      char missedBuf[4]; // enough for "999\0"
      itoa(missedCalls, missedBuf, 10); // base 10
      tft.writeData(missedBuf);
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
      delay(delay_input);
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
      delay(delay_input);
      tft.textSetCursor(210, 40);
      tft.textWrite("View Message");
      viewMessage = true;
    }
  }

  //================================================================================================
  //   TEXT MESSAGE SCREEN
  //================================================================================================
  else if (atTextMessageScreen) {
    if (textReady) {
      // No need to copy into textCharRead — textBuf IS the char buffer
  
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(delay_input);
  
      // Display Index Number
      tft.textSetCursor(45, 100);
      tft.writeCommand(RA8875_MRWC);
      for (int i = 8; i <= 9; i++) {
        tft.writeData(sendCharText[i]);
        delay(1);
      }
  
      // Find start/end of caller number
      int receivingNumStart = strchr(textBuf, ',') - textBuf;
      if (textBuf[receivingNumStart + 2] == '+') {
        receivingNumStart += 3;
      } else {
        receivingNumStart += 2;
      }
  
      int receivingNumEnd = strchr(&textBuf[receivingNumStart], ',') - textBuf - 1;
  
      // Display caller number
      tft.textEnlarge(0);
      delay(delay_input);
      tft.textSetCursor(130, 60);
      tft.writeCommand(RA8875_MRWC);
      for (int i = receivingNumStart; i < receivingNumEnd; i++) {
        tft.writeData(textBuf[i]);
        delay(1);
      }
  
      // Find start and end of message
      char *firstCR = strchr(textBuf, '\r');
      char *secondCR = strchr(firstCR + 1, '\r');
  
      int messageStart = firstCR - textBuf + 2;
      int messageEnd = secondCR - textBuf;
  
      // Display message body
      tft.textSetCursor(175, 0);
      tft.textEnlarge(0);
      delay(delay_input);
      tft.writeCommand(RA8875_MRWC);
      for (int i = messageStart; i < messageEnd; i++) {
        tft.writeData(textBuf[i]);
        delay(1);
      }
  
      textReady = false;
      textBuf[0] = '\0'; // Clear buffer
  
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
      delay(delay_input);
      tft.textSetCursor(75, 20);
      tft.textWrite("Incoming Call");
    }

    //==========================================================
    //    DISPLAY CALLER ID
    //==========================================================
    else if (resultStr == "Caller ID") {
      resultStrChange = false;

      // Find the first and second double quotes
      char *startPtr = strchr(bufGPRS, '"');
      char *endPtr = startPtr ? strchr(startPtr + 1, '"') : NULL;
    
      if (startPtr && endPtr && endPtr > startPtr) {
        // Display caller ID between quotes
        tft.textMode();
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(1);
        delay(delay_input);
        tft.textSetCursor(125, 20);
        tft.writeCommand(RA8875_MRWC);
    
        // Print each character between the quotes
        for (char *p = startPtr + 1; p < endPtr; p++) {
          tft.writeData(*p);
          delay(1);
        }
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
    tft.textEnlarge(text_size);
    delay(delay_input);
    tft.textSetCursor(100, 15);
    tft.textWrite("Call Ended");
    delay(2000);
    if (atPasscodeScreen) {
      drawPasscodeScreen();
    } else {
      drawHomeScreen();
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
        tft.textEnlarge(text_size);
        delay(delay_input);
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

    char *firstComma = strchr(bufGPRS, ',');
    if (firstComma && strlen(firstComma) >= 8) {
      charging = *(firstComma - 1) - '0';  // char to int

      battLevel[0] = *(firstComma + 1);
      battLevel[1] = *(firstComma + 2);

      battVoltage[0] = *(firstComma + 4);
      battVoltage[1] = *(firstComma + 5);
      battVoltage[2] = *(firstComma + 6);
      battVoltage[3] = *(firstComma + 7);
    }

    batteryDisplay();
  }

  //==========================================================
  //      clockTime DISPLAY
  //==========================================================
  if (displayClockTime) {
    printClockTime();
    displayClockTime = false; // Ensure flag is reset

    // Find the last quote (") to locate seconds
    char *lastQuote = strrchr(bufGPRS, '"');
    if (lastQuote && (lastQuote - bufGPRS) >= 5) {
      int secondTens = (lastQuote[-5] - '0') * 10;
      int secondOnes = (lastQuote[-4] - '0');
      displayTime -= (secondTens + secondOnes);
    }

    // Update Battery Indicator
    Serial1.write("AT+CBC\r");
    Serial1.flush();
  }
  else if (currentTime >= displayTime) {
    displayTime += 60;
    Serial1.write("AT+CCLK?\r");
    Serial1.flush();
  }


  //==========================================================
  //   MISSED/NEW MESSAGES COUNT
  //==========================================================
  if (newMessage) {
    newMessages++;
    //=========================
    //    NEW MESSAGE INDEX
    //=========================
    char *comma = strchr(bufGPRS, ',');
    if (comma) {
      if (*(comma + 2) == '\r') {
        sendCharText[9] = *(comma + 1);
      } else {
        sendCharText[8] = *(comma + 1);
        sendCharText[9] = *(comma + 2);
      }
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
  if (missedCalls >= 1 && missedCalls != old_missedCalls) {
    old_missedCalls = missedCalls;
    analogWrite(missedCallsLED, 255);
    tft.textMode();
    tft.textColor(RA8875_RED, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(delay_input);
    tft.textSetCursor(0, 0);
    tft.textWrite("!");
  }

  if (newMessages >= 1 && newMessages != old_newMessages) {
    analogWrite(messageLED, 255);
    tft.textMode();
    tft.textColor(RA8875_YELLOW, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(delay_input);
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
    }
  }

  //==========================================================
  //  DISPLAY clockTimeOUT SLEEP
  //==========================================================
  if (!displaySleep && currentTime >= (lastInteract + displayClockTimeout)) {
    startPasscodeLock = currentTime;
    lastInteract = currentTime;
    tft.displayOn(false);
    tft.PWM1out(0);
    tft.touchEnable(false);
    displaySleep = true;
    delay(20);
  }
  else if (!displaySleep && currentTime >= (lastInteract + (displayClockTimeout - sleepDarken))) {
    tft.PWM1out(65);
    delay(20);
  }

  //==========================================================
  //  PASSCODE LOCK SCREEN
  //==========================================================
  if (!atPasscodeScreen && displaySleep && currentTime >= (startPasscodeLock + passcodeClockTimeout)) {
    drawPasscodeScreen();
  }

  //==========================================================
  //      IN CALL DURATION
  //==========================================================
  if (inCall) {
    if (currentTime >= prevCallTime + 1) {
      prevCallTime = currentTime;
      callTime++; // increment elapsed seconds

      int minutes = callTime / 60;
      int seconds = callTime % 60;

      char timeDisplay[5];  // "mm:ss"
      timeDisplay[0] = (minutes / 10) + '0';
      timeDisplay[1] = (minutes % 10) + '0';
      timeDisplay[2] = ':'; // separator
      timeDisplay[3] = (seconds / 10) + '0';
      timeDisplay[4] = (seconds % 10) + '0';

      tft.graphicsMode();
      tft.fillRect(425, 0, 54, 185, RA8875_BLACK);
      tft.textMode();
      tft.textEnlarge(text_size);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textSetCursor(425, 15);
      tft.writeCommand(RA8875_MRWC);
      tft.textWrite(timeDisplay); // prints "MM:SS"
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
    lastInteract = currentTime;
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
          missedCalls = 0;
          newMessages = 0;
          resultStr = "";
          drawPhoneScreen();
        }

        //=========================
        //      SMS BUTTON
        //=========================
        else if (touchValue_X > 128 && touchValue_X < 300 && touchValue_Y > 580 && touchValue_Y < 815) {
          touchValue_X = 0;
          touchValue_Y = 0;
          missedCalls = 0;
          newMessages = 0;
          Serial1.write(sendCharText);
          delay((11 / delayForUart) * 1000);
          drawTextMessageScreen();
        }

        //=========================
        //    VIEW NEW MESSAGE
        //=========================
        else if (viewMessage && touchValue_X > 490 && touchValue_X < 576 && touchValue_Y > 163 && touchValue_Y < 756) {
          touchValue_X = 0;
          touchValue_Y = 0;
          missedCalls = 0;
          newMessages = 0;
          Serial1.write(sendCharText);
          delay((11 / delayForUart) * 1000);
          drawTextMessageScreen();
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
          delay(delay_input);
          drawMessagePad();
          printClockTime();
        }

        //=========================
        //      HOME BUTTON
        //=========================
        else if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 383 && touchValue_Y < 604) {
          touchValue_X = 0;
          touchValue_Y = 0;
          drawHomeScreen();
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
          delay(delay_input);
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
          delay(delay_input);
          Serial1.write(sendCharText);
          delay((11 / delayForUart) * 1000);
          clearFullScreen();
          drawTextMessageScreen();
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
        tft.textEnlarge(text_size);
        delay(delay_input);
        tft.textSetCursor(100, 30);
        tft.textWrite("Answered");
        delay(1000);
        resultStr = "";
        drawAnsweredScreen();
        countRings = 0;
        callTime = 0;
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
        tft.textEnlarge(text_size);
        delay(delay_input);
        tft.textSetCursor(75, 20);
        tft.textWrite("Ignored");
        delay(1000);
        resultStr = "";
        drawHomeScreen();
        countRings = 0;
      }
    }

    //================================================================================================
    //  PHONE SCREEN TOUCH   //   ANSWERED SCREEN TOUCH
    //================================================================================================
    else if (atPhoneScreen || atAnsweredScreen) {
      char input_result = getPressedKey(touchValue_X, touchValue_Y);
      //=========================
      //          #1
      //=========================
      if (input_result != '\0') {
        touchValue_X = 0;
        touchValue_Y = 0;
        callCharBuf[callCharBuf_Index++] = input_result;
        tft.textMode();
        tft.textRotate(true);
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(text_size);
        delay(delay_input);

        switchcallCharBuf_Index();
        tft.textWrite(input_result);

        // In call DTMF Tone
        if (inCall) {
          Serial1.write("AT+VTS=" + input_result + "\r");
          delay((9 / delayForUart) * 1000);
        }
      }

      if (!atPasscodeScreen && atPhoneScreen) {
        //=========================
        //          CALL
        //=========================
        if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 130 && touchValue_Y < 353) {
          touchValue_X = 0;
          touchValue_Y = 0;
          // might need termination
          callCharBuf[callCharBuf_Index++] = ';';
          callCharBuf[callCharBuf_Index] = '\r';
          Serial1.write(callCharBuf);
          delay((16 / delayForUart) * 1000);
          inCall = true;
          callTime = 0;
          tft.textMode();
          tft.textColor(RA8875_WHITE, RA8875_BLACK);
          tft.textEnlarge(text_size);
          delay(delay_input);
          tft.textSetCursor(40, 20);
          tft.textWrite("Calling...");
          delay(2000);
          lastInteract = currentTime;
          tft.graphicsMode();
          clearScreen();
          drawPhoneScreen();
          callCharBuf_Index = 3;
        }

        //=========================
        //      HOME BUTTON
        //=========================
        else if (touchValue_X > 56 && touchValue_X < 159 && touchValue_Y > 383 && touchValue_Y < 604) {
          touchValue_X = 0;
          touchValue_Y = 0;
          drawHomeScreen();
          callCharBuf_Index = 3;
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
        tft.textEnlarge(text_size);
        delay(delay_input);
        tft.textSetCursor(100, 15);
        tft.textWrite("Call Ended");
        delay(2000);
        lastInteract = currentTime;
        callCharBuf_Index = 3;
        resultStr = "";
        if (atPasscodeScreen) {
          drawPasscodeScreen();
        }
        else {
          drawHomeScreen();
        }
      }
    }

    //================================================================================================
    //  PASSCODE SCREEN
    //================================================================================================
    if (atPasscodeScreen) {
      char input_result = getPressedKey(touchValue_X, touchValue_Y);
      //=========================
      //           1-9
      //=========================
      if (input_result >= '0' && input_result <= '9') {
        touchValue_X = 0;
        touchValue_Y = 0;
        passcode[passcode_Index++] = input_result;
        tft.textMode();
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(text_size);
        delay(delay_input);
        set_passcode_cursor(passcode_Index);
        tft.textWrite("*");
      }
      //=========================
      //           <-
      //=========================
      else if (input_result == '*') {
        touchValue_X = 0;
        touchValue_Y = 0;
        passcode = passcode.substring(0, passcode.length() - 1);
        if (passcode_Index > 0) {
          passcode[--passcode_Index] = '\0';
        }
        tft.textMode();
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(text_size);
        delay(delay_input);
        set_passcode_cursor(passcode_Index);
        tft.textWrite(" ");
      }
      //=========================
      //           OK
      //=========================
      else if (input_result == '#') {
        touchValue_X = 0;
        touchValue_Y = 0;
        if (passcode == realPasscode) {
          tft.graphicsMode();
          drawHomeScreen();
        } else {
          tft.graphicsMode();
          tft.fillRect(50, 20, 80, 200, RA8875_BLACK);
          tft.textMode();
          tft.textEnlarge(1);
          delay(delay_input);
          tft.textColor(RA8875_WHITE, RA8875_RED);
          tft.textSetCursor(75, 30);
          tft.textWrite("Wrong Passcode");
          delay(3000);
          drawPasscodeScreen();
        }
        passcode[0] = '\0';
        passcode_Index = 0;
      }
    }
  }

  //================================================================================================
  //  CLEAR CALL BUFFER AT 17 CHARACTERS
  //================================================================================================
  if (!inCall && callCharBuf_Index > 17) {
    callCharBuf_Index = 3;

    // clear number display
    tft.graphicsMode();
    clearScreen();

    // Alert User of to long of string
    tft.textMode();
    tft.textRotate(true);
    tft.textColor(RA8875_WHITE, RA8875_BLACK);
    tft.textEnlarge(1);
    delay(delay_input);
    tft.textSetCursor(0, 100);
    tft.textWrite("Alert!");
    tft.textSetCursor(35, 20);
    tft.textWrite("17 characters or less");
    delay(3000);
    lastInteract = currentTime;
    drawPhoneScreen();
  }

  //================================================================================================
  //  PRINT SERIAL DATA AT END OF EACH STATEMENT OR REQUEST
  //================================================================================================
  // Print data coming from GPRS then clear
  if (bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()).equals("\r\n")) {
    if (resultStr.equals("Text Read")) {
      textBuf += bufGPRS;
      textReadStarted = true;
    }
    else if (resultStr.equals("OK") && textReadStarted) {
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
void switchcallCharBuf_Index() {
  if (callCharBuf_Index >= 3 && callCharBuf_Index <= 16) {
    int x_Position = 40;
    int y_Position = (callCharBuf_Index - 3) * 20;
    tft.textSetCursor(x_Position, y_Position);
  }
}

void drawHomeScreen() {
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(delay_input);
  tft.graphicsMode();
  clearScreen();
  tft.fillCircle(400, 70, 45, RA8875_WHITE);
  tft.fillCircle(400, 198, 45, RA8875_WHITE);
  tft.textMode();
  tft.textEnlarge(1);
  delay(delay_input);
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(385, 30);
  tft.textWrite("Phone");
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(385, 176);
  tft.textWrite("SMS");
  atHomeScreen = true;
  atPasscodeScreen = false;
  atTextMessageScreen = false;
  atPhoneScreen = false;
  atAnswerScreen = false;
}

void drawKeypad_isPasscode(bool isPasscode, const int rowPositions[4], const int columnPositions[3]) {
  // Define keypad layouts
  const char numpad[4][3] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
  };

  const char passcodePad[4][3] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '<', '0', '>' }  // Representing "<-" and "OK"
  };

  // Start drawing
  tft.textMode();

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 3; col++) {
      tft.textSetCursor(columnPositions[col], rowPositions[row]);

      char key = isPasscode ? passcodePad[row][col] : numpad[row][col];
      tft.writeData(key);
    }
  }
}

void drawPhoneScreen() {
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(delay_input);
  int columnPositions[3] = { 25, 125, 225 };     // left, middle, right
  int rowPositions[4]    = { 100, 185, 270, 350 }; // top, second, third, bottom


  tft.graphicsMode();
  clearScreen();
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(text_size);
  delay(delay_input);

  drawKeypad_isPasscode(false, rowPositions, columnPositions);

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

  atHomeScreen = false;
  atPasscodeScreen = false;
  atTextMessageScreen = false;
  atPhoneScreen = true;
  atAnswerScreen = false;
}
void drawAnsweredScreen() {
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(delay_input);
  int columnPositions[3] = { 25, 125, 225 };     // left, middle, right
  int rowPositions[4]    = { 100, 185, 270, 350 }; // top, second, third, bottom

  tft.graphicsMode();
  clearScreen();
  tft.fillRect(425, 0, 54, 185, RA8875_BLACK);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(text_size);
  delay(delay_input);

  drawKeypad_isPasscode(false, rowPositions, columnPositions);
  
  tft.textEnlarge(1);
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("End");

  atHomeScreen = false;
  atPasscodeScreen = false;
  atTextMessageScreen = false;
  atPhoneScreen = false;
  atAnswerScreen = true;
}
void drawKeyboard(const int* cols, const int* rows) {
  const char* keyboard[] = {
    "QWERTYUIOP",
    "ASDFGHJKL;",
    "ZXCVBNM,./"
  };

  int numRows = sizeof(keyboard) / sizeof(keyboard[0]);
  int numCols = sizeof(cols[0]) ? sizeof(cols) / sizeof(cols[0]) : 0;

  for (int row = 0; row < numRows; row++) {
    const char* rowChars = keyboard[row];
    int len = strlen(rowChars);
    for (int col = 0; col < len && col < numCols; col++) {
      tft.textSetCursor(cols[col], rows[row]);
      tft.textWrite(String(rowChars[col]).c_str());
    }
  }
}
void drawTextMessageScreen() {
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(delay_input);
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
  delay(delay_input);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textSetCursor(45, 0);
  tft.textWrite("Index ");
  tft.textEnlarge(0);
  delay(delay_input);
  tft.textSetCursor(130, 0);
  tft.textWrite("From #");
  tft.textEnlarge(1);
  delay(delay_input);
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(437, 15);
  tft.textWrite("New");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textSetCursor(437, 104);
  tft.textWrite("Home");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("Del");

  atHomeScreen = false;
  atPasscodeScreen = false;
  atTextMessageScreen = true;
  atPhoneScreen = false;
  atAnswerScreen = false;
}
void drawAnswerScreen () {
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(delay_input);
  tft.graphicsMode();
  clearScreen();
  tft.fillCircle(400, 70, 55, RA8875_GREEN);
  tft.fillCircle(400, 198, 55, RA8875_RED);
  tft.textMode();
  tft.textEnlarge(1);
  delay(delay_input);
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(385, 23);
  tft.textWrite("Answer");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(385, 148);
  tft.textWrite("Ignore");

  atHomeScreen = false;
  atPasscodeScreen = false;
  atTextMessageScreen = false;
  atPhoneScreen = false;
  atAnswerScreen = true;
}
void drawMessagePad() {
  int rows[] = {141, 187, 233};
  int cols[] = {30, 75, 120, 165, 210, 255, 300, 345, 390, 435};

  tft.scanV_flip(true);
  tft.textRotate(false);
  delay(delay_input);

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
  delay(delay_input);

  drawKeyboard(cols, rows);
  
  tft.textRotate(true);
}
void drawPasscodeScreen() {
  tft.scanV_flip(false);
  tft.textRotate(true);
  delay(delay_input);
  int columnPositions[3] = { 25, 125, 225 };     // left, middle, right
  int rowPositions[4]    = { 120, 205, 290, 370 }; // top, second, third, bottom

  tft.graphicsMode();
  clearScreen();
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(text_size);
  delay(delay_input);
  
  drawKeypad_isPasscode(true, rowPositions, columnPositions);

  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textEnlarge(1);
  delay(delay_input);
  tft.textSetCursor(440, 24);
  tft.textWrite("Enter Passcode");

  atHomeScreen = false;
  atPasscodeScreen = true;
  atTextMessageScreen = false;
  atPhoneScreen = false;
  atAnswerScreen = false;
}
char getPressedKey(uint16_t touchX, uint16_t touchY) {
  if (touchX >= 689 && touchX <= 778) { // Top Row
    if (touchY >= 190 && touchY <= 305) return '1';
    if (touchY >= 443 && touchY <= 577) return '2';
    if (touchY >= 690 && touchY <= 872) return '3';
  } else if (touchX >= 529 && touchX <= 627) { // Second Row
    if (touchY >= 190 && touchY <= 305) return '4';
    if (touchY >= 443 && touchY <= 577) return '5';
    if (touchY >= 690 && touchY <= 872) return '6';
  } else if (touchX >= 347 && touchX <= 442) { // Third Row
    if (touchY >= 190 && touchY <= 305) return '7';
    if (touchY >= 443 && touchY <= 577) return '8';
    if (touchY >= 690 && touchY <= 872) return '9';
  } else if (touchX >= 194 && touchX <= 285) { // Bottom Row
    if (touchY >= 190 && touchY <= 305) return '*';
    if (touchY >= 443 && touchY <= 577) return '0';
    if (touchY >= 690 && touchY <= 872) return '#';
  }

  return '\0'; // No key matched
}
void printClockTime() {
  if (!displayClockTime) return;
  displayClockTime = false;

  // ===============================
  // Find the starting point of the timestamp in the buffer (first quote)
  // ===============================
  char* clockTimePtr = strchr(bufGPRS, '"');
  if (!clockTimePtr) return;

  // ===============================
  // Parse the hour (24h format)
  // Format: "yy/MM/dd,hh:mm:ss+zone"
  // Hour is at offset +10 and +11
  // ===============================
  int hour = (clockTimePtr[10] - '0') * 10 + (clockTimePtr[11] - '0');

  // Convert to 12-hour format
  int hour12 = hour % 12;
  if (hour12 == 0) hour12 = 12;

  // Store hours
  clockTime[0] = (hour12 >= 10) ? ('0' + (hour12 / 10)) : ' ';
  clockTime[1] = '0' + (hour12 % 10);

  // ===============================
  // Parse minutes
  // Minutes are at offset +13 and +14
  // ===============================
  clockTime[2] = ':';
  clockTime[3] = clockTimePtr[13];
  clockTime[4] = clockTimePtr[14];

  // AM/PM indicator
  clockTime[5] = (hour < 12) ? 'a' : 'p';
  clockTime[6] = 'm';

  // ===============================
  // Parse month to month abbreviation
  // Month is at offset +4 and +5
  // ===============================
  const char* months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  int monthIndex = (clockTimePtr[4] - '0') * 10 + (clockTimePtr[5] - '0') - 1;
  if (monthIndex < 0 || monthIndex > 11) monthIndex = 0;

  clockTime[7]  = months[monthIndex][0];
  clockTime[8]  = months[monthIndex][1];
  clockTime[9]  = months[monthIndex][2];
  clockTime[10] = ' ';

  // ===============================
  // Parse day (offset +7 and +8)
  // ===============================
  clockTime[11] = clockTimePtr[7];
  clockTime[12] = clockTimePtr[8];
  clockTime[13] = ',';
  clockTime[14] = ' ';

  // ===============================
  // Format the year (assumes 20xx)
  // Year is at offset +1 and +2
  // ===============================
  clockTime[15] = '2';
  clockTime[16] = '0';
  clockTime[17] = clockTimePtr[1];
  clockTime[18] = clockTimePtr[2];

  // ===============================
  // Display on TFT
  // ===============================
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(0);

  if (!atSendTextScreen) {
    tft.scanV_flip(false);
    tft.textRotate(true);
    delay(delay_input);
    tft.textSetCursor(15, 90);
  } else {
    tft.scanV_flip(true);
    tft.textRotate(false);
    delay(delay_input);
    tft.textSetCursor(210, 10);
  }

  // Write time
  tft.writeCommand(RA8875_MRWC);
  for (int i = 0; i < 7; i++) {
    tft.writeData(clockTime[i]);
    delay(1);
  }

  // Set cursor for date
  if (!atSendTextScreen) {
    tft.textSetCursor(15, 160);
  } else {
    tft.textSetCursor(285, 10);
  }

  delay(delay_input);
  tft.writeCommand(RA8875_MRWC);

  // Write date
  for (int i = 7; i <= 18; i++) {
    tft.writeData(clockTime[i]);
    delay(1);
  }
}
void batteryDisplay() {
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);

  // Battey Level Indicator text callCharBuf_Indexment changes for the send text massage screen
  if (!atSendTextScreen) {
    tft.scanV_flip(false);
    delay(delay_input);
    tft.textRotate(true);
    delay(delay_input);
    tft.textSetCursor(15, 248);
  }
  else {
    tft.scanV_flip(true);
    delay(delay_input);
    tft.textRotate(false);
    delay(delay_input);
    tft.textSetCursor(370, 10);
  }

  tft.textEnlarge(0);
  delay(delay_input);
  tft.writeCommand(RA8875_MRWC);
  for (int i = 0; i < 2; i++) {
    tft.writeData(battLevel[i]);
    delay(delay_input);
  }
  tft.writeData('%');
  delay(delay_input);
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
void set_passcode_cursor(uint8_t index) {
  if (index < 9) {
    tft.textSetCursor(x_Position, index * 30);
  }
}
void get_substring_by_index(char *output_buffer, const char *input_buffer, int variable_index, int variable_length, int output_buffer_size) {
  if (variable_index < 0 || variable_length < 0 || output_buffer_size < 1) return;

  int i;
  for (i = 0; i < variable_length && i < output_buffer_size - 1 && input_buffer[variable_index + i] != '\0'; i++) {
    output_buffer[i] = input_buffer[variable_index + i];
  }
  output_buffer[i] = '\0'; // Always null-terminate
}

//==================================================================================================================================================

//   END OF FUNCTION CALLS

//==================================================================================================================================================
