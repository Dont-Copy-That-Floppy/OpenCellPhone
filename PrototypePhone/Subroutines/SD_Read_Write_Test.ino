/*
  SD card read/write

 This example shows how to read and write data to and from an SD card file
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4

 created   Nov 2010
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe

 This example code is in the public domain.

 */

#include <SPI.h>
#include <SD.h>

File myFile;

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  Serial.print("Initializing SD card...");

  if (!SD.begin(SS)) {
    Serial.println("initialization failed!");
    while(!SD.begin(SS)) {}
  }
  Serial.println("initialization done.");
}

void loop()
{
  String write_Or_read;

  Serial.println("Would you like to write to file or read?");
  while (Serial.available() == 0) {}
  delay(100);
  for (int i = 0; Serial.available() > 0; i++) {
    write_Or_read += (char)Serial.read();
  }

  /*********************************************************************
   * WRITE FUNCTION
   *********************************************************************/

  if (write_Or_read == "write\r") {
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    String fileName;
    Serial.println("Please enter filename: ");
    while (Serial.available() == 0) {}
    delay(100);
    for (int i = 0; Serial.available() > 1; i++) {
      fileName += (char)Serial.read();
    }
    Serial.read();

    myFile = SD.open(fileName, FILE_WRITE);
input_data:
    // if the file opened okay, write to it:
    if (myFile) {
      Serial.print("Enter Data to be written to file\n");
      delay(100);
      char writeTo[10];
      while (Serial.available() == 0) {}
      delay(100);
      for (int i = 0; Serial.available() > 1; i++) {
        writeTo[i] = (char)Serial.read();
      }
      Serial.read();
      myFile.write(writeTo);
      // close the file:
      myFile.close();
      Serial.println("done.");
    } else {
      myFile.close();
      Serial.println("File " + fileName + " created");
      Serial.println("Write information to new file");
      goto input_data;
    }
  }

  /*********************************************************************
   * READ FUNCTION
   *********************************************************************/

  else if (write_Or_read == "read\r") {
    String fileName;
    Serial.println("Please enter filename: ");
    while (Serial.available() == 0) {}
    delay(100);
    for (int i = 0; Serial.available() > 1; i++) {
      fileName += (char)Serial.read();
    }
    Serial.read();
    // re-open the file for reading:
    myFile = SD.open(fileName);
    if (myFile) {
      Serial.println("Contents of " + fileName + " is:");

      // read from the file until there's nothing else in it:
      while (myFile.available()) {
        Serial.write(myFile.read());
      }
      // close the file:
      myFile.close();
      Serial.println();
    } else {
      // if the file didn't open, print an error:
      Serial.println("error opening " + fileName);
    }
  }

  /*********************************************************************
   * DELETE/REMOVE FUNCTION
   *********************************************************************/

  else if (write_Or_read == "delete\r") {
    String fileName;
    Serial.println("Please enter filename: ");
    while (Serial.available() == 0) {}
    delay(100);
    for (int i = 0; Serial.available() > 1; i++) {
      fileName += (char)Serial.read();
    }
    Serial.read();
    if (SD.exists(fileName)) {
      Serial.println("Deleting " + fileName);
      // delete file
      SD.remove(fileName);
    }
    else {
      Serial.println("" + fileName + " doesn't exist");
    }
  }

  /*********************************************************************
   * LIST FILES FUNCTION
   *********************************************************************/

  else if (write_Or_read == "ls\r") {
    File root;
    root = SD.open("/");
    printDirectory(root, 0);
  }

  /*********************************************************************
   * HELP FUNCTION
   *********************************************************************/

  else {
    Serial.println("Command Not recognized\n\nCurrent Command list:\nwrite\nread\ndelete\nls(list files)\n");
  }
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}


