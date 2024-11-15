#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "TOT_POR_2.4G";
const char* password = "0623080540"; 
const char* serverNameFlow = "http://192.168.1.100/insert_flow.php"; // URL สำหรับส่งข้อมูล Flow
const char* serverNamePressure = "http://192.168.1.100/insert_pressure.php"; // URL สำหรับส่งข้อมูล Pressure

const int flowSensorPin = 13; // Pin ที่เชื่อมต่อกับ Flow Sensor
volatile int pulseCount = 0;
const float calibrationFactor = 7.5;
float flowRate = 0;

// พินสำหรับเซ็นเซอร์วัดแรงดันน้ำ
const int pressureSensorPin1 = 34; // พอร์ตแอนะล็อกสำหรับเซ็นเซอร์ที่ 1
const int pressureSensorPin2 = 35; // พอร์ตแอนะล็อกสำหรับเซ็นเซอร์ที่ 2

const float sensorVoltageMin = 0.5;
const float sensorVoltageMax = 4.5;
const float maxPressure = 150.0;

float offset1 = 0.0;
float offset2 = 0.0;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  // ตั้งค่า Static IP
  IPAddress localIP(192, 168, 1, 105);  // Static IP ที่ต้องการ
  IPAddress gateway(192, 168, 1, 1);    // Gateway ของคุณ
  IPAddress subnet(255, 255, 255, 0);   // Subnet Mask ของคุณ
  WiFi.config(localIP, gateway, subnet); // ตั้งค่า Static IP

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ตั้งค่า Flow Sensor
  pinMode(flowSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, RISING);

  // Calibrate offset สำหรับเซ็นเซอร์แรงดันน้ำ
  const int numSamples = 200;
  int totalValue1 = 0, totalValue2 = 0;

  for (int i = 0; i < numSamples; i++) {
    totalValue1 += analogRead(pressureSensorPin1);
    totalValue2 += analogRead(pressureSensorPin2);
    delay(10);
  }

  float sensorVoltage1 = (totalValue1 / numSamples) * (5.0 / 4095.0);
  float sensorVoltage2 = (totalValue2 / numSamples) * (5.0 / 4095.0);

  offset1 = sensorVoltage1 - sensorVoltageMin;
  offset2 = sensorVoltage2 - sensorVoltageMin;
}

void loop() {
  static unsigned long lastSendTime = 0;
  unsigned long currentMillis = millis();

  // ส่งข้อมูลทุก 10 วินาที
  if (currentMillis - lastSendTime >= 20000) {
    lastSendTime = currentMillis;

    // ตรวจสอบการเชื่อมต่อ Wi-Fi ก่อนส่งข้อมูล
    if (WiFi.status() == WL_CONNECTED) {
      detachInterrupt(digitalPinToInterrupt(flowSensorPin));

      // คำนวณอัตราการไหล (Flow rate)
      float frequency = (float)pulseCount / 60.0;
      flowRate = frequency / calibrationFactor;

      // อ่านค่าแรงดันจากเซ็นเซอร์ที่ 1
      int analogValue1 = analogRead(pressureSensorPin1);
      float sensorVoltage1 = analogValue1 * (5.0 / 4095.0) - offset1;
      float pressure1 = ((sensorVoltage1 - sensorVoltageMin) * maxPressure) / (sensorVoltageMax - sensorVoltageMin);
      pressure1 = max((int)pressure1, 0);

      // อ่านค่าแรงดันจากเซ็นเซอร์ที่ 2
      int analogValue2 = analogRead(pressureSensorPin2);
      float sensorVoltage2 = analogValue2 * (5.0 / 4095.0) - offset2;
      float pressure2 = ((sensorVoltage2 - sensorVoltageMin) * maxPressure) / (sensorVoltageMax - sensorVoltageMin);
      pressure2 = max((int)pressure2, 0);

      // ส่งข้อมูล Flow rate
      if (flowRate > 0) {
        HTTPClient http;
        String flowData = "flowRate=" + String(flowRate);
        http.begin(serverNameFlow);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpResponseCode = http.POST(flowData);

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println("Flow Sensor response: " + response);
        } else {
          Serial.println("Error in sending POST to Flow Sensor: " + String(httpResponseCode));
        }
        http.end();
      }

      // ส่งข้อมูล Pressure sensor
      if (pressure1 > 1 || pressure2 > 1) {
        HTTPClient http;
        String pressureData = "pressure1=" + String(pressure1) + "&pressure2=" + String(pressure2);
        http.begin(serverNamePressure);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpResponseCode = http.POST(pressureData);

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println("Pressure Sensor response: " + response);
        } else {
          Serial.println("Error in sending POST to Pressure Sensor: " + String(httpResponseCode));
        }
        http.end();
      }

      // รีเซ็ต pulse count
      pulseCount = 0;
      attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, RISING);
    } else {
      Serial.println("WiFi not connected, attempting reconnection...");
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Reconnecting to WiFi...");
      }
      Serial.println("Reconnected to WiFi");
    }
  }
}
