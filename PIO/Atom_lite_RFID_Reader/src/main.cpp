// For displaying Chinese chars, don't use Chinese in comments

#include <Arduino.h>
#include <Wire.h> //for I2C

// #include "driver/adc.h"
#define ATOMOLED_TYPE 4   // 1:TALL, 2:SQUARE1, 3:SQUARE2, 4:RFID-BOX
#include "ATOMOLED_def.h" // instance U8G2 by ATOMOLED_TYPE

#include <M5Atom.h>
// M5 ATOM-Lite uses WS2812B or SK6812 3-in-1 full color LED
// The btightnesses of R, G, B are quite different, G>>B>R
// Fine tune color with weightings,
// e.g. 0xffff00 is bright green instead yellow, use 0xff3000 is orange-ish
// M5.dis.drawpix(0, 0xff3000); //set LED orange
#define GREEN 0x001100 // low for color weighting and power saving
#define RED 0xff0000
#define BLUE 0x0000ff
#define WHITE 0xffffff
#define ORANGE 0xff3000
#define MAGENTA 0xff00ff
#define BLACK 0x000000
#define LED_OFF 0x000000

#include <ArduinoJson.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "MFRC522_I2C.h"
MFRC522 mfrc522(0x28); // Create MFRC522 instance.

// define beep pins, GPIO21 for dia.12 buzzer, GPIO25 for dia.9 buzzer
#define beepPin 21
#define beepPin1 25

// define GPIO39 for button
#define buttonPin 39

// define bridge AP SSID and PWD
#define bridge_ssid "abc"
#define bridge_password "12345678"

// define apAP, heartAPI, fwAPI URLs

// api_tmp_page: https://arduino-kang.github.io/ap_tmp_page/
#define apAPI "https://makerkang.com:4500/?getAP" //"https://deploy-ap-api.herokuapp.com/?getAP"
#define heartBeatAPI "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/check_status"
#define fwAPI "https://nodemcu-kang.github.io/man-machine-RFID-bind/ArduinoIDE/D1_Mini_RFID_Reader/fw_version.json"

// define RFID API URL to use
// #define aws_rfid_err  // generate an api error
#if defined(aws_rfid_err) // for test
const char *apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/";
#else
const char *apiURL = "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid";
#endif

String apiHttpsGet(const char *apiURL);
bool apiHttpsPost(const char *apiURL, String rfid_uid);
String H8ToD10(byte *buffer, byte bufferSize);
void beep(int beepTime);
void beeep();
void errorBeep();
void writeStringToEEPROM(int addrOffset, const String &strToWrite);
String readStringFromEEPROM(int addrOffset);
int checkApAPI(int timeoutTick);
void (*resetFunc)(void) = 0; // declare reset function @ address 0

unsigned long lastTime = 0;
bool force_bridge_mode = false;
String aSSID = ""; // Field AP SSID
String aPWD = "";  // Field AP PWD

char json_input[100];
DeserializationError json_error;
const char *json_element;
StaticJsonDocument<200> json_doc;
String apiReturn;

unsigned int max_delay = 0;
byte errorTimes = 5;

byte i = 0; // breath lamp index, should be deleted after
int coffee = 0;
int max_rssi = -100;
int min_rssi = 0;
bool new_rfid_detect = false;
String uid;

