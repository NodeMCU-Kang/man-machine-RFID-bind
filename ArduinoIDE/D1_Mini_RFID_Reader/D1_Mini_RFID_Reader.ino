#include <Arduino.h>
#include <ESP8266WiFi.h>
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

// 這裡全域的 httpsClient 和 https 是給 apiHttpsPost()用的。
// 若宣告在 apiHttpsPost()裡，每次初始加上 setInsecure()和 setTimeout 會比全域多上 1~2 秒
// 從約 0.5s 增加為 1.5s~2.5s，嚴重影像使用者體驗。
std::unique_ptr<BearSSL::WiFiClientSecure>httpsClient(new BearSSL::WiFiClientSecure);
HTTPClient https;

// *** define which AP/Router to use
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
// ************************************

// *** define which API URL to use
//#define aws_rfid_err

#if defined(aws_rfid_err)
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/";
#else
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid";
#endif
// ************************************

bool apiHttpsPost(const char* apiURL, String rfid_uid);

unsigned long lastTime = 0;
long times=0;  // API retry times

void setup() {

  pinMode(greenLED, OUTPUT); 
  pinMode(beepPin, OUTPUT); 

  digitalWrite(greenLED, LOW);
  digitalWrite(beepPin, LOW);  

  powerUpBeep();   // beep--- beep beep

  Serial.begin(115200);
  Serial.println();

  Serial.println("Source: MAQ D:\WebApp Projects\ArduinoDevices\P02-人機綁定\man-machine-RFID-bind\D1_Mini_RFID_Reader");  
  Serial.println();

  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);             // Optional delay. Some board do need more time after init to be ready, see Readme
  
  //WiFi.mode(WIFI_STA); // 本來以為一定要執行，但好像預設就是 Station mode

  WiFi.begin(ssid, password);

  Serial.printf("Connecting to AP/Router %s...\n", ssid); 
  digitalWrite(greenLED, HIGH);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(greenLED, LOW);
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // 這裡設定 apiHttpsPost()用的 httpsClient 和 https 也是只有一次初始化，節省時間
  httpsClient->setInsecure(); 
  https.setTimeout(20000);  

  Serial.println("Get fw_version.json");
  apiHttpsGet("https://nodemcu-kang.github.io/man-machine-RFID-bind/ArduinoIDE/D1_Mini_RFID_Reader/fw_version.json");

  Serial.println();  
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));  
}

byte i=0;  // 呼吸燈指標
void loop() {

  delay(100);  
  if (WiFi.status() == WL_CONNECTED) {

    // *** 呼吸燈   
    i++;
    if (i%10==0){
      digitalWrite(greenLED, HIGH);    
    } else {
      digitalWrite(greenLED, LOW);
    }  
    // ****************************  
    
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

      digitalWrite(greenLED, HIGH); 
      
      shortBeep(); // short beep to acknowdge RFID card is read
      
      // Dump debug info about the card; PICC_HaltA() is automatically called
      //mfrc522.PICC_DumpToSerial(&(mfrc522.uid)); 
  
      // 顯示卡片內容
      Serial.print(F("Card UID:"));
      //dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // 顯示卡片的UID
      String uid = H8ToD10(mfrc522.uid.uidByte, mfrc522.uid.size);
      Serial.println(uid);
      //Serial.print(F("PICC type: "));
      //MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      //Serial.println(mfrc522.PICC_GetTypeName(piccType));  //顯示卡片的類型
  
      mfrc522.PICC_HaltA();  // 卡片進入停止模式

      // 用 iPhone AP 或 TCRD4G 都沒問題，
      // 但用家裡 TP-LINK AP，API 呼叫 API 有時會失敗，所以這裡做 retry   
      // 雖然查清是 timeout 的問題，用 http.setTimeout(20000); 解決此問題。但留著 retry 作為保險   
//      for (int i=0; i<3; i++){
//        lastTime = millis();
//        if(apiHttpsPost(apiURL, uid)) {
//          Serial.printf("delay %d\n", millis() - lastTime);
//          
//          break;
//        }
//        else {
//          Serial.printf("delay %d\n", millis() - lastTime);
//          Serial.printf("Calling API failed %d times\n", (i+1));
//        }
//      }

      lastTime = millis();
      Serial.println(apiURL);
      if (apiHttpsPost(apiURL, uid)){
        Serial.printf("API successed in %d\n", millis() - lastTime);
        shortBeep();
        digitalWrite(greenLED, LOW); 
      }else {
        Serial.printf("API failed in %d\n", millis() - lastTime);
        //delay(500);
        errorBeep();        
      }

      Serial.println();
      Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));      
      //delay(500);
    }

  } else {
    Serial.println("Networked is not connected");
    digitalWrite(greenLED, LOW);
    delay(1000);
  } 
}

