// Copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2018 Webchain project
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cryptonight.h"

#include "oaes_lib.h"
#include "c_keccak.h"
#include "c_groestl.h"
#include "c_blake256.h"
#include "c_jh.h"
#include "c_skein.h"
#include "int-util.h"
#include <x86intrin.h>

#ifdef __amd64
static inline void ExpandAESKey256_sub1(__m128i *tmp1, __m128i *tmp2) {
    __m128i tmp4;
    *tmp2 = _mm_shuffle_epi32(*tmp2, 0xFF);
    tmp4 = _mm_slli_si128(*tmp1, 0x04);
    *tmp1 = _mm_xor_si128(*tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp1 = _mm_xor_si128(*tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp1 = _mm_xor_si128(*tmp1, tmp4);
    *tmp1 = _mm_xor_si128(*tmp1, *tmp2);
}

static inline void ExpandAESKey256_sub2(__m128i *tmp1, __m128i *tmp3) {
    __m128i tmp2, tmp4;

    tmp4 = _mm_aeskeygenassist_si128(*tmp1, 0x00);
    tmp2 = _mm_shuffle_epi32(tmp4, 0xAA);
    tmp4 = _mm_slli_si128(*tmp3, 0x04);
    *tmp3 = _mm_xor_si128(*tmp3, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp3 = _mm_xor_si128(*tmp3, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp3 = _mm_xor_si128(*tmp3, tmp4);
    *tmp3 = _mm_xor_si128(*tmp3, tmp2);
}

// Special thanks to Intel for helping me
// with ExpandAESKey256() and its subroutines
static inline void ExpandAESKey256(char *keybuf) {
    __m128i tmp1, tmp2, tmp3, *keys;

    keys = (__m128i *) keybuf;

    tmp1 = _mm_load_si128((__m128i *) keybuf);
    tmp3 = _mm_load_si128((__m128i * )(keybuf + 0x10));

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x01);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[2] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[3] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x02);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[4] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[5] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x04);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[6] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[7] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x08);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[8] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[9] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x10);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[10] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[11] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x20);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[12] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[13] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x40);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[14] = tmp1;
}

struct cryptonight_ctx {
    uint8_t long_state[MEMORY] __attribute((aligned(16)));
    union cn_slow_hash_state state;
    uint8_t text[INIT_SIZE_BYTE] __attribute((aligned(16)));
    uint64_t a[AES_BLOCK_SIZE >> 3] __attribute__((aligned(16)));
    uint64_t b[AES_BLOCK_SIZE >> 3] __attribute__((aligned(16)));
    uint8_t c[AES_BLOCK_SIZE] __attribute__((aligned(16)));
    oaes_ctx *aes_ctx;
};
#endif

