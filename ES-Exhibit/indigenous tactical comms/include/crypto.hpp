#pragma once
#include "app_config.h"
#include <Arduino.h>

// --------- CRYPTO CONSTANTS / HELPERS ---------
static const size_t KEY_LEN   = 32;   // 256-bit key (SHA256 output length)
static const size_t NONCE_LEN = 12;   // 96-bit nonce (AES-GCM)
static const size_t TAG_LEN   = 16;   // 128-bit authentication tag

struct KeyEntry {
    uint8_t key[KEY_LEN];
    const char* label;
};

extern KeyEntry txKey;
extern KeyEntry rxKeys[RX_KEYS_COUNT];

// Crypto helper function declarations
String toHex(const uint8_t* data, size_t len);
bool fromHex(const String& hex, uint8_t* out, size_t outLen);
void deriveKey(const char* label, uint8_t outKey[KEY_LEN]);
void makeNonce(uint8_t nonce[NONCE_LEN]);
bool aesGcmEncrypt(const uint8_t key[KEY_LEN],
                   const uint8_t* plaintext, size_t plen,
                   const uint8_t* nonce, size_t nlen,
                   const uint8_t* aad, size_t aad_len,
                   uint8_t* ciphertext,
                   uint8_t* tag, size_t tlen);
bool aesGcmDecrypt(const uint8_t key[KEY_LEN],
                   const uint8_t* ciphertext, size_t clen,
                   const uint8_t* nonce, size_t nlen,
                   const uint8_t* aad, size_t aad_len,
                   const uint8_t* tag, size_t tlen,
                   uint8_t* plaintext_out);
bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain);
bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex,
                         String& cHex, String& tHex);
