#pragma once
#include <Arduino.h>

// AES-GCM parameters
static const size_t KEY_LEN   = 32;  // 256-bit (SHA256 output)
static const size_t NONCE_LEN = 12;  // 96-bit nonce
static const size_t TAG_LEN   = 16;  // 128-bit tag

struct KeyEntry {
  uint8_t key[KEY_LEN];
  const char* label;
};

// Globals (defined in crypto.cpp)
extern KeyEntry txKey;
extern KeyEntry rxKeys[];
extern const size_t g_rxKeysCount;

// Helpers
char nybble(uint8_t v);
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

// Frame helpers
bool parseEncryptedFrame(const String& s, String& senderTag,
                         String& nHex, String& cHex, String& tHex);
bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain);
