#define ver "v1.1" // 增加顯示 RSSI
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
#define blueLED   D4
#define greenLED  D1
#define redLED    D3
#define breathLED greenLED
#define rfidLED   blueLED
#define errorLED  redLED
#define beepPin   D2

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

// define which API URL to use
//#define aws_rfid_err  //for test
#if defined(aws_rfid_err)  // for test
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/";
#else
const char* apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid";
//const char* apiURL = "https://api-for-sql.herokuapp.com/test";
#endif

String apiHttpsGet(const char* apiURL);
bool   apiHttpsPost(const char* apiURL, String rfid_uid);
String H8ToD10(byte *buffer, byte bufferSize);
void beep();
void beeep();
void errorBeep();
void writeStringToEEPROM(int addrOffset, const String &strToWrite);
String readStringFromEEPROM(int addrOffset);
int checkApAPI(int timeoutTick);
void(* resetFunc) (void) = 0; //declare reset function @ address 0

unsigned long lastTime = 0;
String aSSID="";                   // 場域 AP SSID
String aPWD="";                    // 場域 AP PWD

char json_input[100];
DeserializationError json_error;
const char* json_element;
StaticJsonDocument<200> json_doc; 
String apiReturn; 

unsigned int max_delay = 0;
byte errorTimes = 5; // 錯誤次數
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

  Serial.println(WiFi.macAddress());

  // Init SPI and MFRC522
  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);             // Optional delay. Some board do need more time after init to be ready, see Readme

  // Check EEPROM
  EEPROM.begin(4096); 
  byte eepromFlag;                // eeprom Flag: 十六進制 55 表示有效
  eepromFlag = EEPROM.read(0); 

  // if eepromFlag !=0x55, connect to the bridge AP, "abc"/"12345678"
  // set mobile phone’s hotspot as "abc"/"12345678"
  if (eepromFlag !=0x55) {
    Serial.print("No SSID in EEPROM, Connect to bridge_AP: ");
    Serial.println(bridge_ssid); 

    digitalWrite(redLED,   HIGH);   
    if (checkApAPI(50)==0){
      digitalWrite(redLED,   LOW);
      EEPROM.write(0,0x55);
      writeStringToEEPROM(1, aSSID);
      writeStringToEEPROM(1+aSSID.length()+1, aPWD);  
      EEPROM.commit();       
      WiFi.disconnect();
    } else {
      resetFunc(); //reset
    }   
  } else { // if eepromFlag==55, read aSSID/aPWD from EEPROM
    Serial.println("Check bridge API for update EEPROM");

    digitalWrite(redLED,   HIGH);
    digitalWrite(blueLED,   HIGH);
    if (checkApAPI(20)!=0){
      digitalWrite(redLED,   LOW);
      digitalWrite(blueLED,   LOW);
      Serial.println("Read SSID/PWD from EEPROM");
      aSSID =  readStringFromEEPROM(1);
      aPWD =  readStringFromEEPROM(1+aSSID.length()+1); 
    } else {
      digitalWrite(redLED,   LOW);
      digitalWrite(blueLED,   LOW);
      EEPROM.write(0,0x55);
      writeStringToEEPROM(1, aSSID);
      writeStringToEEPROM(1+aSSID.length()+1, aPWD);  
      EEPROM.commit();       
      WiFi.disconnect();      
    }
  }
   
  Serial.printf("SSID: %s, PWD: %s\n", aSSID.c_str(), aPWD.c_str());

  beep(); delay(200); beep();

  //WiFi.mode(WIFI_STA); // 本來以為一定要執行，但好像預設就是 Station mode

  WiFi.begin(aSSID, aPWD);
  Serial.printf("Connecting to AP/Router %s...\n", aSSID.c_str()); 

  wifi_set_sleep_type(NONE_SLEEP_T); // 不要讓 WiFi 做 sleep  

  digitalWrite(blueLED, HIGH);
  int iTimeout=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.printf("%d.", iTimeout);
    if (iTimeout++==50) {
      resetFunc();    
      break;
    }
  }
  digitalWrite(blueLED, LOW);
  beep(); delay(200); beep(); delay(200); beep();

  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // 這裡設定 apiHttpsPost()用的 httpsClient 和 https 只有一次初始化，節省時間

  //方法1: httpsClient 使用 pointer 
  //httpsClient->setInsecure(); 

  //方法2: httpsClient 不使用 pointer (我比較習慣，也跟 https 一致)
  httpsClient.setInsecure(); 
  
  https.setTimeout(20000);  

  digitalWrite(greenLED, HIGH);
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

  //第一次呼叫 apiHttpPost() 會花比較久時間，先呼叫來縮短後續的呼叫時間
  lastTime = millis();
  Serial.println(apiURL);
  digitalWrite(greenLED, HIGH);
  if (apiHttpsPost(apiURL, "test")){
    Serial.printf("API successed in %d\n", millis() - lastTime);
    beep(); delay(200); beep(); delay(200); beep(); delay(200); beep();       
  }else {
    Serial.printf("API failed in %d\n", millis() - lastTime);             
    digitalWrite(breathLED, LOW);                
    digitalWrite(rfidLED, LOW);                
    digitalWrite(errorLED, HIGH); 
    errorBeep();
    if (--errorTimes == 0) {
        resetFunc();
    }        
  }

  //digitalWrite(greenLED, LOW);

  Serial.println();  
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));  
  lastTime = millis();
  max_delay =0;
}