void setup()
{
  M5.begin(true, false, true); // M5Atom.h - void begin(bool SerialEnable = true, bool I2CEnable = true, bool DisplayEnable = false);
                               // Atom-lite default 25(SDA), 21(SCL) is not used, so set I2CEnable false
  u8g2.begin();
  u8g2.enableUTF8Print(); // enable UTF8 support for the Arduino print() function
  u8g2.setFont(u8g2_font_unifont_t_chinese1);
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();
  u8g2.setCursor(0,15);
  u8g2.print("啟動 ..."); // u8g2.drawStr() can not display Chinese
  // u8g2.drawStr(0, 63, "Card: Place card");
  u8g2.sendBuffer();

  // button GPIO - pressed when boot up to force to use bridge AP mode
  pinMode(buttonPin, INPUT_PULLUP);
  force_bridge_mode = digitalRead(39) == LOW; // when button is pressed during boot, force to use bridge AP mode
  Serial.print("Button GPIO39 pressed: ");
  Serial.println(force_bridge_mode);

  // Serial.println(getCpuFrequencyMhz());

  // Notify for booting up
  M5.dis.drawpix(0, RED); // RED for start up
  pinMode(beepPin, OUTPUT);
  pinMode(beepPin1, OUTPUT);
  beep(100); // one beep for power up

  // Serial.begin(115200); //M5.begin (LCDEnable, PowerEnable, SerialEnable) already enable Serial
  Serial.println("Source: MAQ D:\\WebApp Projects\\ArduinoDevices\\P02-人機綁定\\man-machine-RFID-bind\\PIO\\Atom_lite_RFID_Reader");
  Serial.println("Github: https://github.com/NodeMCU-Kang/man-machine-RFID-bind");
  Serial.println();

  Serial.println(WiFi.macAddress());

  // Check EEPROM
  EEPROM.begin(4096);
  byte eepromFlag; // eeprom Flag: 0x55 for valid EEPROM
  eepromFlag = EEPROM.read(0);

  // if eepromFlag !=0x55, connect to the bridge AP, "abc"/"12345678"
  // set mobile phone's hotspot as SSID:"abc" and PWD:"12345678" for bridge AP
  if ((eepromFlag != 0x55) || (force_bridge_mode == 1))
  {
    if (eepromFlag != 0x55)
      Serial.print("No SSID in EEPROM, Connect to bridge_AP: ");
    if (force_bridge_mode == 1)
      Serial.print("User forces to Connect to bridge_AP: ");
    Serial.println(bridge_ssid);

    if (checkApAPI(50) == 0)
    {
      // through bridge_ssid connect to api_tmp_page: https://arduino-kang.github.io/ap_tmp_page/
      // and get vaild aSSID and aPWD
      EEPROM.write(0, 0x55);
      writeStringToEEPROM(1, aSSID);
      writeStringToEEPROM(1 + aSSID.length() + 1, aPWD);
      EEPROM.commit();
      WiFi.disconnect();
    }
    else
    {
      resetFunc(); // reset
    }
  }
  else
  { // else eepromFlag==55, read aSSID/aPWD from EEPROM
    Serial.println("Read SSID/PWD from EEPROM");
    aSSID = readStringFromEEPROM(1);
    aPWD = readStringFromEEPROM(1 + aSSID.length() + 1);
    Serial.printf("SSID: %s, PWD: %s\n", aSSID, aPWD);
  }

  // Serial.printf("SSID: %s, PWD: %s\n", aSSID.c_str(), aPWD.c_str());

  // beep(100);
  // delay(200);
  // beep(100); // two beeps for connecting to AP
  // M5.dis.drawpix(0, MAGENTA);

  // WiFi.mode(WIFI_STA);

  // WiFi.begin(aSSID.c_str(), aPWD.c_str());
  // Serial.printf("Connecting to AP/Router %s...\n", aSSID.c_str());

  // // wifi_set_sleep_type(NONE_SLEEP_T); // ESP8266 prevent WiFi entering sleep; ESP32 could not use

  // int iTimeout = 0;
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.printf("%d.", iTimeout);
  //   if (iTimeout++ == 50)
  //   {
  //     Serial.println("\nWiFi Connect Timeout");
  //     resetFunc();
  //     break;
  //   }
  // }

  // beep(100);
  // delay(200);
  // beep(100);
  // delay(200);
  // beep(100); // three beeps AP for connected

  // Serial.println("");
  // Serial.print("Connected! IP address: ");
  // Serial.println(WiFi.localIP());

  // M5.dis.drawpix(0, GREEN);

  // // Check Firmware version");
  // //  Serial.println("Get fw_version.json");
  // //  apiReturn = apiHttpsGet(fwAPI);
  // //  Serial.println(apiReturn);
  // //  apiReturn.toCharArray(json_input,100);
  // //  json_error = deserializeJson(json_doc, json_input);
  // //  if (!json_error) {
  // //    json_element = json_doc["latestVersion"];
  // //    Serial.println(String(json_element));
  // //    json_element = json_doc["Release"];
  // //    Serial.println(String(json_element));
  // //    json_element = json_doc["binName"];
  // //    Serial.println(String(json_element));
  // //  }

  // // Check heartbeat API
  // Serial.println(heartBeatAPI);
  // apiReturn = apiHttpsGet(heartBeatAPI);
  // Serial.println(apiReturn);

  // // First time call apiHttpPost() would take longer time for response, this call could save the response time later
  // Serial.println(apiURL);
  // lastTime = millis();
  // if (apiHttpsPost(apiURL, "test"))
  // {
  //   Serial.printf("API successed in %d\n", millis() - lastTime);
  //   beep(100);
  //   delay(200);
  //   beep(100);
  //   delay(200);
  //   beep(100);
  //   delay(200);
  //   beep(100); // four beeps for heartbeat API success
  // }
  // else
  // {
  //   Serial.printf("API failed in %d\n", millis() - lastTime);
  //   M5.dis.drawpix(0, RED); // heartbeat API failed
  //   errorBeep();            // turn on when ready
  //   if (--errorTimes == 0)
  //   {
  //     resetFunc();
  //   }
  // }

  u8g2.setCursor(0, 63);
  u8g2.print("卡號: 請感應卡片");
  u8g2.sendBuffer();

  Serial.println("\nMFRC522 Init...");
  Wire.begin(26, 32); // MFRC522 use HY2.0 pins for I2C, 26(SDA), 32(SCL)
  mfrc522.PCD_Init(); // Init MFRC522.
  Serial.println("\nMFRC522 OK");

  Serial.println();
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  lastTime = millis();
  max_delay = 0;

  M5.dis.drawpix(0, GREEN); // Everythings OK

  new_rfid_detect = false;
}

