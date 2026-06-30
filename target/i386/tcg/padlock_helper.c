/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "accel/tcg/cpu-ldst.h"

/* SHA-384/512 round constants K */
static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/* Rotate right 64-bit */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

/* SHA-512 logical functions */
#define CH(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define MAJ(x, y, z) (((x) & (y)) | ((z) & ((x) | (y))))
#define SIGMA0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SIGMA1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define sigma0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define sigma1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

static inline void Round(uint64_t a, uint64_t b, uint64_t c, uint64_t *d,
             uint64_t e, uint64_t f, uint64_t g, uint64_t *h,
             uint64_t k, uint64_t w)
{
    uint64_t t1 = *h + SIGMA1(e) + CH(e, f, g) + k + w;
    uint64_t t2 = SIGMA0(a) + MAJ(a, b, c);
    *d += t1;
    *h = t1 + t2;
}

/*
 * Process one 128-byte (1024-bit) SHA-384/512 block.
 * state : array of 8 x uint64_t (H0..H7), updated in-place.
 * block : pointer to 128 bytes of message data (big-endian on wire).
 */
static void sha512_384_compress(uint64_t state[8], const uint8_t block[128])
{
    uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint64_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint64_t w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;

    /* ---- Rounds 0-15: load message words ---- */
    Round(a, b, c, &d, e, f, g, &h, K[0], w0 = ldq_be_p(block + 0));
    Round(h, a, b, &c, d, e, f, &g, K[1], w1 = ldq_be_p(block + 8));
    Round(g, h, a, &b, c, d, e, &f, K[2], w2 = ldq_be_p(block + 16));
    Round(f, g, h, &a, b, c, d, &e, K[3], w3 = ldq_be_p(block + 24));
    Round(e, f, g, &h, a, b, c, &d, K[4], w4 = ldq_be_p(block + 32));
    Round(d, e, f, &g, h, a, b, &c, K[5], w5 = ldq_be_p(block + 40));
    Round(c, d, e, &f, g, h, a, &b, K[6], w6 = ldq_be_p(block + 48));
    Round(b, c, d, &e, f, g, h, &a, K[7], w7 = ldq_be_p(block + 56));
    Round(a, b, c, &d, e, f, g, &h, K[8], w8 = ldq_be_p(block + 64));
    Round(h, a, b, &c, d, e, f, &g, K[9], w9 = ldq_be_p(block + 72));
    Round(g, h, a, &b, c, d, e, &f, K[10], w10 = ldq_be_p(block + 80));
    Round(f, g, h, &a, b, c, d, &e, K[11], w11 = ldq_be_p(block + 88));
    Round(e, f, g, &h, a, b, c, &d, K[12], w12 = ldq_be_p(block + 96));
    Round(d, e, f, &g, h, a, b, &c, K[13], w13 = ldq_be_p(block + 104));
    Round(c, d, e, &f, g, h, a, &b, K[14], w14 = ldq_be_p(block + 112));
    Round(b, c, d, &e, f, g, h, &a, K[15], w15 = ldq_be_p(block + 120));

    /* ---- Rounds 16-31 ---- */
    Round(a, b, c, &d, e, f, g, &h, K[16], w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, K[17], w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, K[18], w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, K[19], w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, K[20], w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, K[21], w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, K[22], w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, K[23], w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, K[24], w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, K[25], w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, K[26], w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, K[27], w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, K[28], w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, K[29], w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, K[30], w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, K[31], w15 += sigma1(w13) + w8 + sigma0(w0));

    /* ---- Rounds 32-47 ---- */
    Round(a, b, c, &d, e, f, g, &h, K[32], w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, K[33], w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, K[34], w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, K[35], w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, K[36], w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, K[37], w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, K[38], w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, K[39], w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, K[40], w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, K[41], w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, K[42], w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, K[43], w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, K[44], w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, K[45], w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, K[46], w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, K[47], w15 += sigma1(w13) + w8 + sigma0(w0));

    /* ---- Rounds 48-63 ---- */
    Round(a, b, c, &d, e, f, g, &h, K[48], w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, K[49], w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, K[50], w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, K[51], w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, K[52], w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, K[53], w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, K[54], w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, K[55], w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, K[56], w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, K[57], w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, K[58], w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, K[59], w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, K[60], w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, K[61], w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, K[62], w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, K[63], w15 += sigma1(w13) + w8 + sigma0(w0));

    /* ---- Rounds 64-79 ---- */
    Round(a, b, c, &d, e, f, g, &h, K[64], w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, K[65], w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, K[66], w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, K[67], w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, K[68], w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, K[69], w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, K[70], w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, K[71], w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, K[72], w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, K[73], w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, K[74], w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, K[75], w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, K[76], w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, K[77], w13 += sigma1(w11) + w6 + sigma0(w14));
    /*
     * Note:The assignment of w14 and w15 can be safely removed
     * as they are not utilized in subsequent operations.
     */
    Round(c, d, e, &f, g, h, a, &b, K[78], w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, K[79], w15 += sigma1(w13) + w8 + sigma0(w0));

    /* ---- Update state ---- */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void sha512_384_block(CPUX86State *env, target_ulong rsi, target_ulong rdi)
{
    uint64_t state[8];
    uint64_t block[16];
    int i;

    for (i = 0; i < 8; i++) {
        state[i] = cpu_ldq_le_data(env, rdi + i * 8);
    }

    for (i = 0; i < 16; i++) {
        block[i] = cpu_ldq_le_data(env, rsi + (i << 3));
    }

    sha512_384_compress(state, (const uint8_t *)block);

    /*
     * Note: The XSHA384 instruction writes back the full 64-byte state,
     * caller is responsible for truncating to 48 bytes
     */
    for (i = 0; i < 8; i++) {
        cpu_stq_le_data(env, rdi + i * 8, state[i]);
    }
}

void helper_xsha384(CPUX86State *env, target_ulong rsi, target_ulong rdi)
{
    sha512_384_block(env, rsi, rdi);
}

void helper_xsha512(CPUX86State *env, target_ulong rsi, target_ulong rdi)
{
    sha512_384_block(env, rsi, rdi);
}