byte i=0;  // 呼吸燈指標
int coffee=0;
int max_rssi=-100;
int min_rssi=0;
void loop() {
  if (WiFi.status() == WL_CONNECTED) {

    int rssi = WiFi.RSSI();
    if (rssi > max_rssi) {
      max_rssi = rssi;
    }
    if (rssi < min_rssi) {
      min_rssi = rssi;
    }
    //Serial.printf("RSSI now:%d, MAX RSSI:%d, MIN RSSI:%d \n", rssi, max_rssi, min_rssi);   

    if (rssi< -70) {
      digitalWrite(errorLED, HIGH);
      digitalWrite(rfidLED, HIGH);
      digitalWrite(greenLED, LOW);
    } else {          
      digitalWrite(errorLED, LOW);
      digitalWrite(rfidLED, LOW);
      digitalWrite(greenLED, HIGH);
    }

    if (millis()-lastTime > 60000){ //refresh to against nodding
      //Serial.printf("Coffee at %d\n", millis());  
      Serial.printf("RSSI now:%d, MAX RSSI:%d, MIN RSSI:%d \n", rssi, max_rssi, min_rssi);       
      if (apiHttpsPost(apiURL, "test")){
        //Serial.printf("API successed in %d\n", usedTime); //millis() - lastTime);
        //beep();
        //digitalWrite(breathLED, LOW); 
        digitalWrite(rfidLED, LOW);                
        digitalWrite(errorLED, LOW);         
        digitalWrite(greenLED, HIGH);         
      } else {
        Serial.printf("API failed in %d\n", millis() - lastTime);
        digitalWrite(breathLED, LOW);                
        digitalWrite(rfidLED, LOW);                
        digitalWrite(errorLED, HIGH); 
        errorBeep();
        if (--errorTimes == 0) {
           resetFunc();
        }        
        lastTime = millis();
      }  

    }  

    // *** 呼吸燈   
    // i++;
    // if (i%10==0){
    //   digitalWrite(breathLED, HIGH);    
    // } else {
    //   digitalWrite(breathLED, LOW);
    // }  
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

      //digitalWrite(breathLED, LOW); 
      digitalWrite(rfidLED, HIGH); 

      
      beep(); // short beep to acknowdge RFID card is read
      
      // Dump debug info about the card; PICC_HaltA() is automatically called
      //mfrc522.PICC_DumpToSerial(&(mfrc522.uid)); 
  
      // 顯示卡片內容
      //Serial.print(F("Card UID:"));
      //dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size); // 顯示卡片的UID
      String uid = H8ToD10(mfrc522.uid.uidByte, mfrc522.uid.size);
      //Serial.println(uid);
      //Serial.print(F("PICC type: "));
      //MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      //Serial.println(mfrc522.PICC_GetTypeName(piccType));  //顯示卡片的類型
  
      mfrc522.PICC_HaltA();  // 卡片進入停止模式，以接受下一張卡片       

      //Serial.println(apiURL);
      lastTime = millis();
      if (apiHttpsPost(apiURL, uid)){
        unsigned int usedTime = millis() - lastTime;
        //Serial.printf("API successed in %d\n", usedTime); //millis() - lastTime);
        beep();
        digitalWrite(rfidLED, LOW);         
        digitalWrite(errorLED, LOW);         
        digitalWrite(breathLED, HIGH);        
      }else {
        //Serial.printf("API failed in %d\n", millis() - lastTime);
        digitalWrite(breathLED, LOW);                
        digitalWrite(rfidLED, LOW);                
        digitalWrite(errorLED, HIGH); 
        errorBeep();
        if (--errorTimes == 0) {
           resetFunc();
        }
        //delay(3000);        
        //digitalWrite(errorLED, LOW);         
        
      }

      //Serial.println();
      //Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));      
      //delay(500);
      lastTime = millis();
    }

  } else {
    Serial.println("Networked is not connected");
    digitalWrite(greenLED, LOW);
    //delay(1000);
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
  
  //Serial.print("[HTTPS] begin...\n");
  //if (https.begin(*httpsClient, "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid")) {
  //if (https.begin(*httpsClient, apiURL)) { //使用 pointer 使用方式  
  if (https.begin(httpsClient, apiURL)) { //不使用 pointer 使用方式
    https.addHeader("Content-Type", "application/json");
  
    //String postBody = "{\"params\":{\"token_id\":\"rfid_token\",\"login_dict\":{\"rf_id\":\"2512255499\",\"wifi_mac\":\"48:3F:DA:49:2E:46\"}}}";
    String postBodyBegin = "{\"params\":{\"token_id\":\"rfid_token\",\"login_dict\":{\"rf_id\":\"";
    String postBodyEnd   = "\",\"wifi_mac\":\"48:3F:DA:49:2E:46\"}}}";
    String postBody = String(postBodyBegin + rfid_uid);
           postBody = String(postBody + postBodyEnd);
    //Serial.println(postBody);

    //Serial.print("[HTTP] POST...\n");
    lastTime = millis();
    int httpCode = https.POST(postBody);
    unsigned int apiTime = millis() - lastTime;
    if (apiTime >max_delay) max_delay = apiTime;

    //Serial.printf("%d, %d\n",coffee++,apiTime); // 統計用
    //Serial.printf("API Time:%d, Max_delay:%d\n", apiTime, max_delay);

    // httpCode will be negative on error
    if (httpCode > 0) {
      //Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK || 
          httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        //Serial.println(payload);
        success = true;
      }
    } else {
      //Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    //Serial.printf("[HTTPS] Unable to connect\n");
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

int checkApAPI(int timeoutTick){            
  // Connect to the Bridge AP
  WiFi.mode(WIFI_STA);
  WiFi.begin(bridge_ssid, bridge_password);

  Serial.printf("Connecting to bridge hotspot AP %s...\n", bridge_ssid); 

  int iTimeout=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.printf("%d.", iTimeout);
    if (iTimeout++==timeoutTick) {
      Serial.println();
      return -1; //timeout failure
    }
  }

  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  while (aSSID=="") {
    Serial.println("Get apAPI");
    apiReturn = apiHttpsGet(apAPI);  
    Serial.println(apiReturn);
    apiReturn.toCharArray(json_input,100);       
    json_error = deserializeJson(json_doc, json_input);
    if (!json_error) {
      json_element = json_doc["SSID"];
      aSSID = String(json_element);  
      json_element = json_doc["PWD"];
      aPWD = String(json_element);  

      // EEPROM.write(0,0x55);
      // writeStringToEEPROM(1, aSSID);
      // writeStringToEEPROM(1+aSSID.length()+1, aPWD);  
      // EEPROM.commit();                           
    } else return -2; //json error  
  }
  return 0; //success  
}
