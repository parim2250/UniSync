/* Optional transport encryption: ChaCha20 stream cipher (RFC 8439), implemented from scratch.
 * Once enabled for a socket, net_send()/net_recv() transparently encrypt/decrypt every byte.
 * Session key = HMAC-SHA256(PIN, "unisync-enc-v1" || nonce)  -> unique for every connection.   */
#include "common.h"
#include "sha256.h"
#include "crypto.h"

#define ROL(x,n) (((x) << (n)) | ((x) >> (32 - (n))))
#define QR(a,b,c,d) a+=b; d^=a; d=ROL(d,16); c+=d; b^=c; b=ROL(b,12); a+=b; d^=a; d=ROL(d,8); c+=d; b^=c; b=ROL(b,7);

typedef struct { uint32_t st[16]; uint8_t ks[64]; int pos; } chacha_t;
typedef struct { chacha_t tx, rx; } crypt_ctx;

#define MAXFD 4096
static crypt_ctx *tab[MAXFD];

static void le32(uint8_t *o, uint32_t v) { o[0]=(uint8_t)v; o[1]=(uint8_t)(v>>8); o[2]=(uint8_t)(v>>16); o[3]=(uint8_t)(v>>24); }
static uint32_t rd32(const uint8_t *p) { return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

static void block(const uint32_t in[16], uint8_t out[64])
{
    uint32_t x[16]; memcpy(x, in, sizeof x);
    for (int i = 0; i < 10; i++) {
        QR(x[0],x[4],x[8],x[12]) QR(x[1],x[5],x[9],x[13]) QR(x[2],x[6],x[10],x[14]) QR(x[3],x[7],x[11],x[15])
        QR(x[0],x[5],x[10],x[15]) QR(x[1],x[6],x[11],x[12]) QR(x[2],x[7],x[8],x[13]) QR(x[3],x[4],x[9],x[14])
    }
    for (int i = 0; i < 16; i++) le32(out + 4 * i, x[i] + in[i]);
}

static void chacha_init(chacha_t *c, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter)
{
    c->st[0]=0x61707865; c->st[1]=0x3320646e; c->st[2]=0x79622d32; c->st[3]=0x6b206574;
    for (int i = 0; i < 8; i++) c->st[4 + i] = rd32(key + 4 * i);
    c->st[12] = counter;
    for (int i = 0; i < 3; i++) c->st[13 + i] = rd32(nonce + 4 * i);
    c->pos = 64;
}

static void chacha_xor(chacha_t *c, const uint8_t *in, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (c->pos == 64) { block(c->st, c->ks); if (++c->st[12] == 0) c->st[13]++; c->pos = 0; }
        out[i] = in[i] ^ c->ks[c->pos++];
    }
}

int crypt_enable(int fd, const uint8_t key[32], int is_initiator)
{
    if (fd < 0 || fd >= MAXFD) return -1;
    crypt_ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    uint8_t n1[12] = {1}, n2[12] = {2};                 /* one keystream per direction */
    chacha_init(&c->tx, key, is_initiator ? n1 : n2, 0);
    chacha_init(&c->rx, key, is_initiator ? n2 : n1, 0);
    tab[fd] = c;
    return 0;
}

void crypt_disable(int fd) { if (fd >= 0 && fd < MAXFD && tab[fd]) { free(tab[fd]); tab[fd] = NULL; } }
int  crypt_active(int fd)  { return fd >= 0 && fd < MAXFD && tab[fd] != NULL; }
void crypt_tx(int fd, const void *in, void *out, size_t n) { chacha_xor(&tab[fd]->tx, in, out, n); }
void crypt_rx(int fd, void *buf, size_t n)                  { chacha_xor(&tab[fd]->rx, buf, buf, n); }

void crypt_derive_key(const char *pin, const uint8_t nonce[16], uint8_t key[32])
{
    uint8_t m[14 + 16]; memcpy(m, "unisync-enc-v1", 14); memcpy(m + 14, nonce, 16);
    hmac_sha256((const uint8_t *)pin, strlen(pin), m, sizeof m, key);
}

/* RFC 8439 section 2.4.2 test vector (first 16 ciphertext bytes) */
int crypt_selftest(void)
{
    uint8_t key[32], nonce[12] = {0,0,0,0,0,0,0,0x4a,0,0,0,0};
    for (int i = 0; i < 32; i++) key[i] = (uint8_t)i;
    const char *pt = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    uint8_t out[128]; chacha_t c; chacha_init(&c, key, nonce, 1);
    chacha_xor(&c, (const uint8_t *)pt, out, strlen(pt));
    const uint8_t exp[16] = {0x6e,0x2e,0x35,0x9a,0x25,0x68,0xf9,0x80,0x41,0xba,0x07,0x28,0xdd,0x0d,0x69,0x81};
    return memcmp(out, exp, 16) == 0;
}