void loop()
{
  if (new_rfid_detect)
  {
    Wire.begin(ATOMOLED_SDA, ATOMOLED_SCL); // Switch I2C to u8g2. using u8g2.begin() will causing flicker
    u8g2.setDrawColor(0);
    u8g2.drawBox(45, 48, 128, 16);
    u8g2.setDrawColor(1);
    u8g2.drawStr(45, 63, uid.c_str());
    u8g2.sendBuffer();
    new_rfid_detect = false;
    delay(1000);
    u8g2.setCursor(0, 63);
    u8g2.print("卡號: 請感應卡片");
    u8g2.sendBuffer();    
  }
  // re-init for cheap RFID cards
  Wire.begin(26, 32); // Switch I2C to MFRC522.
  mfrc522.PCD_Init();
  delay(4);

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent())
  {
  }
  else
  {
    if (!mfrc522.PICC_ReadCardSerial())
    {
      return;
    }

    M5.dis.drawpix(0, ORANGE);

    beep(100); // short beep to acknowdge RFID card is read

    uid = H8ToD10(mfrc522.uid.uidByte, mfrc522.uid.size);

    mfrc522.PICC_HaltA(); // Halt for the next card

    Serial.println(uid);
    Serial.println();

    new_rfid_detect = true;
    // Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  }
}

