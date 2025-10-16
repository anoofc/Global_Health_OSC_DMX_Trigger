#define DEBUG       1

#define SWITCH_PIN  32  // GPIO pin for the switch
#define LED_PIN     13  // GPIO pin for the built-in LED

#define LED_COUNT   9  // Number of LEDs in the strip

#define DEBOUNCE_DELAY  50 // Debounce delay in milliseconds
#define FPS             30 // Frames per second for DMX updates

#include <Wire.h>
#include <Arduino.h>
#include <OSCMessage.h>
#include <ETH.h>                                                                                                    
#include <WiFiUdp.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include "eth_properties.h"


BluetoothSerial SerialBT; // Bluetooth Serial
Preferences preferences;  // Preferences for storing data
WiFiUDP Udp;

IPAddress ip, subnet, gateway, outIp;
uint16_t inPort = 7000;
uint16_t outPort = 7001;

uint32_t lastMillis = 0;
bool switchState = false;

const String HELP = "SET_IP x.x.x.x\nSET_SUBNET x.x.x.x\nSET_GATEWAY x.x.x.x\nSET_OUTIP x.x.x.x\nSET_INPORT xxxx\nSET_OUTPORT xxxx\nGET\nIP\nMAC\nHELP";

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
  preferences.putUInt("inPort", inPort); // Save input port
  preferences.putUInt("outPort", outPort); // Save output port
  preferences.end();
}

void loadNetworkConfig() {
  preferences.begin("NET_CONFIG", true);
  ip      = loadIPAddress("ip",  IPAddress(10, 255, 250, 150));
  subnet  = loadIPAddress("sub", IPAddress(255, 255, 254, 0));
  gateway = loadIPAddress("gw",  IPAddress(10, 255, 250, 1));
  outIp   = loadIPAddress("out", IPAddress(10, 255, 250, 129));
  inPort  = preferences.getUInt("inPort", 7001); // Load input port
  outPort = preferences.getUInt("outPort", 7000); // Load output port
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
  SerialBT.printf("Input port: %d\n", inPort);
  SerialBT.printf("Output port: %d\n", outPort);

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

void readSwitch(){
  if (digitalRead(SWITCH_PIN) == LOW && switchState == HIGH) {
    if (millis() - lastMillis < DEBOUNCE_DELAY) return; // Debounce check
    switchState = LOW;
    lastMillis = millis();
    // TODO: TRIGGER SLAVE
    oscSend(1, 1); // Send OSC message for column 1
    // if (DEBUG) { Serial.println("Switch Pressed - DMX 255 Sent"); }
  } else if (digitalRead(SWITCH_PIN) == HIGH && switchState == LOW) {
    if (millis() - lastMillis < DEBOUNCE_DELAY) return; // Debounce check
    lastMillis = millis();
    switchState = HIGH;
    // TODO: RESET SLAVE
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
  SerialBT.begin("GH_MediaPro");
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  loadNetworkConfig();
  ethInit();

}

void loop() {
  readSwitch();
  readBTSerial();
}