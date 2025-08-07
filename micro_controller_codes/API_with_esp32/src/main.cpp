#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>  // Required for getLocalTime()

const char* ssid = "Fake Extender";                    // ← your 2.4GHz Wi-Fi
const char* password = "Aa1231325213!";               // ← your password
const char* serverName = "http://192.168.68.123:3000/presses";  // ← your PC's IP + Rails port

const int buttonPin = 25;  // GPIO where your button is connected
bool lastButtonState = HIGH;
String getTimestamp();  // <--- put this at the top, before sendPost()

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);  // ISO format
  return String(buffer);
}

void sendPost() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String timestamp = getTimestamp();
    String jsonData = "{\"device_id\":\"ESP32_REAL\",\"pressed_at\":\"" + timestamp + "\"}";

    int httpResponseCode = http.POST(jsonData);
    Serial.print("Response: ");
    Serial.println(httpResponseCode);
    http.end();
  } else {
    Serial.println("WiFi not connected.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");

  // NTP time config
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" done!");
}

void loop() {
  bool currentState = digitalRead(buttonPin);

  if (lastButtonState == HIGH && currentState == LOW) {
    Serial.println("Button pressed!");
    sendPost();
    delay(200); // debounce
  }

  lastButtonState = currentState;
}
