// ESP32 I2C Scanner
// Based on code of Nick Gammon  http://www.gammon.com.au/forum/?id=10896
// ESP32 DevKit - Arduino IDE 1.8.5
// Device tested PCF8574 - Use pullup resistors 3K3 ohms !
// PCF8574 Default Freq 100 KHz 

#include <Arduino.h>

//#include <Wire.h>

// void setup()
// {
//   Serial.begin (115200);  
//   Wire.begin (26, 32);   // sda= GPIO_21 /scl= GPIO_22
// }

// void Scanner ()
// {
//   Serial.println ();
//   Serial.println ("I2C scanner. Scanning ...");
//   byte count = 0;

//   Wire.begin();
//   for (byte i = 8; i < 120; i++)
//   {
//     Wire.beginTransmission (i);          // Begin I2C transmission Address (i)
//     if (Wire.endTransmission () == 0)  // Receive 0 = success (ACK response) 
//     {
//       Serial.print ("Found address: ");
//       Serial.print (i, DEC);
//       Serial.print (" (0x");
//       Serial.print (i, HEX);     // PCF8574 7 bit address
//       Serial.println (")");
//       count++;
//     }
//   }
//   Serial.print ("Found ");      
//   Serial.print (count, DEC);        // numbers of devices
//   Serial.println (" device(s).");
// }

// void loop()
// {
//   Scanner ();
//   delay (100);
// }

#include <M5Atom.h>
#include "MFRC522_I2C.h"

MFRC522 mfrc522(0x28);   // Create MFRC522 instance.  创建MFRC522实例

void setup() {
  //M5.begin(); //Init M5Atom.  初始化M5Atom
  M5.begin(true, false, true);    //Init Atom-Matrix(Initialize serial port, LED).  初始化 ATOM-Matrix(初始化串口、LED点阵)
  delay(50);   //delay 50ms.  延迟50ms
  M5.dis.drawpix(0, 0xffffff);    //Light the LED with the specified RGB color 00ff00(Atom-Matrix has only one light).  以指定RGB颜色0x00ff00点亮第0个LED

  //GPIO21 for dia.12 buzzer
  //GPIO25 for dia.9 buzzer  
  pinMode(21, OUTPUT);
  pinMode(25, OUTPUT);
  digitalWrite(21, HIGH);
  digitalWrite(25, HIGH);
  delay(100);
  digitalWrite(21, LOW);
  digitalWrite(25, LOW);


  //Serial.begin(115200); //Init M5Atom.  初始化M5Atom
  Serial.println("\n\nMFRC522 Test");
  Wire.begin(26,32);  //Initialize pin 26,32.  初始化26,32引脚

  mfrc522.PCD_Init(); // Init MFRC522.  初始化 MFRC522
  Serial.println("Please put the card");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()) {  //如果没有读取到新的卡片
    delay(200);
    return;
  }
  pinMode(21, OUTPUT);
  pinMode(25, OUTPUT);
  digitalWrite(21, HIGH);
  digitalWrite(25, HIGH);
  delay(100);
  digitalWrite(21, LOW);
  digitalWrite(25, LOW);  
  Serial.print("UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println("");
}