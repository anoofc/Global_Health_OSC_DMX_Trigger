#define DEBUG       1

#define SWITCH_PIN  32  // GPIO pin for the switch
#define LED_PIN     13  // GPIO pin for the built-in LED
#define ESP_OUT     14  // GPIO pin for the ESP output

#define LED_COUNT   9  // Number of LEDs in the strip

#define DEBOUNCE_DELAY  50 // Debounce delay in milliseconds
#define FPS             30 // Frames per second for DMX updates

#include <Arduino.h>
#include <OSCMessage.h>
#include <ETH.h>                                                                                                    
#include <WiFiUdp.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include "eth_properties.h"
#include <Adafruit_NeoPixel.h>


BluetoothSerial SerialBT; // Bluetooth Serial
Preferences preferences;  // Preferences for storing data
WiFiUDP Udp;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

IPAddress ip, subnet, gateway, outIp, MAIp;
uint16_t inPort = 7000;
uint16_t outPort = 7001;
uint16_t maOutPort = 8000;

uint32_t timeout = 0;

uint32_t lastMillis = 0;
uint32_t showTimer = 0;

uint8_t trigledColor[3] = {0, 0, 0};
uint8_t idleledColor[3] = {0, 0, 0};

bool switchState = false;
bool showRunning = false;

const String HELP = "SET_IP x.x.x.x\nSET_SUBNET x.x.x.x\nSET_GATEWAY x.x.x.x\nSET_OUTIP x.x.x.x\nSET_INPORT xxxx\nSET_OUTPORT xxxx\nSET_TIMEOUT xxxx\nSET_MAIP x.x.x.x\nSET_MAOUTPORT xxxx\n SET_TRIG_LED xxx,xx,xx \n SET_IDLE_LED xxx,xx,xx\nGET\nIP\nMAC\nHELP";

void saveIPAddress(const char* keyPrefix, IPAddress address) {
  for (int i = 0; i < 4; i++) {
    String key = String(keyPrefix) + i;
    preferences.putUInt(key.c_str(), address[i]);
  }
}

IPAddress loadIPAddress(const char* keyPrefix, IPAddress defaultIP) {
  IPAddress result;
  for (int i = 0; i < 4; i++) {
    String key = String(keyPrefix) + i;
    result[i] = preferences.getUInt(key.c_str(), defaultIP[i]);
  }
  return result;
}

void saveNetworkConfig() {
  preferences.begin("NET_CONFIG", false);
  saveIPAddress("ip", ip);
  saveIPAddress("sub", subnet);
  saveIPAddress("gw", gateway);
  saveIPAddress("out", outIp);
  saveIPAddress("ma", MAIp);
  preferences.putUInt("inPort",  inPort); // Save input port
  preferences.putUInt("outPort", outPort); // Save output port
  preferences.putInt("maOutPort", maOutPort); // Save maOutPort
  preferences.putUInt("timeout", timeout); // Save timeout
  preferences.putUInt("trigR", trigledColor[0]);
  preferences.putUInt("trigG", trigledColor[1]);
  preferences.putUInt("trigB", trigledColor[2]);
  preferences.putUInt("idleR", idleledColor[0]);
  preferences.putUInt("idleG", idleledColor[1]);
  preferences.putUInt("idleB", idleledColor[2]);
  preferences.end();
}

void loadNetworkConfig() {
  preferences.begin("NET_CONFIG", true);
  ip      = loadIPAddress("ip",  IPAddress(192, 168, 1, 99));
  subnet  = loadIPAddress("sub", IPAddress(255, 0, 0, 0));
  gateway = loadIPAddress("gw",  IPAddress(192, 168, 0, 1));
  outIp   = loadIPAddress("out", IPAddress(192, 168, 1, 101));
  MAIp    = loadIPAddress("ma",  IPAddress(192, 168, 1, 100));
  inPort  = preferences.getUInt("inPort", 7001); // Load input port
  outPort = preferences.getUInt("outPort", 7000); // Load output port
  timeout = preferences.getUInt("timeout", 5000); // Load timeout
  maOutPort = preferences.getInt("maOutPort", 8000);
  trigledColor[0] = preferences.getUInt("trigR", 255);
  trigledColor[1] = preferences.getUInt("trigG", 0);
  trigledColor[2] = preferences.getUInt("trigB", 0);
  idleledColor[0] = preferences.getUInt("idleR", 0);
  idleledColor[1] = preferences.getUInt("idleG", 255);
  idleledColor[2] = preferences.getUInt("idleB", 0);
  preferences.end();
}

