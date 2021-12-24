#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

#include <SPI.h>
// MRFC522
#include <MFRC522.h>
#define RST_PIN         D0 //D3       // Configurable, see typical pin layout above
#define SS_PIN          D8 //D4       // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

ESP8266WiFiMulti WiFiMulti;

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();

  Serial.println("Source: MAQ D:\WebApp Projects\ArduinoDevices\P02-人機綁定\man-machine-RFID-bind\D1_Mini_RFID_Reader");  
  Serial.println();

  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);       // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));     

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("TCRD4G", "cOUWUnWPU1");
  WiFiMulti.addAP("abc", "12345678");  

}

long times=0;
void loop() {
  // *****
  // re-init for cheap RFID cards
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

    // Dump debug info about the card; PICC_HaltA() is automatically called
    mfrc522.PICC_DumpToSerial(&(mfrc522.uid)); 

    Serial.println("******** times *********");
    Serial.println(++times);
    Serial.println("******** times *********");
            
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
  }
    
  //delay(30000);
}
