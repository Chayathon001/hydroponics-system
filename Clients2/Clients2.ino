#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

// I2C address ของ SHT20 คือ 0x40
#define SHT20_ADDR 0x40

const char* ssid = "TOT_POR_2.4G";
const char* password = "0623080540";
const char* serverIp = "192.168.1.101"; // ใส่ IP ของ ESP32 Server
unsigned int serverPort = 4210; // พอร์ตของ Server

WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  
  // เชื่อมต่อ Wi-Fi
  connectToWiFi();

  Wire.begin(); // เริ่มต้นการทำงานของ I2C
}

void loop() {
  // ตรวจสอบการเชื่อมต่อ Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    connectToWiFi();
  }

  float temperature = readTemperature();
  float humidity = readHumidity();

  if (temperature != 0 && humidity != 0) {
    String message = "Client 2 - Temperature: " + String(temperature) + " °C, Humidity: " + String(humidity) + " %";

    // ส่งข้อมูลไปยัง Server
    udp.beginPacket(serverIp, serverPort);
    udp.print(message);
    udp.endPacket();

    Serial.println("Sent data to server: " + message);
  } else {
    Serial.println("Failed to read temperature or humidity.");
  }

  delay(300000); // รอ 5 นาที (300000 milliseconds) ก่อนส่งข้อมูลอีกครั้ง
}

// ฟังก์ชันสำหรับเชื่อมต่อ Wi-Fi
void connectToWiFi() {
  // กำหนด Static IP, Gateway และ Subnet Mask
  IPAddress localIP(192, 168, 1, 103);  // Static IP ที่ต้องการ
  IPAddress gateway(192, 168, 1, 1);  // Gateway ของคุณ (อาจเป็น router ของคุณ)
  IPAddress subnet(255, 255, 255, 0); // Subnet Mask ของคุณ

  WiFi.config(localIP, gateway, subnet); // ตั้งค่า Static IP

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ฟังก์ชันสำหรับอ่านค่าอุณหภูมิ
float readTemperature() {
  Wire.beginTransmission(SHT20_ADDR);
  Wire.write(0xF3);
  Wire.endTransmission();
  delay(100);
  Wire.requestFrom(SHT20_ADDR, 2);
  if (Wire.available() == 2) {
    int temp = Wire.read() << 8 | Wire.read();
    return -46.85 + 175.72 * (temp / 65536.0);
  }
  return 0; // คืนค่า 0 หากอ่านไม่สำเร็จ
}

// ฟังก์ชันสำหรับอ่านค่าความชื้น
float readHumidity() {
  Wire.beginTransmission(SHT20_ADDR);
  Wire.write(0xF5);
  Wire.endTransmission();
  delay(100);
  Wire.requestFrom(SHT20_ADDR, 2);
  if (Wire.available() == 2) {
    int hum = Wire.read() << 8 | Wire.read();
    return -6.0 + 125.0 * (hum / 65536.0);
  }
  return 0; // คืนค่า 0 หากอ่านไม่สำเร็จ
}
