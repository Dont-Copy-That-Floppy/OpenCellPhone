// include the library code:
#include <LiquidCrystal.h>
int RS=22;
int E=23;
int D4=24;
int D5=25;
int D6=26;
int D7=27;
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(RS, E, D4, D5, D6, D7);

void setup(){
    // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // initialize the serial communications:
  Serial.begin(9600);
}

void loop()
{
  // when characters arrive over the serial port...
  if (Serial.available()) {
    // wait a bit for the entire message to arrive
    delay(100);
    // clear the screen
    lcd.clear();
    // read all the available characters
    while (Serial.available() > 0) {
      // display each character to the LCD
      lcd.write(Serial.read());
    }
  }
}
