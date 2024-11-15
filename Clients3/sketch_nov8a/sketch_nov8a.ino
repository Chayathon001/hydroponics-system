#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// ข้อมูล Wi-Fi
const char* ssid = "";
const char* password = "";

// ตั้งค่าขารีเลย์
const int relayPin1 = 26;  // ขารีเลย์ตัวที่หนึ่ง
const int relayPin2 = 14;  // ขารีเลย์ตัวที่สอง

// สร้าง Web Server บนพอร์ต 80
WebServer server(80);

// ตั้งค่า NTP Server และ Timezone (GMT+7)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

bool relayState1 = true;  // สถานะรีเลย์ 1 (เริ่มต้นเป็นเปิด)
bool relayState2 = true;  // สถานะรีเลย์ 2 (เริ่มต้นเป็นเปิด)
unsigned long lastRelay1OnTime = 0;  // เวลาที่เปิดรีเลย์ 1 ล่าสุด
unsigned long lastRelay2OnTime = 0;  // เวลาที่เปิดรีเลย์ 2 ล่าสุด

void setup() {
  // เริ่มต้นการสื่อสาร Serial
  Serial.begin(115200);
  
  // ตั้งค่าขารีเลย์เป็น OUTPUT
  pinMode(relayPin1, OUTPUT);
  digitalWrite(relayPin1, LOW);  // เริ่มต้นให้รีเลย์ 1 เปิด
  
  pinMode(relayPin2, OUTPUT);  
  digitalWrite(relayPin2, LOW);  // เริ่มต้นให้รีเลย์ 2 เปิด

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ซิงค์เวลา NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for NTP time sync...");
  while (!time(nullptr)) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nTime synchronized!");

  // กำหนด Route สำหรับเปิดรีเลย์ 1
  server.on("/relay1/on", []() {
    digitalWrite(relayPin1, HIGH);  // เปิดรีเลย์ 1
    relayState1 = true;
    lastRelay1OnTime = millis();  // บันทึกเวลาที่เปิดรีเลย์ 1
    server.send(200, "text/plain", "Relay 1 is ON");
  });

  // กำหนด Route สำหรับปิดรีเลย์ 1
  server.on("/relay1/off", []() {
    digitalWrite(relayPin1, LOW);  // ปิดรีเลย์ 1
    relayState1 = false;
    server.send(200, "text/plain", "Relay 1 is OFF");
  });

  // กำหนด Route สำหรับเปิดรีเลย์ 2
  server.on("/relay2/on", []() {
    digitalWrite(relayPin2, HIGH);  // เปิดรีเลย์ 2
    relayState2 = true;
    lastRelay2OnTime = millis();  // บันทึกเวลาที่เปิดรีเลย์ 2
    server.send(200, "text/plain", "Relay 2 is ON");
  });

  // กำหนด Route สำหรับปิดรีเลย์ 2
  server.on("/relay2/off", []() {
    digitalWrite(relayPin2, LOW);  // ปิดรีเลย์ 2
    relayState2 = false;
    server.send(200, "text/plain", "Relay 2 is OFF");
  });

  // เริ่มต้น Web Server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  // ตรวจสอบการเชื่อมต่อ Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    WiFi.begin(ssid, password);
    // รอจนกว่าจะเชื่อมต่อใหม่
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Reconnecting to WiFi...");
    }
    Serial.println("Reconnected to WiFi");
  }

  // ตรวจสอบเวลา
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;

  // ตรวจสอบช่วงเวลาที่ต้องเปิดรีเลย์ 1 อัตโนมัติ
  bool shouldRelay1BeOn = (
    (currentHour == 8 && currentMinute >= 00 && currentMinute < 01) ||
    (currentHour == 10 && currentMinute >= 00 && currentMinute < 03) ||
    (currentHour == 12 && currentMinute >= 00 && currentMinute < 10) ||
    (currentHour == 14 && currentMinute >= 00 && currentMinute < 05) ||
    (currentHour == 16 && currentMinute >= 30 && currentMinute < 35)
  );

  if (shouldRelay1BeOn) {
    if (!relayState1) {
      digitalWrite(relayPin1, HIGH);  // เปิดรีเลย์ 1
      relayState1 = true;
      Serial.println("Relay 1 turned ON (Auto)");
    }
  } else if (relayState1 && millis() - lastRelay1OnTime >= 120000) {  // ปิดหลัง 1 นาที
    digitalWrite(relayPin1, LOW);  // ปิดรีเลย์ 1
    relayState1 = false;
    Serial.println("Relay 1 turned OFF (Auto Timeout)");
  }

  // ตรวจสอบช่วงเวลาที่ต้องเปิดรีเลย์ 2 อัตโนมัติ
  bool shouldRelay2BeOn = (
    (currentHour == 8 && currentMinute >= 00 && currentMinute < 01) ||
    (currentHour == 10 && currentMinute >= 00 && currentMinute < 03) ||
    (currentHour == 12 && currentMinute >= 00 && currentMinute < 10) ||
    (currentHour == 14 && currentMinute >= 00 && currentMinute < 05) ||
    (currentHour == 16 && currentMinute >= 30 && currentMinute < 35)
  );

  if (shouldRelay2BeOn) {
    if (!relayState2) {
      digitalWrite(relayPin2, HIGH);  // เปิดรีเลย์ 2
      relayState2 = true;
      Serial.println("Relay 2 turned ON (Auto)");
    }
  } else if (relayState2 && millis() - lastRelay2OnTime >= 120000) {  // ปิดหลัง 2 นาที
    digitalWrite(relayPin2, LOW);  // ปิดรีเลย์ 2
    relayState2 = false;
    Serial.println("Relay 2 turned OFF (Auto Timeout)");
  }

  // ตรวจสอบคำขอจาก Web Server
  server.handleClient();

  delay(500);  
}
