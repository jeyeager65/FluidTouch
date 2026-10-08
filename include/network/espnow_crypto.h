#ifndef ESPNOW_CRYPTO_H
#define ESPNOW_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

// Cryptographic helpers for the FluidNC ESP-NOW pendant protocol (FluidNC
// v4.0.4+). Backed by mbedtls on both the ESP32 and the desktop simulator.
namespace EspNowCrypto {

constexpr size_t PRIVATE_KEY_SIZE = 32;  // P-256 scalar
constexpr size_t PUBLIC_KEY_SIZE  = 65;  // P-256 point, uncompressed (0x04 || X || Y)
constexpr size_t SECRET_SIZE      = 32;  // ECDH shared secret (X coordinate)
constexpr size_t KEY_SIZE         = 16;  // ESP-NOW PMK/LMK size
constexpr size_t TAG_SIZE         = 16;  // Truncated HMAC-SHA256

struct Bytes {
    const void* data;
    size_t len;
};

void randomBytes(uint8_t* out, size_t len);
uint32_t randomNonZero32();

// First 16 bytes of SHA-256(label)
void keyFromLabel(const char* label, uint8_t out[KEY_SIZE]);

// First 16 bytes of HMAC-SHA256(key, parts[0] || parts[1] || ...)
void hmac16(const uint8_t* key, size_t key_len, const Bytes* parts, size_t count, uint8_t out[TAG_SIZE]);

bool generateKeyPair(uint8_t private_key[PRIVATE_KEY_SIZE], uint8_t public_key[PUBLIC_KEY_SIZE]);
bool sharedSecret(const uint8_t private_key[PRIVATE_KEY_SIZE], const uint8_t peer_public_key[PUBLIC_KEY_SIZE],
                  uint8_t out[SECRET_SIZE]);

// Constant-time comparison
bool equal(const void* a, const void* b, size_t len);
// Zeroing the compiler can't optimize away
void wipe(void* data, size_t len);

}  // namespace EspNowCrypto

#endif  // ESPNOW_CRYPTO_H
