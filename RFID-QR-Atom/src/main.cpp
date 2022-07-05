// 2022-06 Paul Kang
// 使用 串口屏 做為 QR-RFID 配對站
// - 讀取 RFID，顯示 QR
// - 不用 WiFi

#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h> //參考 https://www.twblogs.net/a/5c343c54bd9eee35b3a5404e

#include <M5Atom.h>
//M5 ATOM-Lite 用的是 WS2812B 或 SK6812 這種 三合一的 全彩LED
//但 R, G, B 的亮度差異滿多，G>>B>R
//除了 Red, Green, Blue, White 這種純色外，其他調色要調整比重
//例如 0xffff00 的黃色會變成亮綠，改用 0xff3000 卻比較有明顯橘色
//M5.dis.drawpix(0, 0xff3000); //set LED orange 
#define GREEN   0x00ff00 //調低 0x001100 可省電
#define RED     0xff0000
#define BLUE    0x0000ff
#define WHITE   0xffffff
#define ORANGE  0xff3000
#define MAGENTA 0xff00ff
#define BLACK   0x000000
#define LED_OFF 0x000000

#include <ArduinoJson.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "MFRC522_I2C.h"
MFRC522 mfrc522(0x28);   // Create MFRC522 instance. 

//define GPIO39 for button 
#define buttonPin 39

HardwareSerial serialTFT(1); //使用 Hardware Serial 1

int incomingByte = 0; // for incoming serial data
char cmd[100]   ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char status[100]="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
bool statusForProcess=false;
int  status_index=0;
byte eepromFlag; 
bool special_mode;

// Strings stored in EEPROM
String eGroupID="G-0001";
String eSiteID ="S-0001";

void(* resetFunc) (void) = 0; //declare reset function @ address 0
void   writeStringToEEPROM (int addrOffset, const String &strToWrite);
String readStringFromEEPROM(int addrOffset);
void   checkStatus();
void   beep();
void   setMakeQR();
void   setGroupID(String text);
void   setSiteID (String text);
void   setCardID (String text);
String H8ToD10(byte *buffer, byte bufferSize);

void setup() {
  //Serial.begin(115200);
  M5.begin(true, true, true);    // M5Atom.h 裡 void begin(bool SerialEnable = true, bool I2CEnable = true, bool DisplayEnable = false);
                                  // 由於我們不使用 Atom-lite 預設的 25(SDA), 21(SCL) ，所以 I2CEnable 設為 false
  Wire.begin(26,32);              // 使用 HY2.0 的 pins 作為 I2C    26(SDA), 32(SCL). Even I2CEnable is set to true, Overwrite it.                                 

  //Notify for booting up
  M5.dis.drawpix(0, RED);    //RED for start up

  //button GPIO - pressed when boot up t0 force to use bridge AP mode
  pinMode(buttonPin, INPUT_PULLUP);
  special_mode = (digitalRead(39)==LOW); // when button is pressed during boot, for special mode

  Serial.print("Button GPIO39 status: ");
  Serial.println(getCpuFrequencyMhz());
  Serial.println();  

  // hardware serial begin parameters: 
  //  unsigned long baud, 
  //  uint32_t config=SERIAL_8N1, 
  //  int8_t rxPin=-1, 
  //  int8_t txPin=-1, 
  //  bool invert=false, 
  //  unsigned long timeout_ms = 20000UL);
  serialTFT.begin(115200, SERIAL_8N1, /*RX*/ 21, /*TX*/ 25);

  Serial.println("Source: MAQ D:\\WebApp Projects\\ArduinoDevices\\P02-人機綁定\\man-machine-RFID-bind\\RFID-QR-Atom");  
  Serial.println("Github: https://github.com/NodeMCU-Kang/man-machine-RFID-bind");  
  Serial.println();

  Serial.println("\nMFRC522 Init");
  //mfrc522.PCD_Init(); // Init MFRC522, 若沒連接 RFID 模組，程式會 hang 住 

  // Init EEPROM and test byte 0
  EEPROM.begin(4096);     
  eepromFlag = EEPROM.read(0); 
  Serial.println(eepromFlag, HEX);
  if (eepromFlag!=0x55) {
    Serial.println("EEPROM is not valid");
    EEPROM.write(0, 0x55);
    writeStringToEEPROM(16, eGroupID);
    writeStringToEEPROM(32, eSiteID);     
    EEPROM.commit();
  }

  Serial.println("Read GroupID/SiteID from EEPROM");
  eGroupID= readStringFromEEPROM(16);
  eSiteID = readStringFromEEPROM(32);  
  Serial.println(eGroupID);
  Serial.println(eSiteID);

  M5.dis.drawpix(0, GREEN); 
  Serial.println("Ready");  

  delay(1000); // 等待 串口屏 啟動，視情況調整 delay 時間 

  setGroupID(eGroupID);
  setSiteID(eSiteID);
  setCardID("UCM"); setMakeQR();
}

