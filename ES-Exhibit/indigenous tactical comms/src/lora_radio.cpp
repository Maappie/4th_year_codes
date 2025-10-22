#include "lora_radio.hpp"
#include "app_config.h"
#include "crypto.hpp"
#include "storage.hpp"
#include "ui.hpp"
#include "display.hpp"    

#include <SPI.h>
#include <LoRa.h>
#include <memory>


String lastRx = "";
String rxFull  = "";

static bool loraSendEncrypted(const String& plaintext){
  uint8_t nonce[NONCE_LEN]; 
  makeNonce(nonce);

  String nHex = toHex(nonce, NONCE_LEN);
  Serial.print("[TX NONCE] "); Serial.println(nHex);

  const size_t plen = plaintext.length();
  std::unique_ptr<uint8_t[]> pbuf(new uint8_t[plen]);
  memcpy(pbuf.get(), plaintext.c_str(), plen);

  std::unique_ptr<uint8_t[]> cbuf(new uint8_t[plen]);
  uint8_t tag[TAG_LEN];

  const uint8_t* aad = (const uint8_t*)TX_SENDER_TAG;
  size_t aad_len = strlen(TX_SENDER_TAG);

  bool ok = aesGcmEncrypt(txKey.key, pbuf.get(), plen, nonce, NONCE_LEN, aad, aad_len, cbuf.get(), tag, TAG_LEN);
  if(!ok){
    Serial.println("[ENC] AES-GCM encrypt failed");
    return false;
  }

  String frame = "ENC|tag="; frame += TX_SENDER_TAG;
  frame += "|n="; frame += nHex;
  frame += "|c="; frame += toHex(cbuf.get(), plen);
  frame += "|t="; frame += toHex(tag, TAG_LEN);

  Serial.print("[TX] "); Serial.println(frame);
  LoRa.beginPacket();
  LoRa.print(frame);
  LoRa.endPacket();
  return true;
}

void beginLoRa() {
  SPI.begin(18, 19, 23);
  LoRa.setPins(PIN_LORA_SS, PIN_LORA_RST, PIN_LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed!");
  }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  LoRa.setSyncWord(0x34);
  LoRa.disableInvertIQ();
}

void loraSend(const String& s) {
  if(loraSendEncrypted(s)){
    Serial.print("[TX PLAINTEXT] "); Serial.println(s);
  }else{
    Serial.println("[WARN] Fallback to plaintext TX");
    LoRa.beginPacket(); LoRa.print(s); LoRa.endPacket();
  }
}

void handleLoRaRx() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String incoming = "";
  while (LoRa.available()) incoming += (char)LoRa.read();

  String shown = incoming;
  bool handledEncrypted = false;

  if(incoming.startsWith("ENC|")){
    String tag, nHex, cHex, tHex;
    if(parseEncryptedFrame(incoming, tag, nHex, cHex, tHex)){
      Serial.print("[RX NONCE] ");
      Serial.println(nHex);

      for(size_t i=0;i<g_rxKeysCount;i++){
        String plain;
        if(tryDecryptWithKey(rxKeys[i], tag, nHex, cHex, tHex, plain)){
          shown = plain; handledEncrypted = true;

          addLogEntryPersistent(tag, plain, nHex);

          Serial.printf("[RX] (AUTH) key=%s RSSI=%d SNR=%.1f\n",
                        rxKeys[i].label, LoRa.packetRssi(), LoRa.packetSnr());
          break;
        }
      }
      if(!handledEncrypted){
        shown = "[AUTH FAIL] Unable to decrypt";
        Serial.printf("[RX] (AUTH FAIL) RSSI=%d SNR=%.1f\n",
                      LoRa.packetRssi(), LoRa.packetSnr());
      }
    }else{
      shown = "[PARSE FAIL] Bad ENC frame";
    }
  }else{
    Serial.printf("[RX] (PLAINTEXT) RSSI=%d SNR=%.1f\n",
                  LoRa.packetRssi(), LoRa.packetSnr());
  }

  lastRx = shown;
  rxFull = shown;
  screen = SCREEN_RX;
  drawRxScreen();
}
