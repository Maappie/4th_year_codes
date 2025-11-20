#include "crypto.hpp"
#include <cstring>      // for memcpy, strlen
#include <esp_system.h> // for esp_random
#include <memory>       // for std::unique_ptr

// Definition of global cryptographic variables
const char* TX_SENDER_TAG = "Radio 2";
const char* TX_KEY_LABEL  = "unit-002";
const char* RX_KEYS_LABELS[] = { "HQ-PH", "unit-001" };
const size_t RX_KEYS_COUNT = sizeof(RX_KEYS_LABELS) / sizeof(RX_KEYS_LABELS[0]);

KeyEntry txKey;
KeyEntry rxKeys[RX_KEYS_COUNT];

// Internal helper for nibble-to-hex conversion
static char nybble(uint8_t v) { 
    return (v < 10 ? '0' + v : 'a' + (v - 10)); 
}

String toHex(const uint8_t* data, size_t len) {
    String s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        s += nybble((data[i] >> 4) & 0x0F);
        s += nybble(data[i] & 0x0F);
    }
    return s;
}

bool fromHex(const String& hex, uint8_t* out, size_t outLen) {
    if (hex.length() != (int)(outLen * 2)) return false;
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (size_t i = 0; i < outLen; ++i) {
        int hi = hexVal(hex[2 * i]);
        int lo = hexVal(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool deriveKey(const char* label, uint8_t outKey[KEY_LEN]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256
    mbedtls_sha256_update_ret(&ctx, (const unsigned char*)label, strlen(label));
    mbedtls_sha256_finish_ret(&ctx, outKey);
    mbedtls_sha256_free(&ctx);
    return true;
}

void makeNonce(uint8_t nonce[NONCE_LEN]) {
    for (size_t i = 0; i < NONCE_LEN; i += 4) {
        uint32_t r = esp_random();
        size_t chunk = (4 < (NONCE_LEN - i) ? 4 : (NONCE_LEN - i));
        memcpy(nonce + i, &r, chunk);
    }
}

bool aesGcmEncrypt(const uint8_t key[KEY_LEN],
                   const uint8_t* plaintext, size_t plen,
                   const uint8_t* nonce, size_t nlen,
                   const uint8_t* aad, size_t aad_len,
                   uint8_t* ciphertext,
                   uint8_t* tag, size_t tlen) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8);
    if (rc != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }
    rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plen,
                                   nonce, nlen,
                                   aad, aad_len,
                                   plaintext, ciphertext,
                                   tlen, tag);
    mbedtls_gcm_free(&gcm);
    return (rc == 0);
}

bool aesGcmDecrypt(const uint8_t key[KEY_LEN],
                   const uint8_t* ciphertext, size_t clen,
                   const uint8_t* nonce, size_t nlen,
                   const uint8_t* aad, size_t aad_len,
                   const uint8_t* tag, size_t tlen,
                   uint8_t* plaintext_out) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8);
    if (rc != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }
    rc = mbedtls_gcm_auth_decrypt(&gcm, clen,
                                  nonce, nlen,
                                  aad, aad_len,
                                  tag, tlen,
                                  ciphertext, plaintext_out);
    mbedtls_gcm_free(&gcm);
    return (rc == 0);
}

bool buildEncryptedFrame(const String& plaintext, String& outFrame) {
    uint8_t nonce[NONCE_LEN];
    makeNonce(nonce);
    String nHex = toHex(nonce, NONCE_LEN);
    Serial.print("[TX NONCE] ");
    Serial.println(nHex);

    const size_t plen = plaintext.length();
    std::unique_ptr<uint8_t[]> pbuf(new uint8_t[plen]);
    memcpy(pbuf.get(), plaintext.c_str(), plen);

    std::unique_ptr<uint8_t[]> cbuf(new uint8_t[plen]);
    uint8_t tag[TAG_LEN];

    const uint8_t* aad = (const uint8_t*)TX_SENDER_TAG;
    size_t aad_len = strlen(TX_SENDER_TAG);

    bool ok = aesGcmEncrypt(txKey.key, pbuf.get(), plen,
                             nonce, NONCE_LEN,
                             aad, aad_len,
                             cbuf.get(), tag, TAG_LEN);
    if (!ok) {
        Serial.println("[ENC] AES-GCM encrypt failed");
        return false;
    }

    outFrame  = "ENC|tag=";
    outFrame += TX_SENDER_TAG;
    outFrame += "|n=";
    outFrame += nHex;
    outFrame += "|c=";
    outFrame += toHex(cbuf.get(), plen);
    outFrame += "|t=";
    outFrame += toHex(tag, TAG_LEN);
    return true;
}

bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex, String& cHex, String& tHex) {
    if (!s.startsWith("ENC|")) return false;
    int p1 = s.indexOf("|tag=");
    int p2 = s.indexOf("|n=");
    int p3 = s.indexOf("|c=");
    int p4 = s.indexOf("|t=");
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return false;

    int tagStart = p1 + 5;
    senderTag = s.substring(tagStart, p2);

    int nStart = p2 + 3;
    nHex = s.substring(nStart, p3);

    int cStart = p3 + 3;
    cHex = s.substring(cStart, p4);

    int tStart = p4 + 3;
    tHex = s.substring(tStart);
    return true;
}

bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain) {
    uint8_t nonce[NONCE_LEN];
    if (!fromHex(nonceHex, nonce, NONCE_LEN)) return false;
    size_t clen = cHex.length() / 2;
    std::unique_ptr<uint8_t[]> cbuf(new uint8_t[clen]);
    if (!fromHex(cHex, cbuf.get(), clen)) return false;
    uint8_t tag[TAG_LEN];
    if (!fromHex(tHex, tag, TAG_LEN)) return false;
    std::unique_ptr<uint8_t[]> pbuf(new uint8_t[clen]);

    const uint8_t* aad = (const uint8_t*)senderTag.c_str();
    size_t aad_len = senderTag.length();

    bool ok = aesGcmDecrypt(k.key, cbuf.get(), clen,
                             nonce, NONCE_LEN,
                             aad, aad_len,
                             tag, TAG_LEN,
                             pbuf.get());
    if (!ok) {
        return false;
    }
    outPlain = String((const char*)pbuf.get(), clen);
    return true;
}
