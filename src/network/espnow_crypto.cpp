#include "network/espnow_crypto.h"

#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h>
#include <string.h>

#ifdef FLUIDTOUCH_SIM
#include <random>
#else
#include <esp_random.h>
#endif

namespace EspNowCrypto {

void randomBytes(uint8_t* out, size_t len) {
#ifdef FLUIDTOUCH_SIM
    static std::random_device rd;
    for (size_t i = 0; i < len; i++) {
        out[i] = (uint8_t)rd();
    }
#else
    esp_fill_random(out, len);
#endif
}

uint32_t randomNonZero32() {
    uint32_t v = 0;
    while (v == 0) {
        randomBytes((uint8_t*)&v, sizeof(v));
    }
    return v;
}

// mbedtls RNG callback
static int rngCallback(void*, unsigned char* out, size_t len) {
    randomBytes(out, len);
    return 0;
}

void keyFromLabel(const char* label, uint8_t out[KEY_SIZE]) {
    uint8_t digest[32];
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char*)label, strlen(label), digest);
    memcpy(out, digest, KEY_SIZE);
    wipe(digest, sizeof(digest));
}

void hmac16(const uint8_t* key, size_t key_len, const Bytes* parts, size_t count, uint8_t out[TAG_SIZE]) {
    uint8_t digest[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, key_len);
    for (size_t i = 0; i < count; i++) {
        mbedtls_md_hmac_update(&ctx, (const unsigned char*)parts[i].data, parts[i].len);
    }
    mbedtls_md_hmac_finish(&ctx, digest);
    mbedtls_md_free(&ctx);
    memcpy(out, digest, TAG_SIZE);
    wipe(digest, sizeof(digest));
}

bool generateKeyPair(uint8_t private_key[PRIVATE_KEY_SIZE], uint8_t public_key[PUBLIC_KEY_SIZE]) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&q);

    size_t written = 0;
    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
              mbedtls_ecdh_gen_public(&grp, &d, &q, rngCallback, nullptr) == 0 &&
              mbedtls_mpi_write_binary(&d, private_key, PRIVATE_KEY_SIZE) == 0 &&
              mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &written, public_key,
                                             PUBLIC_KEY_SIZE) == 0 &&
              written == PUBLIC_KEY_SIZE;

    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    if (!ok) {
        wipe(private_key, PRIVATE_KEY_SIZE);
    }
    return ok;
}

bool sharedSecret(const uint8_t private_key[PRIVATE_KEY_SIZE], const uint8_t peer_public_key[PUBLIC_KEY_SIZE],
                  uint8_t out[SECRET_SIZE]) {
    if (peer_public_key[0] != 0x04) {
        return false;
    }
    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&q);

    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
              mbedtls_mpi_read_binary(&d, private_key, PRIVATE_KEY_SIZE) == 0 &&
              mbedtls_ecp_point_read_binary(&grp, &q, peer_public_key, PUBLIC_KEY_SIZE) == 0 &&
              mbedtls_ecp_check_pubkey(&grp, &q) == 0 &&
              mbedtls_ecdh_compute_shared(&grp, &z, &q, &d, rngCallback, nullptr) == 0 &&
              mbedtls_mpi_write_binary(&z, out, SECRET_SIZE) == 0;

    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&z);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    if (!ok) {
        wipe(out, SECRET_SIZE);
    }
    return ok;
}

bool equal(const void* a, const void* b, size_t len) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= pa[i] ^ pb[i];
    }
    return diff == 0;
}

void wipe(void* data, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)data;
    while (len--) {
        *p++ = 0;
    }
}

}  // namespace EspNowCrypto
