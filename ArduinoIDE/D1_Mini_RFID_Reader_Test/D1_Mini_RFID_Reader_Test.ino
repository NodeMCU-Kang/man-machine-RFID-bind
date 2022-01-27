//#define test_LED
//#define use_u8g2
//#define test_WiFi

#define blueLED   3 //RX
#define greenLED D1
#define redLED   D2 // same as beep
#define beepPin  D2

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#if defined(use_u8g2)
#include <U8g2lib.h> // 0.96" SSD1306-based OLED display
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0, 
  0, // clock=GPIO0=D3 in D1-Mini
  2, // data =GPIO2=D4 in D1-Mini
  U8X8_PIN_NONE
); // All Boards without Reset of the Display
#endif

#include <SPI.h>
// MRFC522
#include <MFRC522.h>
#define RST_PIN         D0 //D3       // Configurable, see typical pin layout above
#define SS_PIN          D8 //D4       // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

ESP8266WiFiMulti WiFiMulti;

void setup() {

  pinMode(beepPin, OUTPUT); 

  // beep--- beep beep
  digitalWrite(beepPin, HIGH);   
  delay(300);
  digitalWrite(beepPin, LOW);
  delay(100);
  digitalWrite(beepPin, HIGH);   
  delay(100);
  digitalWrite(beepPin, LOW);
  delay(100);
  digitalWrite(beepPin, HIGH);   
  delay(100);
  digitalWrite(beepPin, LOW);
 
  
#if defined(test_LED)   
  // LED test
  //********************* CHANGE PIN FUNCTION **********************
  pinMode(redLED, FUNCTION_3); //GPIO 3 swap the pin from RX to a GPIO.
  //****************************************************************
  pinMode(redLED, OUTPUT);    
  pinMode(greenLED,OUTPUT);
  pinMode(blueLED,OUTPUT);
  digitalWrite(greenLED, HIGH); 
  delay(1000);            // waits for a second
  digitalWrite(greenLED, LOW);  
  //digitalWrite(blueLED, HIGH); 
  digitalWrite(redLED, HIGH);  
  delay(1000);            // waits for a second
  digitalWrite(blueLED, LOW); 
  digitalWrite(redLED, LOW);  
  delay(1000);  
#endif  
        
#if defined(use_u8g2)
  u8g2.begin();
  u8g2.enableUTF8Print(); 
  u8g2.setFont(u8g2_font_unifont_t_chinese2);
  u8g2.setFontMode(1);  /* activate transparent font mode */

  u8g2.clearBuffer();
  u8g2.setCursor(0, 13);   
  u8g2.print("Testing Line #1");  
  u8g2.setCursor(0, 29);   
  u8g2.print("Testing Line #2");  
  u8g2.setCursor(0, 45);   
  u8g2.print("Testing Line #3");  
  u8g2.setCursor(0, 61);   
  u8g2.print("Testing Line #4");  

  u8g2.sendBuffer();
#endif    

  //********************* CHANGE PIN FUNCTION **********************
  //pinMode(redLED, FUNCTION_0); //GPIO 3 swap the pin to a RX.
  //****************************************************************
  
  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();

  Serial.println("Source: MAQ D:\\WebApp Projects\\ArduinoDevices\\P02-人機綁定\\man-machine-RFID-bind\\D1_Mini_RFID_Reader_Test");  
  Serial.println();

  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);       // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));     

#if defined(test_WiFi) 
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("TCRD4G", "cOUWUnWPU1");
  WiFiMulti.addAP("abc", "12345678");  
#endif  

}

long times=0;
void loop() {

//  //********************* CHANGE PIN FUNCTION **********************
//  pinMode(redLED, FUNCTION_3); //GPIO 3 swap the pin from RX to a GPIO.
//  //****************************************************************
//  pinMode(redLED, OUTPUT); 
//  digitalWrite(redLED, HIGH);   
//  delay(1000);
//  digitalWrite(redLED, LOW);
//  delay(1000);  
//  
//  Serial.begin(115200);  

  // ***** re-init for cheap RFID cards
  mfrc522.PCD_Init();
  delay(4);  
  // *****
    
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent()) {

    
    // Read A0
    //analogValue=analogRead(A0);
//    if (analogValue <1000){
//      Serial.println(analogRead(A0));   
//      return;
//    }
       
  } else {
    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial()) {
      // Read A0
      Serial.println(analogRead(A0));      
      return;
    }

    // short beep
    digitalWrite(beepPin, HIGH);   
    delay(100);
    digitalWrite(beepPin, LOW);
    
    // Dump debug info about the card; PICC_HaltA() is automatically called
    //mfrc522.PICC_DumpToSerial(&(mfrc522.uid)); 

    //Serial.println("******** times *********");
    //Serial.println(++times);
    //Serial.println("******** times *********");

    // 顯示卡片內容
    Serial.print(F("Card UID:"));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // 顯示卡片的UID
    Serial.println();
    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));  //顯示卡片的類型

    mfrc522.PICC_HaltA();  // 卡片進入停止模式
    delay(500);

#if defined(test_WiFi) 
    // wait for WiFi connection
    if ((WiFiMulti.run() == WL_CONNECTED)) {
  
      WiFiClient client;
  
      HTTPClient http;
  
      Serial.print("[HTTP] begin...\n");
      //if (http.begin(client, "http://jigsaw.w3.org/HTTP/connection.html")) {  // HTTP
      if (http.begin(client, "http://charder-weight-api.herokuapp.com/")) {  // HTTP
  
  
        Serial.print("[HTTP] GET...\n");
        // start connection and send HTTP header
        int httpCode = http.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTP] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
  
        http.end();
      } else {
        Serial.printf("[HTTP} Unable to connect\n");
      }
    }
#endif
    
  }
    
  //delay(30000);
}

/**
 * 這個副程式把讀取到的UID，用16進位顯示出來
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
