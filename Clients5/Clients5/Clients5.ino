#include <ModbusMaster.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define PZEM_SLAVE_ADDR 0x01
#define RXD_PIN 16 // พิน RX ของ Serial2 (ปรับตามที่เหมาะสม)
#define TXD_PIN 17 // พิน TX ของ Serial2 (ปรับตามที่เหมาะสม)

const char* ssid = "";
const char* password = "";
const char* serverName = "";

ModbusMaster node;
const int maxRetries = 3;
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000; // 10 วินาที

void setup() {
  Serial.begin(115200); 
  Serial2.begin(9600, SERIAL_8N1, RXD_PIN, TXD_PIN); // เริ่มต้น RS485 ที่ 9600 bps
  node.begin(PZEM_SLAVE_ADDR, Serial2); 

  // เชื่อมต่อ Wi-Fi
  connectWiFi();
}

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  // รอเชื่อมต่อ Wi-Fi แต่ไม่เกิน 10 วินาที
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }
}

float readRegister(uint16_t reg, float factor, const char* type) {
  int retries = 0;
  float resultValue = -1;
  while (retries < maxRetries) {
    uint8_t result = node.readInputRegisters(reg, 1);
    if (result == node.ku8MBSuccess) {
      uint16_t rawValue = node.getResponseBuffer(0);
      resultValue = rawValue * factor;
      Serial.printf("Read %s: %.2f\n", type, resultValue);
      return resultValue;
    }
    retries++;
    Serial.printf("Error reading %s, retry %d/%d\n", type, retries, maxRetries);
    delay(500);
  }
  return resultValue;
}

float readVoltage() {
  return readRegister(0x0000, 0.01, "Voltage");
}

float readCurrent() {
  return readRegister(0x0001, 0.01, "Current");
}

float readPower() {
  uint8_t result = node.readInputRegisters(0x0002, 2);
  if (result == node.ku8MBSuccess) {
    uint32_t powerRaw = (node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return powerRaw * 0.1;
  }
  return -1;
}

float readEnergy() {
  uint8_t result = node.readInputRegisters(0x0004, 2);
  if (result == node.ku8MBSuccess) {
    uint32_t energyRaw = (node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return energyRaw * 1.0;
  }
  return -1;
}

void loop() {
  // ตรวจสอบการเชื่อมต่อ Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= reconnectInterval) {
      Serial.println("Attempting to reconnect to WiFi...");
      WiFi.disconnect();
      connectWiFi();
      lastReconnectAttempt = now;
    }
  } else {
    float voltage = readVoltage();
    float current = readCurrent();
    float power = readPower();
    float energy = readEnergy();

    if (voltage == -1 || current == -1 || power == -1 || energy == -1) {
      Serial.println("Error reading from PZEM-017, skipping this cycle.");
    } else {
      String httpRequestData = "data=PZEM," + String(voltage) + "," + String(current) + "," + String(power) + "," + String(energy);

      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverName);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println("Server response: " + response);
        } else {
          Serial.println("Error in sending POST request: " + String(httpResponseCode));
        }

        http.end();
      } else {
        Serial.println("WiFi Disconnected, cannot send data");
      }
    }

    delay(60000); // อ่านข้อมูลและส่งทุกๆ 1 นาที
  }
}