void loop() {

  checkStatus();

  // while (Serial.available() > 0) {
  //   // read the incoming byte:
  //   incomingByte = Serial.read();

  //   // say what you got:
  //   Serial.print("I received from Serial: ");
  //   Serial.println(incomingByte, HEX);
  //   serialTFT.print(cmd);
  // }  

  delay(100);

  // re-init for cheap RFID cards
  mfrc522.PCD_Init();
  delay(4);  
    
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent()) { 
  } else {
    if ( ! mfrc522.PICC_ReadCardSerial()) { 
      return;
    }

    M5.dis.drawpix(0, ORANGE); 

    beep(); // short beep to acknowdge RFID card is read

    String uid = H8ToD10(mfrc522.uid.uidByte, mfrc522.uid.size);
    mfrc522.PICC_HaltA();  // 卡片進入停止模式，以接受下一張卡片       

    Serial.println(uid);
    setCardID(uid); 
    setMakeQR();

    M5.dis.drawpix(0, GREEN); 

    Serial.println();
    Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));      
  }  
}

// EEPROM write/read string routines
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
}

String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0'; // !!! NOTE !!! Remove the space between the slash "/" and "0" (I've added a space because otherwise there is a display bug)
  return String(data);
}
// End EEPROM write/read string routines


// Atom sends to set GroupID to G-0001:           EE B1 10 00 00 00 02 47 2D 30 30 30 31 FF FC FF FF 
// Atom receives when user set GroupID as G-0002: EE B1 11 00 00 00 02 11 47 2D 30 30 30 32 00 FF FC FF FF 
// Atom sends to set SiteID to S-0001:            EE B1 10 00 00 00 03 53 2D 30 30 30 31 FF FC FF FF 
// Atom receives when user set SiteID as S-0002:  EE B1 11 00 00 00 03 11 53 2D 30 30 30 32 00 FF FC FF FF 
// Atom sends self-define to call make_qr():      EE B5 00 11 FF FC FF FF

// 蜂鸣器控制 指令格式：EE【 61  Time 】FF FC FF FF 参数说明： Time (1 个字节): 蜂鸣器讯响的时间，单位 10ms 
// 该指令用于蜂鸣器的控制，通过设定 Time 参数实现不同频率的讯响。一般触摸讯响时 间 Time 设置为 100ms

// 复位设备 EE 07 35 5A 53 A5
// 主机在运行的过程中通过串口指令来复位设备。建议主机初始 
// 化设备时增加该指令，以便主机意外复位后，设备也跟着复位。  
// 返回指令格式：EE 07 FF FC FF FF 

void beep(){
  cmd[ 0]=0xEE; cmd[ 1]=0x61;  // cmd header
  cmd[ 2]=0x0A;                // 10ms x 10 = 100ms
  cmd[ 3]=0xFF; cmd[ 4]=0xFC; 
  cmd[ 5]=0xFF; cmd[ 6]=0xFF; 
 
  for (int i=0; i<8; i++) serialTFT.print(cmd[i]);    
}

void setMakeQR(){
  cmd[ 0]=0xEE; cmd[ 1]=0xB5;  // cmd header
  cmd[ 2]=0x00; cmd[ 3]=0x11; 
  cmd[ 4]=0xFF; cmd[ 5]=0xFC; 
  cmd[ 6]=0xFF; cmd[ 7]=0xFF; 
 
  for (int i=0; i<8; i++) serialTFT.print(cmd[i]);  
}

