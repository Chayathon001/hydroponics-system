#include <ModbusMaster.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>  // สำหรับการดึงเวลาจาก NTP

#define PZEM_SLAVE_ADDR 0x01
#define RXD_PIN 16
#define TXD_PIN 17

const char* ssid = "TOT_POR_2.4G";
const char* password = "0623080540";
const char* serverName = "http://192.168.1.100/insert_pzem.php";

ModbusMaster node;
const int maxRetries = 3;
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000;

// ตั้งค่า NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;  // เปลี่ยนตาม timezone ของคุณ (ประเทศไทยคือ UTC+7)
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD_PIN, TXD_PIN);
  node.begin(PZEM_SLAVE_ADDR, Serial2);

  connectWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // ตั้งค่า NTP
}

void connectWiFi() {
  IPAddress localIP(192, 168, 1, 106);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.config(localIP, gateway, subnet);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

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
  for (int retries = 0; retries < maxRetries; retries++) {
    if (node.readInputRegisters(reg, 1) == node.ku8MBSuccess) {
      uint16_t rawValue = node.getResponseBuffer(0);
      float resultValue = rawValue * factor;
      Serial.printf("Read %s: %.2f\n", type, resultValue);
      return resultValue;
    }
    delay(500);
    Serial.printf("Error reading %s, retry %d/%d\n", type, retries + 1, maxRetries);
  }
  return -1;
}

float readVoltage() { return readRegister(0x0000, 0.01, "Voltage"); }
float readCurrent() { return readRegister(0x0001, 0.01, "Current"); }

float readPower() {
  if (node.readInputRegisters(0x0002, 2) == node.ku8MBSuccess) {
    uint32_t powerRaw = (node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return powerRaw * 0.1;
  }
  return -1;
}

float readEnergy() {
  if (node.readInputRegisters(0x0004, 2) == node.ku8MBSuccess) {
    uint32_t energyRaw = (node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return energyRaw;
  }
  return -1;
}

uint16_t calculateCRC(uint8_t *data, uint8_t length) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void resetEnergy() {
  uint8_t resetCommand[4];
  resetCommand[0] = PZEM_SLAVE_ADDR; // Slave address
  resetCommand[1] = 0x42;            // Function code for energy reset

  // Calculate CRC
  uint16_t crc = calculateCRC(resetCommand, 2);
  resetCommand[2] = crc & 0xFF;           // CRC Low Byte
  resetCommand[3] = (crc >> 8) & 0xFF;    // CRC High Byte

  // Send reset command to PZEM-017
  Serial2.write(resetCommand, 4);

  // Read response from PZEM-017
  delay(100);  // Wait for a response
  if (Serial2.available()) {
    while (Serial2.available()) {
      Serial.print("0x");
      Serial.print(Serial2.read(), HEX);
      Serial.print(" ");
    }
    Serial.println("\nEnergy reset successful.");
  } else {
    Serial.println("No response from PZEM-017. Energy reset may have failed.");
  }
}

void loop() {
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
      // Send data to the server
      String httpRequestData = "data=PZEM," + String(voltage) + "," + String(current) + "," + String(power) + "," + String(energy);
      HTTPClient http;

      if (WiFi.status() == WL_CONNECTED) {
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

    // ตรวจสอบเวลาและรีเซ็ตเมื่อถึงเที่ยงคืน
    time_t now;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
        resetEnergy();  // Reset energy
        delay(60000);   // หน่วงเวลา 1 นาทีเพื่อป้องกันการรีเซ็ตซ้ำ
      }
    }

    delay(60000);  // Delay 1 minute
  }
}
