#pragma once
#include <Arduino.h>
#include <mbedtls/sha256.h>
#include <mbedtls/gcm.h>

// Length constants for cryptography
static const size_t KEY_LEN   = 32;  // 256-bit key (SHA256 output length)
static const size_t NONCE_LEN = 12;  // 96-bit nonce (GCM standard)
static const size_t TAG_LEN   = 16;  // 128-bit authentication tag

// Key entry structure for storing keys and their labels
struct KeyEntry {
    uint8_t key[KEY_LEN];
    const char* label;
};

// Global key storage: one TX key and multiple RX keys
extern KeyEntry txKey;
extern KeyEntry rxKeys[];        // size will be defined in crypto.cpp
extern const size_t RX_KEYS_COUNT;

// Key labels and identifiers
extern const char* TX_SENDER_TAG;
extern const char* TX_KEY_LABEL;
extern const char* RX_KEYS_LABELS[];

// Cryptographic utility functions
bool deriveKey(const char* label, uint8_t outKey[KEY_LEN]);
void makeNonce(uint8_t nonce[NONCE_LEN]);
bool aesGcmEncrypt(const uint8_t key[KEY_LEN], const uint8_t* plaintext, size_t plen,
                   const uint8_t* nonce, size_t nlen, const uint8_t* aad, size_t aad_len,
                   uint8_t* ciphertext, uint8_t* tag, size_t tlen);
bool aesGcmDecrypt(const uint8_t key[KEY_LEN], const uint8_t* ciphertext, size_t clen,
                   const uint8_t* nonce, size_t nlen, const uint8_t* aad, size_t aad_len,
                   const uint8_t* tag, size_t tlen, uint8_t* plaintext_out);

// Conversion utilities between byte arrays and hex strings
String toHex(const uint8_t* data, size_t len);
bool fromHex(const String& hex, uint8_t* out, size_t outLen);

// Encryption frame building and parsing
bool buildEncryptedFrame(const String& plaintext, String& outFrame);
bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex, String& cHex, String& tHex);

// Attempt decryption with a given key
bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain);
