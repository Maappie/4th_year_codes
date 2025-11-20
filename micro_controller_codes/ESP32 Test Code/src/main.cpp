#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "ESP32_Test";
const char* password = "12345678";

AsyncWebServer server(80);
unsigned long lastLogTime = 0;
const unsigned long logInterval = 3000; // 3 seconds
const char* logFile = "/data.txt";

// 🧩 Print ALL storage info (LittleFS + Flash + Heap)
void printFullStorageInfo() {
  Serial.println("\n===== ESP32 STORAGE INFORMATION =====");
  
  // ----- Flash (program memory) -----
  Serial.printf("Flash Chip Size      : %u bytes (%.2f MB)\n", ESP.getFlashChipSize(), ESP.getFlashChipSize() / 1048576.0);
  Serial.printf("Flash Frequency      : %u Hz\n", ESP.getFlashChipSpeed());
  Serial.printf("Sketch Size          : %u bytes\n", ESP.getSketchSize());
  Serial.printf("Free Sketch Space    : %u bytes\n", ESP.getFreeSketchSpace());
  
  // ----- LittleFS -----
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  Serial.println("\n----- LittleFS Info -----");
  Serial.printf("Total Space: %u bytes\n", totalBytes);
  Serial.printf("Used Space : %u bytes\n", usedBytes);
  Serial.printf("Free Space : %u bytes\n", totalBytes - usedBytes);

  // ----- RAM / Heap -----
  Serial.println("\n----- RAM (Heap) Info -----");
  Serial.printf("Heap Total   : %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap Free    : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap Minimum : %u bytes (lowest ever)\n", ESP.getMinFreeHeap());
  
  Serial.println("=====================================\n");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting ESP32...");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }

  // Print initial storage details
  printFullStorageInfo();

  // Start Wi-Fi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("ESP32 Wi-Fi started");
  Serial.print("Access this web page at: http://");
  Serial.println(WiFi.softAPIP());

  // Serve the web dashboard
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><head><meta http-equiv='refresh' content='2'>";
    html += "<style>body{font-family:Arial;background:#fafafa;margin:40px;}pre{background:#fff;padding:10px;border-radius:10px;white-space:pre-wrap;}</style>";
    html += "<h2>📊 ESP32 LittleFS Log Viewer</h2><pre>";

    File file = LittleFS.open("/data.txt", "r");
    if (!file) {
      html += "No log data yet.";
    } else {
      while (file.available()) {
        html += file.readStringUntil('\n');  // Each line appears as a new row
      }
      file.close();
    }

    html += "</pre></html>";
    request->send(200, "text/html", html);
  });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLogTime >= logInterval) {
    lastLogTime = currentMillis;

    // Open the file in append mode
    File file = LittleFS.open(logFile, FILE_APPEND);
    if (!file) {
      Serial.println("Failed to open log file for writing");
      return;
    }

    // Write data as a new line (row)
    String logEntry = "Time (ms): " + String(currentMillis) + "\n";
    file.print(logEntry);
    file.close();

    Serial.print("Logged: ");
    Serial.println(logEntry);

    // Print full storage info every 10 logs
    static int counter = 0;
    if (++counter >= 10) {
      printFullStorageInfo();
      counter = 0;
    }
  }
}
