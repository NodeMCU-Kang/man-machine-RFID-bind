#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <time.h>

#include <FS.h>
#include <LittleFS.h>

#ifndef APSSID
#define APSSID "MAC"
#define APPSK  "Kpc24642464"
#endif

ESP8266WiFiMulti WiFiMulti;

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

unsigned long lastTime = 0;
void setup() {

  Serial.begin(115200);
  Serial.flush();
  delay(1000);  
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(APSSID, APPSK);
  WiFiMulti.addAP("abc", "12345678");  
  WiFiMulti.addAP("def", "12345678");   

  Serial.println();
  Serial.println("Wait for WiFi ");
 
//  while(WiFiMulti.run() != WL_CONNECTED) {
//    Serial.print(".");
//    delay(500);
//  }
  int connected=0;
  while (1) {
    lastTime = millis();
    connected= WiFiMulti.run();
    Serial.printf("after %d ms\n",millis() - lastTime);
    if (connected==WL_CONNECTED) break;
    delay(500);
  }
  
  Serial.println();
  Serial.printf("%s connected! IP address: ", WiFi.SSID().c_str());
  Serial.println(WiFi.localIP());

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  Serial.println("Checking if the server supports Max Fragment Length");
  bool mfln = client.probeMaxFragmentLength("nodemcu-kang.github.io", 
                                             443, 1024); 

  Serial.printf("MFLN supported: %s\n", mfln ? "yes" : "no");
  if (mfln) {
    client.setBufferSizes(1024, 1024);
  }
  
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

  // Add optional callback notifiers
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);    

  String binURL = "https://nodemcu-kang.github.io/man-machine-RFID-bind/ArduinoIDE/D1_Mini_RFID_Reader/D1_Mini_RFID_Reader.ino.nodemcu.bin";

  t_httpUpdate_return ret = ESPhttpUpdate.update(client, binURL);    
 
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", 
                    ESPhttpUpdate.getLastError(), 
                    ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }  
}

void loop() {
}