void setGroupID(String group_id){
  int group_id_length=group_id.length();

  cmd[ 0]=0xEE; cmd[ 1]=0xB1;  // cmd header
  cmd[ 2]=0x10;                // set text control
  cmd[ 3]=0x00; cmd[ 4]=0x00;  // screen id         
  cmd[ 5]=0x00; cmd[ 6]=0x02;  // control id for GroupID
 
  // text to set
  for (int i=0; i<group_id_length; i++){
    cmd[i+7]=group_id[i];
  } 

  //cmd[ 7]=0x47; cmd[ 8]=0x2D; cmd[ 9]=0x30; cmd[10]=0x30; cmd[11]=0x30; cmd[12]='A'; // text to set

  // cmd tail
  cmd[group_id_length+7]=0xFF; cmd[group_id_length+8]=0xFC; cmd[group_id_length+9]=0xFF; cmd[group_id_length+10]=0xFF; 

  for (int i=0; i<(group_id_length+11);i++) serialTFT.print(cmd[i]);  
}

void setSiteID(String site_id){
  int site_id_length=site_id.length();

  cmd[ 0]=0xEE; cmd[ 1]=0xB1;  // cmd header
  cmd[ 2]=0x10;                // set text control
  cmd[ 3]=0x00; cmd[ 4]=0x00;  // screen id         
  cmd[ 5]=0x00; cmd[ 6]=0x03;  // control id for SiteID
 
  // text to set
  for (int i=0; i<site_id_length; i++){
    cmd[i+7]=site_id[i];
  } 

  // cmd tail
  cmd[site_id_length+7]=0xFF; cmd[site_id_length+8]=0xFC; cmd[site_id_length+9]=0xFF; cmd[site_id_length+10]=0xFF; 

  for (int i=0; i<(site_id_length+11);i++) serialTFT.print(cmd[i]);  
}

void setCardID(String card_id){
  int card_id_length=card_id.length();

  cmd[ 0]=0xEE; cmd[ 1]=0xB1;  // cmd header
  cmd[ 2]=0x10;                // set text control
  cmd[ 3]=0x00; cmd[ 4]=0x00;  // screen id         
  cmd[ 5]=0x00; cmd[ 6]=0x04;  // control id for CardID
 
  // text to set
  for (int i=0; i<card_id_length; i++){
    cmd[i+7]=card_id[i];
  } 

  // cmd tail
  cmd[card_id_length+7]=0xFF; cmd[card_id_length+8]=0xFC; cmd[card_id_length+9]=0xFF; cmd[card_id_length+10]=0xFF; 

  for (int i=0; i<(card_id_length+11);i++) serialTFT.print(cmd[i]);  
}

void checkStatus(){
  while (serialTFT.available() > 0) {
    // read the incoming byte:
    incomingByte = serialTFT.read();
    if (incomingByte!=0xEE && status_index==0) return; //invalid header, skip to prevent hangup;

    // say what you got:
    // Serial.print("I received from serialTFT: ");
    // Serial.print(incomingByte, HEX);
    // Serial.print(' ');

    status[status_index++]=incomingByte;
    statusForProcess=true;
  }

  int text_length=0;
  if (statusForProcess) {
    Serial.println(" ");
    for (int i=0; i<status_index; i++) {
      if (i>8 && status[i]==0) text_length=i-8;
      Serial.print(status[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");
    Serial.println(text_length);
    String text="";
    for (int i=0; i<text_length;i++) text.concat(status[i+8]);
    Serial.println(text);    

    if (status[2]==0x11) { // text control
      switch(status[6]) {
        case 2:
          Serial.println("GroupID set");
          writeStringToEEPROM(16, text);    
          EEPROM.commit();          
          break;
        case 3:
          Serial.println("SiteID set");
          writeStringToEEPROM(32, text);    
          EEPROM.commit();                 
          break;
        case 4:
          Serial.println("CardID set");
          setMakeQR();
          break;
        default:
          Serial.println("Invalid control id");
      }      
    }
    
    statusForProcess=false;
    status_index=0;
  }
}

String H8ToD10(byte *buffer, byte bufferSize) {
  unsigned long uid=0;
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    uid = uid + (buffer[i] << (8*i));
  }
  Serial.println();
  return String(uid);
}


