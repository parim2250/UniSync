/* sha256.h — SHA-256 and HMAC-SHA-256 written from scratch (FIPS 180-4 / RFC 2104) */
#ifndef SHA256_H
#define SHA256_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t st[8]; uint64_t bits; uint8_t buf[64]; size_t len; } sha256_ctx;
void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const uint8_t *d, size_t n);
void sha256_final(sha256_ctx *c, uint8_t out[32]);
void hmac_sha256(const uint8_t *key, size_t klen, const uint8_t *msg, size_t mlen, uint8_t out[32]);
int  sha256_file(const char *path, uint8_t out[32], uint64_t *size_out);
void hex_encode(const uint8_t *in, size_t n, char *out);
#endif