void cryptonight_hash_aesni(void *ctx2, const char *input, char *output, uint32_t len) {
#ifdef __amd64
    struct cryptonight_ctx *ctx = ctx2;
    uint8_t ExpandedKey[256];

    hash_process(&ctx->state.hs, (const uint8_t*) input, len);
    memcpy(ctx->text, ctx->state.init, INIT_SIZE_BYTE);
    memcpy(ExpandedKey, ctx->state.hs.b, AES_KEY_SIZE);
    ExpandAESKey256(ExpandedKey);

    __m128i *longoutput, *expkey, *xmminput;
    longoutput = (__m128i *) ctx->long_state;
    expkey = (__m128i *) ExpandedKey;
    xmminput = (__m128i *) ctx->text;
    int i, j;

    for (i = 0; __builtin_expect(i < 0x4000, 1); ++i) {
        for (j = 0; j < 10; j++) {
            xmminput[0] = _mm_aesenc_si128(xmminput[0], expkey[j]);
            xmminput[1] = _mm_aesenc_si128(xmminput[1], expkey[j]);
            xmminput[2] = _mm_aesenc_si128(xmminput[2], expkey[j]);
            xmminput[3] = _mm_aesenc_si128(xmminput[3], expkey[j]);
            xmminput[4] = _mm_aesenc_si128(xmminput[4], expkey[j]);
            xmminput[5] = _mm_aesenc_si128(xmminput[5], expkey[j]);
            xmminput[6] = _mm_aesenc_si128(xmminput[6], expkey[j]);
            xmminput[7] = _mm_aesenc_si128(xmminput[7], expkey[j]);
        }
        _mm_store_si128(&(longoutput[(i << 3)]), xmminput[0]);
        _mm_store_si128(&(longoutput[(i << 3) + 1]), xmminput[1]);
        _mm_store_si128(&(longoutput[(i << 3) + 2]), xmminput[2]);
        _mm_store_si128(&(longoutput[(i << 3) + 3]), xmminput[3]);
        _mm_store_si128(&(longoutput[(i << 3) + 4]), xmminput[4]);
        _mm_store_si128(&(longoutput[(i << 3) + 5]), xmminput[5]);
        _mm_store_si128(&(longoutput[(i << 3) + 6]), xmminput[6]);
        _mm_store_si128(&(longoutput[(i << 3) + 7]), xmminput[7]);
    }

    for (i = 0; i < 2; i++) {
        ctx->a[i] = ((uint64_t *) ctx->state.k)[i] ^ ((uint64_t *) ctx->state.k)[i + 4];
        ctx->b[i] = ((uint64_t *) ctx->state.k)[i + 2] ^ ((uint64_t *) ctx->state.k)[i + 6];
    }

    __m128i b_x = _mm_load_si128((__m128i *) ctx->b);
    uint64_t a[2] __attribute((aligned(16))), b[2] __attribute((aligned(16)));
    a[0] = ctx->a[0];
    a[1] = ctx->a[1];

    for (i = 0; __builtin_expect(i < 0x80000, 1); i++) {
        __m128i c_x = _mm_load_si128((__m128i * ) & ctx->long_state[a[0] & 0x1FFFF0]);
        __m128i a_x = _mm_load_si128((__m128i *) a);
        uint64_t c[2];
        c_x = _mm_aesenc_si128(c_x, a_x);

        _mm_store_si128((__m128i *) c, c_x);
        __builtin_prefetch(&ctx->long_state[c[0] & 0x1FFFF0], 0, 1);

        b_x = _mm_xor_si128(b_x, c_x);
        _mm_store_si128((__m128i * ) & ctx->long_state[a[0] & 0x1FFFF0], b_x);

        VARIANT_WEB_1_1(&ctx->long_state[a[0] & 0x1FFFF0]);

        uint64_t *nextblock = (uint64_t * ) & ctx->long_state[c[0] & 0x1FFFF0];
        uint64_t b[2];
        b[0] = nextblock[0];
        b[1] = nextblock[1];

        {
            uint64_t hi, lo;
            // hi,lo = 64bit x 64bit multiply of c[0] and b[0]

            __asm__("mulq %3\n\t"
            : "=d" (hi),
            "=a" (lo)
            : "%a" (c[0]),
            "rm" (b[0])
            : "cc" );

            a[0] += hi;
            a[1] += lo;
        }
        uint64_t *dst = (uint64_t*)&ctx->long_state[c[0] & 0x1FFFF0];
        dst[0] = a[0];
        dst[1] = a[1];

        a[0] ^= b[0];
        a[1] ^= b[1];

        VARIANT_WEB_1_2(dst);

        b_x = c_x;
        __builtin_prefetch(&ctx->long_state[a[0] & 0x1FFFF0], 0, 3);
    }
    memcpy(ctx->text, ctx->state.init, INIT_SIZE_BYTE);

    memcpy(ExpandedKey, &ctx->state.hs.b[32], AES_KEY_SIZE);
    ExpandAESKey256(ExpandedKey);

    //for (i = 0; likely(i < MEMORY); i += INIT_SIZE_BYTE)
    //    aesni_parallel_xor(&ctx->text, ExpandedKey, &ctx->long_state[i]);

    for (i = 0; __builtin_expect(i < 0x4000, 1); ++i) {
        xmminput[0] = _mm_xor_si128(longoutput[(i << 3)], xmminput[0]);
        xmminput[1] = _mm_xor_si128(longoutput[(i << 3) + 1], xmminput[1]);
        xmminput[2] = _mm_xor_si128(longoutput[(i << 3) + 2], xmminput[2]);
        xmminput[3] = _mm_xor_si128(longoutput[(i << 3) + 3], xmminput[3]);
        xmminput[4] = _mm_xor_si128(longoutput[(i << 3) + 4], xmminput[4]);
        xmminput[5] = _mm_xor_si128(longoutput[(i << 3) + 5], xmminput[5]);
        xmminput[6] = _mm_xor_si128(longoutput[(i << 3) + 6], xmminput[6]);
        xmminput[7] = _mm_xor_si128(longoutput[(i << 3) + 7], xmminput[7]);

        for (j = 0; j < 10; j++) {
            xmminput[0] = _mm_aesenc_si128(xmminput[0], expkey[j]);
            xmminput[1] = _mm_aesenc_si128(xmminput[1], expkey[j]);
            xmminput[2] = _mm_aesenc_si128(xmminput[2], expkey[j]);
            xmminput[3] = _mm_aesenc_si128(xmminput[3], expkey[j]);
            xmminput[4] = _mm_aesenc_si128(xmminput[4], expkey[j]);
            xmminput[5] = _mm_aesenc_si128(xmminput[5], expkey[j]);
            xmminput[6] = _mm_aesenc_si128(xmminput[6], expkey[j]);
            xmminput[7] = _mm_aesenc_si128(xmminput[7], expkey[j]);
        }
    }

    memcpy(ctx->state.init, ctx->text, INIT_SIZE_BYTE);

    hash_permutation(&ctx->state.hs);
    extra_hashes[ctx->state.hs.b[0] & 3](&ctx->state, 200, output);
#else
    cryptonight_hash(ctx2, input, output, len);
#endif
}
