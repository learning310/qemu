#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "accel/tcg/cpu-ldst.h"
#include <string.h>
#include <stdint.h>

/* SHA-384 uses the same core as SHA-512 with different IV and truncated output */

/* SHA-384/512 round constants K */
static const uint64_t K[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/* Rotate right 64-bit */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

/* SHA-512 logical functions */
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SIGMA1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define sigma0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define sigma1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

/*
 * Process one 128-byte (1024-bit) SHA-384/512 block.
 * state: array of 8 x uint64_t  (H0..H7), updated in-place.
 * block: pointer to 128 bytes of message data (big-endian on wire).
 */
static void sha512_384_compress(uint64_t state[8], const uint8_t block[128])
{
	uint64_t W[80];
	uint64_t a, b, c, d, e, f, g, h;
	int t;

	/* Prepare message schedule W[0..79] */
	for (t = 0; t < 16; t++) {
		W[t] = ldq_be_p(block + t * 8);
	}
	for (t = 16; t < 80; t++) {
		W[t] = sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t - 16];
	}

	/* Initialize working variables */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	f = state[5];
	g = state[6];
	h = state[7];

	/* 80 rounds */
	for (t = 0; t < 80; t++) {
		uint64_t T1 = h + SIGMA1(e) + CH(e, f, g) + K[t] + W[t];
		uint64_t T2 = SIGMA0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;
	}

	/* Update state */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

/*
 * helper_xsha384 - QEMU helper for the REP XSHA384 instruction.
 *
 * @env : CPU state
 * @rsi : guest virtual address of input data stream (ES segment assumed)
 * @rdi : guest virtual address of the 64-byte, 16-byte-aligned hash state
 *         (8 x uint64_t big-endian, writable)
 * @rcx : number of 128-byte blocks to process
 *
 * Behaviour:
 *   - If rcx == 0 the function is a NOP (matches the "0 means NOP" spec).
 *   - Reads each 128-byte block from [rsi], processes it into the state at
 *     [rdi], advances rsi by 128 per block.
 *   - On return: rcx = 0, rsi advanced, rdi unchanged (state updated in mem).
 */
void helper_xsha384(CPUX86State *env, target_ulong rsi, target_ulong rdi, target_ulong rcx)
{
    uint64_t state[8];
    uint8_t block[128];
    target_ulong i, j;

    /* NOP case */
    if (rcx == 0) {
        return;
    }

    /* ------------------------------------------------------------------ *
     * Load the current hash state (8 x uint64_t, little-endian in memory)   *
     * RDI points to 64 bytes, 16-byte aligned.                           *
     * ------------------------------------------------------------------ */
    for (i = 0; i < 8; i++) {
        state[i] = cpu_ldq_le_data(env, rdi + i * 8);
    }

    /* ------------------------------------------------------------------ *
     * Process each 128-byte block                                         *
     * ------------------------------------------------------------------ */
    for (i = 0; i < rcx; i++) {
        /* Read one 128-byte block from guest memory */
        for (j = 0; j < 128; j++) {
            block[j] = cpu_ldub_data(env, rsi + j);
        }
        sha512_384_compress(state, block);
        rsi += 128;
    }

    /* ------------------------------------------------------------------ *
     * Write the updated hash state back to [RDI] (little-endian)            *
     * ------------------------------------------------------------------ */
    for (i = 0; i < 8; i++) {
        cpu_stq_le_data(env, rdi + i * 8, state[i]);
    }

    env->regs[R_ECX] = 0;
    env->regs[R_ESI] = rsi;
}

/*
 * helper_xsha512 - QEMU helper for the REP XSHA512 instruction.
 *
 * @env : CPU state
 * @rsi : guest virtual address of input data stream (ES segment assumed)
 * @rdi : guest virtual address of the 64-byte, 16-byte-aligned hash state
 *         (8 x uint64_t little-endian, writable)
 * @rcx : number of 128-byte blocks to process
 *
 * Behaviour:
 *   - If rcx == 0 the function is a NOP.
 *   - Reads each 128-byte block from [rsi], processes it into the state at
 *     [rdi], advances rsi by 128 per block.
 *   - On return: rcx = 0, rsi advanced, rdi unchanged (state updated in mem).
 */
void helper_xsha512(CPUX86State *env, target_ulong rsi, target_ulong rdi, target_ulong rcx)
{
	 uint64_t state[8];
    uint8_t block[128];
    target_ulong i, j;

    /* NOP case */
    if (rcx == 0) {
        return;
    }

    /* ------------------------------------------------------------------ *
     * Load the current hash state (8 x uint64_t, little-endian in memory)   *
     * RDI points to 64 bytes, 16-byte aligned.                           *
     * ------------------------------------------------------------------ */
    for (i = 0; i < 8; i++) {
        state[i] = cpu_ldq_le_data(env, rdi + i * 8);
    }

    /* ------------------------------------------------------------------ *
     * Process each 128-byte block                                         *
     * ------------------------------------------------------------------ */
    for (i = 0; i < rcx; i++) {
        /* Read one 128-byte block from guest memory */
        for (j = 0; j < 128; j++) {
            block[j] = cpu_ldub_data(env, rsi + j);
        }
        sha512_384_compress(state, block);
        rsi += 128;
    }

    /* ------------------------------------------------------------------ *
     * Write the updated hash state back to [RDI] (little-endian)            *
     * ------------------------------------------------------------------ */
    for (i = 0; i < 8; i++) {
        cpu_stq_le_data(env, rdi + i * 8, state[i]);
    }

    env->regs[R_ECX] = 0;
    env->regs[R_ESI] = rsi;
}