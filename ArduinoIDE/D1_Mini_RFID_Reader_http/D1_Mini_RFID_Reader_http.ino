#include <Arduino.h>

#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <SPI.h>
// MRFC522
#include <MFRC522.h>
#define RST_PIN         D0 //D3       // Configurable, see typical pin layout above
#define SS_PIN          D8 //D4       // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN);     // Create MFRC522 instance

#define blueLED   3 //RX
#define greenLED D1
#define redLED   D2 // same as beep
#define beepPin  D2

//ESP8266WiFiMulti WiFiMulti;

long times=0;
WiFiClient client;
HTTPClient http;

#define MAC
//#define abc
//#define TCRD4G

#if defined(MAC)
const char* ssid = "MAC";
const char* password = "Kpc24642464";
#endif
#if defined(abc)
const char* ssid = "abc";
const char* password = "12345678";
#endif
#if defined(TCRD4G)
const char* ssid = "TCRD4G";
const char* password = "cOUWUnWPU1";
#endif

//#define jigsaw
//#define heroku
#define aws_rfid

#if defined(jigsaw)
const char* apiURL = "http://jigsaw.w3.org/HTTP/connection.html";
#endif
#if defined(heroku)
const char* apiURL = "http://charder-weight-api.herokuapp.com/";
#endif

#if defined(aws_rfid)
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid";
#endif

https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid

unsigned long lastTime = 0;

void setup() {

  pinMode(beepPin, OUTPUT); 

  // beep--- beep beep
  digitalWrite(beepPin, HIGH);   delay(300);
  digitalWrite(beepPin, LOW);    delay(100);
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);    delay(100);
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);  

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();

  Serial.println("Source: MAQ D:\WebApp Projects\ArduinoDevices\P02-人機綁定\man-machine-RFID-bind\D1_Mini_RFID_Reader");  
  Serial.println();

  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);             // Optional delay. Some board do need more time after init to be ready, see Readme
  
//  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  http.setTimeout(20000);
  
  Serial.printf("Connecting to AP/Router %s...\n", ssid); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // 用 iPhone AP 或 TCRD4G 都沒問題，
  // 但用家裡 TP-LINK AP，第一次呼叫 API 總是失敗，所以這裡做 retry
  // 雖然查清是 timeout 的問題，用 http.setTimeout(20000); 解決此問題。但留著 retry 作為保險
  for (int i=0; i<3; i++){
    lastTime = millis();
    if(apiHttpGet(apiURL)) {
      Serial.printf("delay %d\n", millis() - lastTime);
      break;
    }
    else {
      Serial.printf("delay %d\n", millis() - lastTime);
      Serial.printf("Calling API failed %d times\n", (i+1));
    }
  }

  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));  
}

void loop() {
  
  if (WiFi.status() == WL_CONNECTED) {
    
    // re-init for cheap RFID cards
    mfrc522.PCD_Init();
    delay(4);  
      
    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if ( ! mfrc522.PICC_IsNewCardPresent()) {
        
    } else {
      // Select one of the cards
      if ( ! mfrc522.PICC_ReadCardSerial()) {
        // Read A0
        // Serial.println(analogRead(A0));      
        return;
      }
  
      // short beep
      digitalWrite(beepPin, HIGH);   
      delay(100);
      digitalWrite(beepPin, LOW);
      
      // Dump debug info about the card; PICC_HaltA() is automatically called
      //mfrc522.PICC_DumpToSerial(&(mfrc522.uid)); 
  
      // 顯示卡片內容
      Serial.print(F("Card UID:"));
      dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // 顯示卡片的UID
      Serial.println();
      Serial.print(F("PICC type: "));
      MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      Serial.println(mfrc522.PICC_GetTypeName(piccType));  //顯示卡片的類型
  
      mfrc522.PICC_HaltA();  // 卡片進入停止模式

      // 用 iPhone AP 或 TCRD4G 都沒問題，
      // 但用家裡 TP-LINK AP，API 呼叫 API 有時會失敗，所以這裡做 retry   
      // 雖然查清是 timeout 的問題，用 http.setTimeout(20000); 解決此問題。但留著 retry 作為保險   
      for (int i=0; i<3; i++){
        lastTime = millis();
        if(apiHttpGet(apiURL)) {
          Serial.printf("delay %d\n", millis() - lastTime);
          break;
        }
        else {
          Serial.printf("delay %d\n", millis() - lastTime);
          Serial.printf("Calling API failed %d times\n", (i+1));
        }
      }
              
      Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));      
      delay(500);
    }

  } else {
    Serial.println("Networked is not connected");
    delay(1000);
  }
}

bool apiHttpGet(const char* apiURL){
  bool success=false;
  
  Serial.print("[HTTP] begin...\n");
  if (http.begin(client, apiURL)) {  // HTTP

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
        success = true;
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
  }  
}
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