bool apiHttpsGet(const char* apiURL){
  bool success=false;   

  // 為避免跟 apiHttpsPost 的 httpsClient 和 https 衝突，
  // apiHttpsGet 每次都會初始化本地的 httpsClient 和 https，以及 setInsecure()和 setTimeout
  // 雖然時間會久一點，但 apiHttpsGet 只有開始時呼叫，不影響使用者體驗 
  std::unique_ptr<BearSSL::WiFiClientSecure>httpsClient(new BearSSL::WiFiClientSecure);
  HTTPClient https;  
  httpsClient->setInsecure(); 
  https.setTimeout(20000);   
  
  Serial.print("[HTTPS] begin...\n");
  if (https.begin(*httpsClient, apiURL)) { 

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println(payload);
        success = true;
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
    return success;
  }  
}

bool apiHttpsPost(const char* apiURL, String rfid_uid){
  bool success=false;   
  
  Serial.print("[HTTPS] begin...\n");
  //if (https.begin(*httpsClient, "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid")) {
  if (https.begin(*httpsClient, apiURL)) {    

    https.addHeader("Content-Type", "application/json");
  
    //String postBody = "{\"params\":{\"token_id\":\"rfid_token\",\"login_dict\":{\"rf_id\":\"2512255499\",\"wifi_mac\":\"48:3F:DA:49:2E:46\"}}}";
    String postBodyBegin = "{\"params\":{\"token_id\":\"rfid_token\",\"login_dict\":{\"rf_id\":\"";
    String postBodyEnd   = "\",\"wifi_mac\":\"48:3F:DA:49:2E:46\"}}}";
    String postBody = String(postBodyBegin + rfid_uid);
           postBody = String(postBody + postBodyEnd);
    Serial.println(postBody);

    Serial.print("[HTTP] POST...\n");
    lastTime = millis();
    int httpCode = https.POST(postBody);
    Serial.print("API Time:");
    Serial.println(millis() - lastTime);

    // httpCode will be negative on error
    if (httpCode > 0) {
      Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK || 
          httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println(payload);
        success = true;
      }
    } else {
      Serial.printf("[HTTPS] POST... failed, error: %s\n", 
                     https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }  
  return success;
}
void dump_byte_array(byte *buffer, byte bufferSize) {
  unsigned long uid=0;
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    uid = uid + (buffer[i] << (8*i));
    Serial.println(uid, HEX);
  }
  Serial.println(uid);
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
void powerUpBeep(){
  // beep--- beep beep  
  digitalWrite(beepPin, HIGH);   delay(300);
  digitalWrite(beepPin, LOW);    delay(100);
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);    delay(100);
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);  
}
void shortBeep(){
  // beep
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);  
}

void errorBeep(){
  // beep-- beep beep beep beep--
  digitalWrite(beepPin, HIGH);   delay(1000);
  digitalWrite(beepPin, LOW);    delay(100);
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);    delay(100);  
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);    delay(100);
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);    delay(100);   
  digitalWrite(beepPin, HIGH);   delay(1000);
  digitalWrite(beepPin, LOW);  
}
