/* SHA-256 + HMAC-SHA-256, implemented from scratch (FIPS 180-4 / RFC 2104). */
#include "common.h"
#include "sha256.h"
#define SHA_BUF (256 * 1024)

static const uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static void transform(sha256_ctx *c, const uint8_t *d)
{
    uint32_t m[64], a, b, cc, dd, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        m[i] = ((uint32_t)d[i*4] << 24) | ((uint32_t)d[i*4+1] << 16) | ((uint32_t)d[i*4+2] << 8) | d[i*4+3];
    for (; i < 64; i++) {
        uint32_t s0 = ROR(m[i-15],7) ^ ROR(m[i-15],18) ^ (m[i-15] >> 3);
        uint32_t s1 = ROR(m[i-2],17) ^ ROR(m[i-2],19) ^ (m[i-2] >> 10);
        m[i] = m[i-16] + s0 + m[i-7] + s1;
    }
    a = c->st[0]; b = c->st[1]; cc = c->st[2]; dd = c->st[3];
    e = c->st[4]; f = c->st[5]; g = c->st[6]; h = c->st[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        t1 = h + S1 + ch + K[i] + m[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        t2 = S0 + mj;
        h = g; g = f; f = e; e = dd + t1; dd = cc; cc = b; b = a; a = t1 + t2;
    }
    c->st[0] += a; c->st[1] += b; c->st[2] += cc; c->st[3] += dd;
    c->st[4] += e; c->st[5] += f; c->st[6] += g; c->st[7] += h;
}

void sha256_init(sha256_ctx *c)
{
    c->len = 0; c->bits = 0;
    c->st[0]=0x6a09e667; c->st[1]=0xbb67ae85; c->st[2]=0x3c6ef372; c->st[3]=0xa54ff53a;
    c->st[4]=0x510e527f; c->st[5]=0x9b05688c; c->st[6]=0x1f83d9ab; c->st[7]=0x5be0cd19;
}

void sha256_update(sha256_ctx *c, const uint8_t *d, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        c->buf[c->len++] = d[i];
        if (c->len == 64) { transform(c, c->buf); c->bits += 512; c->len = 0; }
    }
}

void sha256_final(sha256_ctx *c, uint8_t out[32])
{
    size_t i = c->len;
    c->buf[i++] = 0x80;
    if (i > 56) { while (i < 64) c->buf[i++] = 0; transform(c, c->buf); i = 0; }
    while (i < 56) c->buf[i++] = 0;
    c->bits += (uint64_t)c->len * 8;
    for (int j = 0; j < 8; j++) c->buf[63 - j] = (uint8_t)(c->bits >> (8 * j));
    transform(c, c->buf);
    for (int j = 0; j < 8; j++) {
        out[j*4]   = (uint8_t)(c->st[j] >> 24);
        out[j*4+1] = (uint8_t)(c->st[j] >> 16);
        out[j*4+2] = (uint8_t)(c->st[j] >> 8);
        out[j*4+3] = (uint8_t)(c->st[j]);
    }
}

void hmac_sha256(const uint8_t *key, size_t klen, const uint8_t *msg, size_t mlen, uint8_t out[32])
{
    uint8_t k[64] = {0}, ip[64], op[64], inner[32];
    sha256_ctx c;
    if (klen > 64) { sha256_init(&c); sha256_update(&c, key, klen); sha256_final(&c, k); }
    else if (klen) memcpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    sha256_init(&c); sha256_update(&c, ip, 64); sha256_update(&c, msg, mlen); sha256_final(&c, inner);
    sha256_init(&c); sha256_update(&c, op, 64); sha256_update(&c, inner, 32); sha256_final(&c, out);
}

/* Hash a whole file using low-level open()/read(). */
int sha256_file(const char *path, uint8_t out[32], uint64_t *size_out)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    uint8_t *buf = malloc(SHA_BUF);
    if (!buf) { close(fd); return -1; }
    sha256_ctx c; sha256_init(&c);
    uint64_t total = 0;
    for (;;) {
        ssize_t r = read(fd, buf, SHA_BUF);
        if (r == 0) break;
        if (r < 0) { if (errno == EINTR) continue; free(buf); close(fd); return -1; }
        sha256_update(&c, buf, (size_t)r);
        total += (uint64_t)r;
    }
    sha256_final(&c, out);
    if (size_out) *size_out = total;
    free(buf); close(fd);
    return 0;
}

void hex_encode(const uint8_t *in, size_t n, char *out)
{
    static const char *hx = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { out[i*2] = hx[in[i] >> 4]; out[i*2+1] = hx[in[i] & 15]; }
    out[n*2] = 0;
}