void oscSend(uint8_t column, int value) {
  char address[64];
  snprintf(address, sizeof(address), "/composition/columns/%d/connect", column);
  OSCMessage msg(address);
  msg.add(value);
  Udp.beginPacket(outIp, outPort); 
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void MAoscSend(int fader,int message) {
  char address[100];
  snprintf(address, sizeof(address), "/gma3/Page1/Fader%d", fader);
  OSCMessage msg(address);
  msg.add(message);
  Udp.beginPacket(MAIp, maOutPort); 
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}


// void timeCodeOSCSend(uint8_t mode){
//   char address[20];
//   snprintf(address, sizeof(address), "/mode%d", mode);
//   OSCMessage msg(address);
//   Udp.beginPacket(outIp, outPort);
//   msg.send(Udp);
//   Udp.endPacket();
//   msg.empty();
// }


void getConfig(){
  preferences.begin("NET_CONFIG", true); // Open preferences with read-only access
  SerialBT.printf("IP: %s\n", ip.toString().c_str());
  SerialBT.printf("Subnet: %s\n", subnet.toString().c_str());
  SerialBT.printf("Gateway: %s\n", gateway.toString().c_str());
  SerialBT.printf("OutIP: %s\n", outIp.toString().c_str());
  SerialBT.printf("MA IP: %s\n", MAIp.toString().c_str());
  SerialBT.printf("Input port: %d\n", inPort);
  SerialBT.printf("Output port: %d\n", outPort);
  SerialBT.printf("MA Output port: %d\n", maOutPort);
  SerialBT.printf("Timeout: %d ms\n", timeout);
  trigledColor[0] = preferences.getUInt("trigR", 255);
  trigledColor[1] = preferences.getUInt("trigG", 0);
  trigledColor[2] = preferences.getUInt("trigB", 0);
  idleledColor[0] = preferences.getUInt("idleR", 0);
  idleledColor[1] = preferences.getUInt("idleG", 255);
  idleledColor[2] = preferences.getUInt("idleB", 0);

  SerialBT.printf("Trigger LED Color: R:%d G:%d B:%d\n", trigledColor[0], trigledColor[1], trigledColor[2]);
  SerialBT.printf("Idle LED Color: R:%d G:%d B:%d\n", idleledColor[0], idleledColor[1], idleledColor[2]);
  preferences.end();

}

void processData(String data) {
  data.trim(); // Remove leading and trailing whitespace
  auto updateIP = [&](const String& prefix, IPAddress& target, int offset) {
    String value = data.substring(offset);
    if (target.fromString(value)) {
      saveNetworkConfig();
      SerialBT.printf("✅ %s updated and saved.\n", prefix.c_str());
    } else {
      SerialBT.printf("❌ Invalid %s format.\n", prefix.c_str());
    }
  };
  if (data.startsWith("SET_IP ")) { updateIP("IP", ip, 7); } 
  else if (data.startsWith("SET_SUBNET ")) { updateIP("Subnet", subnet, 11); } 
  else if (data.startsWith("SET_GATEWAY ")) { updateIP("Gateway", gateway, 12);  } 
  else if (data.startsWith("SET_OUTIP ")) { updateIP("OutIP", outIp, 10); } 
  else if (data.startsWith("SET_MAIP ")) { updateIP("MA IP", MAIp, 9); }
  else if (data.startsWith ("SET_INPORT ")) {
    int port = data.substring(10).toInt();
    if (port > 0 && port < 65536) { inPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ Input port set to %d and saved.\n", inPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }
  else if (data.startsWith("SET_OUTPORT ")) {
    int port = data.substring(12).toInt();
    if (port > 0 && port < 65536) { outPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ Output port set to %d and saved.\n", outPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }
  else if (data.startsWith("SET_MAPORT ")) {
    int port = data.substring(11).toInt();
    if (port > 0 && port < 65536) { maOutPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ MA Output port set to %d and saved.\n", maOutPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }
  else if (data.startsWith("SET_TIMEOUT ")) {
    uint32_t t = data.substring(12).toInt();
    timeout = t; 
    saveNetworkConfig();
    SerialBT.printf("✅ Timeout set to %d ms and saved.\n", timeout);
  }
  else if (data.startsWith("SET_TRIG_LED ")) {
    String value = data.substring(13);
    int r, g, b;
    if (sscanf(value.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
      trigledColor[0] = r; trigledColor[1] = g; trigledColor[2] = b;
      SerialBT.printf("✅ LED Color set to R:%d G:%d B:%d.\n", r, g, b);
      saveNetworkConfig();
    } else {
      SerialBT.println("❌ Invalid LED Color format. Use R,G,B.");
    }
  }
  else if (data.startsWith("SET_IDLE_LED ")) {
    String value = data.substring(13);
    int r, g, b;
    if (sscanf(value.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
      idleledColor[0] = r; idleledColor[1] = g; idleledColor[2] = b;
      SerialBT.printf("✅ Idle LED Color set to R:%d G:%d B:%d.\n", r, g, b);
      saveNetworkConfig();
    } else {
      SerialBT.println("❌ Invalid Idle LED Color format. Use R,G,B.");
    }
  }
  else if (data == "IP") { SerialBT.printf("ETH IP: %s\n", ETH.localIP().toString().c_str());}
  else if (data == "MAC") { SerialBT.printf("ETH MAC: %s\n", ETH.macAddress().c_str());}
  
  else if (data.indexOf("HELP")>=0){
    SerialBT.println(HELP);
    Serial.println(HELP);
    return;
  } else if (data.indexOf("GET")>=0){
    getConfig();
    return;
  }
}

void readBTSerial(){
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    processData(incoming);
    if (DEBUG) {SerialBT.println(incoming);}
  }
}

void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  strip.clear();
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  if (DEBUG) { Serial.printf("LED Color Set to R:%d G:%d B:%d\n", r, g, b);  }
}

void checkShowTimeout(){
  if (showRunning && (millis() - showTimer >= timeout)){ 
  setLEDColor(idleledColor[0], idleledColor[1], idleledColor[2]);
  digitalWrite(ESP_OUT, HIGH);
  MAoscSend(216, 0); // Send OSC message to MA
  MAoscSend(230, 100); // Send OSC message to MA
  showRunning = false;
  if (DEBUG) { Serial.println("Show Timeout - DMX 0 Sent"); }
  }
}

void readSwitch(){
  if (digitalRead(SWITCH_PIN) == LOW && switchState == HIGH) {
    if (millis() - lastMillis < DEBOUNCE_DELAY) return; // Debounce check
    if (!showRunning) {
      setLEDColor(trigledColor[0], trigledColor[1], trigledColor[2]);
      switchState = LOW;
      lastMillis = millis();
      digitalWrite(ESP_OUT, LOW);
      oscSend(3, 1); // Send OSC message for column 1
      MAoscSend(230, 0); // Send OSC message to MA
      MAoscSend(216, 100); // Send OSC message to MA
      showTimer = millis();
      showRunning = true;
      if (DEBUG) { Serial.println("Switch Pressed - DMX 255 Sent"); }
    }
  } else if (digitalRead(SWITCH_PIN) == HIGH && switchState == LOW) {
    if (millis() - lastMillis < DEBOUNCE_DELAY) return; // Debounce check
    lastMillis = millis();
    switchState = HIGH;
    // digitalWrite(ESP_OUT, HIGH);
    // if (DEBUG) { Serial.println("Switch Released - DMX 0 Sent"); }
  }
}


void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH IP: ");
      Serial.println(ETH.localIP());
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      break;
    default:
      break;
  }
}

void ethInit() {
  ETH.begin( ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE_0);
  ETH.config(ip, gateway, subnet);
  WiFi.onEvent(WiFiEvent);
  Udp.begin(inPort);
  // delay(5000); // Wait for the Ethernet to initialize
  Serial.println("ETH Initialized");
  Serial.printf("ETH IP: %s\n", ETH.localIP().toString().c_str());
  Serial.printf("ETH MAC: %s\n", ETH.macAddress().c_str());
}


void setup() {
  Serial.begin(115200);
  SerialBT.begin("GH_mediaPro");
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(ESP_OUT, OUTPUT);
  digitalWrite(ESP_OUT, HIGH);

  loadNetworkConfig();
  ethInit();

  MAoscSend(216, 0); // Initialize MA Fader 216 to 0
  MAoscSend(230, 100); // Initialize MA Fader 230 to 0

  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  strip.show(); // Initialize all pixels to 'off'
  setLEDColor(idleledColor[0], idleledColor[1], idleledColor[2]);
}

void loop() {
  readSwitch();
  readBTSerial();
  checkShowTimeout();
}