void loop_1()
{

  //**** simple working RFID reading
  // if (!mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()) {  //no new card detected
  //   delay(200);
  //   return;
  // }
  // beep(100);

  // Serial.print("UID:");
  // for (byte i = 0; i < mfrc522.uid.size; i++) {
  //   Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
  //   Serial.print(mfrc522.uid.uidByte[i], HEX);
  // }
  // Serial.println("");
  //******************************************************************************************

  // Copy from D1_Mini_RFID_Reader
  if (WiFi.status() == WL_CONNECTED)
  {

    int rssi = WiFi.RSSI();
    if (rssi > max_rssi)
    {
      max_rssi = rssi;
    }
    if (rssi < min_rssi)
    {
      min_rssi = rssi;
    }
    // Serial.printf("RSSI now:%d, MAX RSSI:%d, MIN RSSI:%d \n", rssi, max_rssi, min_rssi);

    if (rssi < -70)
    {
      M5.dis.drawpix(0, RED); // RSSI is too low
    }
    else if (rssi < -65)
    {
      M5.dis.drawpix(0, ORANGE); // RSSI is low
    }
    else
    {
      M5.dis.drawpix(0, GREEN); // RSSI is good
    }

    if (millis() - lastTime > 60000)
    { // refresh to against cloud sleeping
      lastTime = millis();
      Serial.printf("Coffee at %d\n", millis());
      Serial.printf("RSSI now:%d, MAX RSSI:%d, MIN RSSI:%d \n", rssi, max_rssi, min_rssi);
      Serial.println(heartBeatAPI);
      apiReturn = apiHttpsGet(heartBeatAPI);
      Serial.println(apiReturn);
      if (apiReturn == "\"OK!\"")
      {
        Serial.printf("API successed in %d\n", millis() - lastTime);
        M5.dis.drawpix(0, GREEN);
      }
      else
      {
        Serial.printf("API failed in %d\n", millis() - lastTime);
        Serial.println("");
        M5.dis.drawpix(0, RED);
        errorBeep();
        if (--errorTimes == 0)
        {
          resetFunc();
        }
        // lastTime = millis();
      }
      lastTime = millis();
    }

    // re-init for cheap RFID cards
    mfrc522.PCD_Init();
    delay(4);

    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if (!mfrc522.PICC_IsNewCardPresent())
    {
    }
    else
    {
      if (!mfrc522.PICC_ReadCardSerial())
      {
        return;
      }

      M5.dis.drawpix(0, ORANGE);

      beep(100); // short beep to acknowdge RFID card is read

      String uid = H8ToD10(mfrc522.uid.uidByte, mfrc522.uid.size);
      mfrc522.PICC_HaltA(); // Halt for the next card

      // Serial.println(apiURL);
      lastTime = millis();
      if (apiHttpsPost(apiURL, uid))
      {
        Serial.printf("API successed in %d\n", millis() - lastTime);
        beep(100);
        M5.dis.drawpix(0, GREEN);
      }
      else
      {
        Serial.printf("API failed in %d\n", millis() - lastTime);
        M5.dis.drawpix(0, RED);
        errorBeep();
        if (--errorTimes == 0)
        {
          resetFunc();
        }
      }

      // Serial.println();
      // Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
      lastTime = millis();
    }
  }
  else
  {
    Serial.println("Networked is not connected");
    M5.dis.drawpix(0, GREEN);
    // delay(1000);
  }
}

String apiHttpsGet(const char *apiURL)
{

  // BearSSL::WiFiClientSecure wifiClientSecure;
  WiFiClientSecure wifiClientSecure;
  HTTPClient httpsGet;

  wifiClientSecure.setInsecure();
  httpsGet.setTimeout(20000);

  String payload = "";

  Serial.printf("[HTTPS] GET %s ...\n", apiURL);
  if (httpsGet.begin(wifiClientSecure, apiURL))
  {
    // if (https.begin(httpsClient, apiURL)) {

    // start connection and send HTTP header
    unsigned long lastTime1 = millis();
    int httpCode = httpsGet.GET();
    // Serial.println("httpsGet:", millis()-lastTime1);

    Serial.printf("[HTTPS] GOT code: %d\n", httpCode);
    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // data found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        payload = httpsGet.getString();
      }
    }
    else
    {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", httpsGet.errorToString(httpCode).c_str());
      payload = "";
    }

    httpsGet.end();
    // return payload;
  }
  return payload;
}

