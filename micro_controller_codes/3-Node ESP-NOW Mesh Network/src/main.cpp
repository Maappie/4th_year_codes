#include <esp_now.h>
#include <WiFi.h>

#define LED_PIN 2

uint8_t nodeA[] = {0x20, 0xE7, 0xC8, 0x59, 0x75, 0xA8};
uint8_t nodeB[] = {0x20, 0xE7, 0xC8, 0x59, 0x7F, 0x84};
uint8_t nodeC[] = {0x20, 0xE7, 0xC8, 0x59, 0x73, 0xA0};

uint8_t thisNode[6];

struct Message {
  char sender[18];
  uint8_t hops;
  char msg[50];
};

void onReceive(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Message msg;
  memcpy(&msg, incomingData, sizeof(msg));

  if (strcmp(msg.sender, "Node C") == 0) return;

  Serial.print("Received from ");
  Serial.print(msg.sender);
  Serial.print(" | Hops: ");
  Serial.print(msg.hops);
  Serial.print(" | Msg: ");
  Serial.println(msg.msg);

  msg.hops++;
  if (msg.hops < 3) {
    if (memcmp(mac, nodeA, 6) != 0) esp_now_send(nodeA, (uint8_t*)&msg, sizeof(msg));
    if (memcmp(mac, nodeB, 6) != 0) esp_now_send(nodeB, (uint8_t*)&msg, sizeof(msg));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  WiFi.mode(WIFI_STA);

  memcpy(thisNode, nodeC, 6);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_peer_info_t peer;

  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, nodeA, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("Failed to add Node A");

  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, nodeB, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("Failed to add Node B");

  esp_now_register_recv_cb(onReceive);
}

void loop() {
  Message message;
  strcpy(message.sender, "Node C");
  message.hops = 0;
  strcpy(message.msg, "Hello from Node C");

  esp_now_send(nodeA, (uint8_t*)&message, sizeof(message));
  esp_now_send(nodeB, (uint8_t*)&message, sizeof(message));

  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  delay(5000);
}
          