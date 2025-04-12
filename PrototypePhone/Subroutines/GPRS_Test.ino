//Serial Relay - Arduino will act as a go between for the
//serial link between the computer and the GPRS Shield
//at 19200 bps 8-N-1
//Computer is connected to Hardware/Serial UART
//GPRS Shield is connected to the Hardware/Serial1 UART



void setup() {
  Serial.begin(19200);           // the Serial port of Arduino baud rate
  Serial1.begin(19200);          // the GPRS baud rate
}

void loop() {
  while (Serial1.available())
    Serial.write(Serial1.read());
    
  while (Serial.available())
    Serial1.write(Serial.read());
}
