#ifndef CRYPTO_MODULE_H
#define CRYPTO_MODULE_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

// Encryption constants and key management
constexpr size_t KEY_LEN   = 32;   // 256-bit key length
constexpr size_t NONCE_LEN = 12;   // 96-bit nonce length
constexpr size_t TAG_LEN   = 16;   // 128-bit tag length

struct KeyEntry {
    uint8_t key[KEY_LEN];
    const char* label;
};

// Global encryption keys
extern KeyEntry txKey;
extern KeyEntry rxKeys[];      // RX_KEYS_COUNT elements
extern const size_t RX_KEYS_COUNT;
extern const char* TX_SENDER_TAG;
extern const char* TX_KEY_LABEL;
extern const char* RX_KEYS_LABELS[];

// Crypto helper functions
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

// Frame encryption/decryption
bool buildEncryptedFrame(const String& plaintext, String& outFrame);
bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex, String& cHex, String& tHex);
bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain);

#endif // CRYPTO_MODULE_H
