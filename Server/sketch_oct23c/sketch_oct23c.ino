#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

// ข้อมูล Wi-Fi
IPAddress local_IP(192, 168, 1, 101);  // IP ที่ต้องการให้เป็น static IP
IPAddress gateway(192, 168, 1, 1);    // Gateway ของ router
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
const char* ssid = "TOT_POR_2.4G";
const char* password = "0623080540"; 

// ข้อมูลเกี่ยวกับ MySQL และ PHP Server
const char* serverName1 = "http://192.168.1.100/insert_data.php";
const char* serverName2 = "http://192.168.1.100/insert1_data.php"; 

WiFiUDP udp;
unsigned int localUdpPort = 4210; 
char incomingPacket[255]; 

HTTPClient http; // สร้าง instance ของ HTTPClient

void setup() {
  Serial.begin(115200);

  // กำหนดค่า static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // เริ่มต้นการทำงานของ UDP
  udp.begin(localUdpPort);
  Serial.printf("UDP server started at port %d\n", localUdpPort);
}

void loop() {
  // ตรวจสอบ Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    WiFi.reconnect();
    delay(5000);
  } else {
    // ตรวจสอบว่ามี packet UDP เข้ามาหรือไม่
    int packetSize = udp.parsePacket(); 
    if (packetSize) {
      int len = udp.read(incomingPacket, 255);
      if (len > 0) {
        incomingPacket[len] = 0;
        Serial.printf("Received packet: %s\n", incomingPacket);

        // ตรวจสอบข้อมูลที่รับและเลือก server ที่จะส่งข้อมูล
        String httpRequestData = "data=" + String(incomingPacket);
        String serverName;

        if (String(incomingPacket).startsWith("Client 1")) {
          serverName = serverName1;
        } else if (String(incomingPacket).startsWith("Client 2")) {
          serverName = serverName2;
        } else {
          Serial.println("Unknown packet type.");
          return; // ถ้าไม่มีประเภทที่รู้จัก ไม่ต้องส่ง
        }

        // ส่งข้อมูลไปยัง server ที่เหมาะสม
        http.begin(serverName);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println("Server response: " + response);
        } else {
          Serial.println("Error in sending POST: " + String(httpResponseCode));
        }
        http.end();
      }
    }
  }

  delay(100); // หน่วงเวลาเล็กน้อยก่อนเช็ค packet ใหม่
}