bool apiHttpsPost(const char *apiURL, String rfid_uid)
{
  bool success = false;

  WiFiClientSecure wifiClientSecure;
  HTTPClient httpsPost;

  wifiClientSecure.setInsecure();
  httpsPost.setTimeout(20000);

  // Serial.print("[HTTPS] begin...\n");
  // if (https.begin(*httpsClient, "https://ilxgxw0o3a.execute-api.ap-northeast-1.amazonaws.com/ugym/5/v4/machine/machine_user_login_with_rfid")) {
  // if (https.begin(*httpsClient, apiURL)) { //another way using pointer of httpsClient
  if (httpsPost.begin(wifiClientSecure, apiURL))
  { // not pointer
    httpsPost.addHeader("Content-Type", "application/json");

    // String postBody = "{\"params\":{\"token_id\":\"rfid_token\",\"login_dict\":{\"rf_id\":\"2512255499\",\"wifi_mac\":\"48:3F:DA:49:2E:46\"}}}";
    String postBodyBegin = "{\"params\":{\"token_id\":\"rfid_token\",\"login_dict\":{\"rf_id\":\"";
    // String postBodyEnd   = "\",\"wifi_mac\":\"48:3F:DA:49:2E:46\"}}}";
    String postBodyEnd = "\",\"wifi_mac\":\"" + WiFi.macAddress() + "\"}}}";
    String postBody = String(postBodyBegin + rfid_uid);
    postBody = String(postBody + postBodyEnd);
    Serial.println(postBody);

    Serial.print("[HTTP] POST...\n");
    lastTime = millis();
    int httpCode = httpsPost.POST(postBody);
    unsigned int apiTime = millis() - lastTime;
    Serial.println(apiTime);
    if (apiTime > max_delay)
      max_delay = apiTime;

    // Serial.printf("%d, %d\n",coffee++,apiTime);
    // Serial.printf("API Time:%d, Max_delay:%d\n", apiTime, max_delay);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK ||
          httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = httpsPost.getString();
        // Serial.println(payload);
        success = true;
      }
    }
    else
    {
      // Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    httpsPost.end();
  }
  else
  {
    // Serial.printf("[HTTPS] Unable to connect\n");
  }
  return success;
}

void dump_byte_array(byte *buffer, byte bufferSize)
{
  unsigned long uid = 0;
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    uid = uid + (buffer[i] << (8 * i));
    Serial.println(uid, HEX);
  }
  Serial.println(uid);
}
String H8ToD10(byte *buffer, byte bufferSize)
{
  unsigned long uid = 0;
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    uid = uid + (buffer[i] << (8 * i));
  }
  Serial.println();
  return String(uid);
}

void beep(int beep_time)
{
  digitalWrite(beepPin, HIGH);
  digitalWrite(beepPin1, HIGH);

  delay(beep_time);

  digitalWrite(beepPin, LOW);
  digitalWrite(beepPin1, LOW);
}
void beeep()
{
  beep(300);
}
void errorBeep()
{
  // beep-- beep beep beep beep--
  beep(1000);
  delay(100);
  beep(100);
  delay(100);
  beep(100);
  delay(100);
  beep(100);
  delay(100);
  beep(100);
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

int checkApAPI(int timeoutTick)
{
  // Connect to the Bridge AP
  u8g2.setCursor(0, 31);
  u8g2.print("連接 AP 中: abc");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.begin(bridge_ssid, bridge_password);

  Serial.printf("Connecting to bridge hotspot AP %s...\n", bridge_ssid);

  int iTimeout = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.printf("%d.", iTimeout);
    if (iTimeout++ == timeoutTick)
    {
      Serial.println();
      return -1; // timeout failure
    }
  }

  u8g2.setCursor(0, 47);
  u8g2.print("連接 apAPI 中");
  u8g2.sendBuffer();

  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  while (aSSID == "") // Continue until get valid aSSID set to the api_tmp_page: https://arduino-kang.github.io/ap_tmp_page/
  {
    Serial.println("Get apAPI");
    apiReturn = apiHttpsGet(apAPI);
    Serial.println(apiReturn);
    apiReturn.toCharArray(json_input, 100);
    json_error = deserializeJson(json_doc, json_input);
    if (!json_error)
    {
      json_element = json_doc["SSID"];
      aSSID = String(json_element);
      json_element = json_doc["PWD"];
      aPWD = String(json_element);
    }
    else
      return -2; // json error
  }
  return 0; // success
}
