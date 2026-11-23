/* crypto.h — optional transport encryption: ChaCha20 (RFC 8439) applied transparently to a socket */
#ifndef CRYPTO_H
#define CRYPTO_H
#include <stdint.h>
#include <stddef.h>
int  crypt_enable(int fd, const uint8_t key[32], int is_initiator);
void crypt_disable(int fd);
int  crypt_active(int fd);
void crypt_tx(int fd, const void *in, void *out, size_t n);
void crypt_rx(int fd, void *buf, size_t n);
void crypt_derive_key(const char *pin, const uint8_t nonce[16], uint8_t key[32]);
int  crypt_selftest(void);
#endif
