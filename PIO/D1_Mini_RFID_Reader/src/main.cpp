#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
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

//Buzzer has capacitance, connected to D3 or D4, will cause booting unstable.
//So use D2 to drive buzzer 
#define blueLED  D4
#define greenLED D1
#define redLED   D3
#define beepPin  D2

// 這裡全域的 httpsClient 和 https 是給 apiHttpsPost()用的。
// 若宣告在 apiHttpsPost()裡，每次初始加上 setInsecure()和 setTimeout 會比全域多上 1~2 秒
// 從約 0.5s 增加為 1.5s~2.5s，嚴重影像使用者體驗。
//
//httpsClient 使用 pointer 的方法
//std::unique_ptr<BearSSL::WiFiClientSecure>httpsClient(new BearSSL::WiFiClientSecure);

//httpsClient 不使用 pointer 的方法 (我比較習慣，也跟 https 一致)
BearSSL::WiFiClientSecure httpsClient;

HTTPClient https;

// *** define which AP/Router to use
#define apAPI "https://deploy-ap-api.herokuapp.com/?getAP"
#define bridge_ssid     "abc"       // 跳板 AP SSID
#define bridge_password "12345678"  // 跳板 AP PWD

#define MAC
//#define TCRD4G

#if defined(MAC)
const char* ssid = "MAC";
const char* password = "Kpc24642464";
#endif
#if defined(TCRD4G)
const char* ssid = "TCRD4G";
const char* password = "cOUWUnWPU1";
#endif
// ************************************

// define which API URL to use
//#define aws_rfid_err

#if defined(aws_rfid_err)
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/";
#else
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid";
#endif
// ************************************

String apiHttpsGet(const char* apiURL);
bool   apiHttpsPost(const char* apiURL, String rfid_uid);
String H8ToD10(byte *buffer, byte bufferSize);
void beep();
void beeep();
void writeStringToEEPROM(int addrOffset, const String &strToWrite);
String readStringFromEEPROM(int addrOffset);

//StaticJsonDocument<200> json_doc;
//char json_input[100];
//DeserializationError json_error;

unsigned long lastTime = 0;
long times=0;  // API retry times

