//GPRS
unsigned char buffer[64];                                 // buffer array for data recieve over serial port
int count = 0;                                            // counter for buffer array

//buttons
int zero = 34;
int one = 35;
int two = 36;
int three = 37;
int four = 38;
int five = 39;
int six = 48;
int seven = 49;
int eight = 50;
int nine = 51;
int call = 52;                                            // set pin 41 as the button read to intiate call
int ans_hang = 53;                                        // set pin 53 as the button to answer/hangup

//Display
#include <LiquidCrystal.h>
int RS=22;
int E=23;
int D4=24;
int D5=25;
int D6=26;
int D7=27;
LiquidCrystal lcd(RS, E, D4, D5, D6, D7);           // initialize the library with the numbers of the interface pins

int c = 1;

void setup()
{
  Serial1.begin(19200);              // baud to GPRS   
  Serial.begin(19200);               // baud to arduino
  pinMode(34, INPUT);
  pinMode(35, INPUT);
  pinMode(36, INPUT);
  pinMode(37, INPUT);
  pinMode(38, INPUT);
  pinMode(39, INPUT);
  pinMode(48, INPUT);
  pinMode(49, INPUT);
  pinMode(50, INPUT);
  pinMode(51, INPUT);
  pinMode(52, INPUT);
  pinMode(53, INPUT);
  lcd.begin(16, 2);
  delay(100);
  lcd.clear();
  delay(1000);
}
 
void loop()
{
  if (Serial1.available())              // if date is comming from softwareserial port ==> data is comming from Serial1 shield
  {
    while(Serial1.available())          // reading data into char array 
      {
      buffer[count++]=Serial1.read();     // writing data into array
      if(count == 64)break;
      }
    Serial.write(buffer,count);            // if no data transmission ends, write buffer to hardware serial port
    clearBufferArray();                // call clearBufferArray function to clear the storaged data from the array
    count = 0;                         // set counter of while loop to zero
  }
  if (Serial.available())               // if data is available on hardwareserial port ==> data is comming from PC or notebook
    Serial1.write(Serial.read());       // write it to the Serial1 shield
  if (digitalRead(zero)==HIGH)
    while (c==1) {
      lcd.print("0");
      c++;}
  if (digitalRead(one)==HIGH)
    while (c==1) {
      lcd.print("1");
      c++;}
  if (digitalRead(two)==HIGH)
    while (c==1) {
      lcd.print("2");
      c++;}
  if (digitalRead(three)==HIGH)
    while (c==1) {
      lcd.print("3");
      c++;}
  if (digitalRead(four)==HIGH)
    while (c==1) {
      lcd.print("4");
      c++;)
  if (digitalRead(five)==HIGH)
    while (c==1) {
      lcd.print("0");
      c++;}
  if (digitalRead(six)==HIGH)
    while (c==1) {
      lcd.print("6");
      c++;}
  if (digitalRead(seven)==HIGH)
    while (c==1) {
      lcd.print("7");
      c++;}
  if (digitalRead(eight)==HIGH)
    while (c==1) {
      lcd.print("8");
      c++;}
  if (digitalRead(nine)==HIGH)
    while (c==1) {
      lcd.print("9");
      c++;}
  if (digitalRead(call)==HIGH)
    while (c==1) {
      lcd.print("call");
      c++;}
  if (digitalRead(ans_hang)==HIGH)
    while (c==1) {
      lcd.print("ans_hang");
      c++;}
}
void clearBufferArray()              // function to clear buffer array
{
  for (int i=0; i<count; i++)
    { buffer[i]=NULL;}                  // clear all index of array with command NULL
}