void setup() {

  pinMode(redLED,   OUTPUT);    digitalWrite(redLED,   LOW);
  pinMode(greenLED, OUTPUT);    digitalWrite(greenLED, LOW); 
  pinMode(blueLED,  OUTPUT);    digitalWrite(blueLED,  LOW);  
  pinMode(beepPin,  OUTPUT);    digitalWrite(beepPin,  LOW); 

  // beep once indicating power on
  beep();   

  // Init Serial
  Serial.begin(115200);
  Serial.println();
  Serial.println("Source: MAQ D:\\WebApp Projects\\ArduinoDevices\\P02-人機綁定\\man-machine-RFID-bind\\D1_Mini_RFID_Reader");  
  Serial.println();

  // Init SPI and MFRC522
  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);             // Optional delay. Some board do need more time after init to be ready, see Readme

  String aSSID="";                   // 場域 AP SSID
  String aPWD="";                    // 場域 AP PWD
    
  // Check EEPROM
  EEPROM.begin(4096); 
  byte eepromFlag;                // eeprom Flag: 十進制 55 表示有效
  eepromFlag = EEPROM.read(0); 

  char json_input[100];
  DeserializationError json_error;
  const char* json_element;
  StaticJsonDocument<200> json_doc; 
  String apiReturn;  

  // if eepromFlag !=0x55, connect to the bridge AP, ssid1/password1
  // set mobile phone’s hotspot as ssid1/password1
  if (eepromFlag !=0xAA) {
    Serial.print("No SSID in EEPROM, Connect to bridge_AP: ");
    Serial.println(bridge_ssid);
    digitalWrite(greenLED, LOW);  
    digitalWrite(redLED, HIGH);      
             
    // Connect to the Bridge AP
    WiFi.mode(WIFI_STA);
    WiFi.begin(bridge_ssid, bridge_password);

    Serial.printf("Connecting to bridge hotspot AP %s...\n", bridge_ssid); 
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(blueLED, HIGH);  
    while (aSSID=="") {
      Serial.println("Get apAPI");
      apiReturn = apiHttpsGet(apAPI);  
      Serial.println(apiReturn);
      apiReturn.toCharArray(json_input,100);       
      json_error = deserializeJson(json_doc, json_input);
      if (!json_error) {
        json_element = json_doc["SSID"];
        Serial.println(String(json_element)); 
        aSSID = String(json_element);  
        json_element = json_doc["PWD"];
        Serial.println(String(json_element)); 
        aPWD = String(json_element);  

        // EEPROM.write(0,0x55);
        // writeStringToEEPROM(1, aSSID);
        // writeStringToEEPROM(1+aSSID.length()+1, aPWD);  
        // EEPROM.commit();                           
      }  
    } 
    
    WiFi.disconnect();
       
  } else { // if eepromFlag==55, read aSSID/aPWD from EEPROM
    aSSID =  readStringFromEEPROM(1);
    Serial.printf("EEPROM SSID: %s\n", aSSID);
    aPWD =  readStringFromEEPROM(1+aSSID.length()+1); 
    Serial.printf("EEPROM PWD: %s\n", aPWD);
  }
   
  beep(); delay(500); beep();
  digitalWrite(redLED, LOW);   
  digitalWrite(blueLED, LOW); 
  digitalWrite(greenLED, HIGH);  

  //WiFi.mode(WIFI_STA); // 本來以為一定要執行，但好像預設就是 Station mode

  WiFi.begin(ssid, password);

  Serial.printf("Connecting to AP/Router %s...\n", ssid); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(greenLED, LOW);
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // 這裡設定 apiHttpsPost()用的 httpsClient 和 https 只有一次初始化，節省時間

  //httpsClient 使用 pointer
  //httpsClient->setInsecure(); 

  //httpsClient 不使用 pointer (我比較習慣，也跟 https 一致)
  httpsClient.setInsecure(); 
  https.setTimeout(20000);  

  Serial.println("Get apAPI");
  apiReturn = apiHttpsGet(apAPI);  
  Serial.println(apiReturn);
  apiReturn.toCharArray(json_input,100);       
  json_error = deserializeJson(json_doc, json_input);
  if (!json_error) {
    json_element = json_doc["SSID"];
    Serial.println(String(json_element));    
    json_element = json_doc["PWD"];
    Serial.println(String(json_element));           
  }    

  Serial.println("Get fw_version.json");
  apiReturn = apiHttpsGet("https://nodemcu-kang.github.io/man-machine-RFID-bind/ArduinoIDE/D1_Mini_RFID_Reader/fw_version.json");
  Serial.println(apiReturn);
  apiReturn.toCharArray(json_input,100);       
  json_error = deserializeJson(json_doc, json_input);
  if (!json_error) {
    json_element = json_doc["latestVersion"];
    Serial.println(String(json_element));    
    json_element = json_doc["Release"];
    Serial.println(String(json_element)); 
    json_element = json_doc["binName"];
    Serial.println(String(json_element));           
  }  

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
      
      beep(); // short beep to acknowdge RFID card is read
      
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

      lastTime = millis();
      Serial.println(apiURL);
      if (apiHttpsPost(apiURL, uid)){
        Serial.printf("API successed in %d\n", millis() - lastTime);
        beep();
        digitalWrite(greenLED, LOW); 
      }else {
        Serial.printf("API failed in %d\n", millis() - lastTime);
        //delay(500);
        //errorBeep();        
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

String apiHttpsGet(const char* apiURL){  

  // 為避免跟 apiHttpsPost 的 httpsClient 和 https 衝突，
  // apiHttpsGet 每次都會初始化本地的 httpsClient 和 https，以及 setInsecure()和 setTimeout
  // 雖然時間會久一點，但 apiHttpsGet 只有開始時呼叫，不影響使用者體驗 

  //這邊保留使用 pointer 使用方式
  std::unique_ptr<BearSSL::WiFiClientSecure>httpsClient(new BearSSL::WiFiClientSecure);
  HTTPClient https;  

  //這邊保留使用 pointer 使用方式
  httpsClient->setInsecure(); 
  https.setTimeout(20000);   

  String payload="";
  
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
        payload = https.getString();          
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      payload="";
    }

    https.end();
    //return payload;
  }  
  return payload;
}

bool apiHttpsPost(const char* apiURL, String rfid_uid){
  bool success=false;   
  
  Serial.print("[HTTPS] begin...\n");
  //if (https.begin(*httpsClient, "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid")) {
  //if (https.begin(*httpsClient, apiURL)) {    
  if (https.begin(httpsClient, apiURL)) { 
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

void beep(){
  // beep
  digitalWrite(beepPin, HIGH);   delay(100);
  digitalWrite(beepPin, LOW);  
}
void beeep(){
  // beep
  digitalWrite(beepPin, HIGH);   delay(300);
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

