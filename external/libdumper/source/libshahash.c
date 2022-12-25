/*
 * FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 02/02/2007
 * Issue date:  04/30/2005
 *
 * Copyright (C) 2013, Con Kolivas <kernel@kolivas.org>
 * Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdint.h>
#include <string.h>
#include <stdint.h>
#include "libshahash.h"


uint32_t sha256_h0[8] =
            {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
             0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

uint32_t sha256_k[64] =
            {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
             0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
             0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
             0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
             0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
             0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
             0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
             0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
             0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
             0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
             0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
             0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
             0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
             0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
             0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
             0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/* SHA-256 functions */
void sha256_transf(SHA256_CTX *ctx, const unsigned char *message,
                   unsigned int block_nb)
{
    uint32_t w[64];
    uint32_t wv[8];
    uint32_t t1, t2;
    const unsigned char *sub_block;
    int i;

    int j;

    for (i = 0; i < (int) block_nb; i++) {
        sub_block = message + (i << 6);

        for (j = 0; j < 16; j++) {
            PACK32(&sub_block[j << 2], &w[j]);
        }

        for (j = 16; j < 64; j++) {
            SHA256_SCR(j);
        }

        for (j = 0; j < 8; j++) {
            wv[j] = ctx->h[j];
        }

        for (j = 0; j < 64; j++) {
            t1 = wv[7] + SHA256_F2(wv[4]) + CH(wv[4], wv[5], wv[6])
                + sha256_k[j] + w[j];
            t2 = SHA256_F1(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
            wv[7] = wv[6];
            wv[6] = wv[5];
            wv[5] = wv[4];
            wv[4] = wv[3] + t1;
            wv[3] = wv[2];
            wv[2] = wv[1];
            wv[1] = wv[0];
            wv[0] = t1 + t2;
        }

        for (j = 0; j < 8; j++) {
            ctx->h[j] += wv[j];
        }
    }
}

void SHA256(const unsigned char *message, unsigned int len, unsigned char *digest)
{
    SHA256_CTX ctx;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, message, len);
    SHA256_Final(&ctx, digest);
}

void SHA256_Init(SHA256_CTX *ctx)
{
    int i;
    for (i = 0; i < 8; i++) {
        ctx->h[i] = sha256_h0[i];
    }

    ctx->len = 0;
    ctx->tot_len = 0;
}

void SHA256_Update(SHA256_CTX *ctx, const unsigned char *message,
                   unsigned int len)
{
    unsigned int block_nb;
    unsigned int new_len, rem_len, tmp_len;
    const unsigned char *shifted_message;

    tmp_len = SHA256_BLOCK_SIZE - ctx->len;
    rem_len = len < tmp_len ? len : tmp_len;

    memcpy(&ctx->block[ctx->len], message, rem_len);

    if (ctx->len + len < SHA256_BLOCK_SIZE) {
        ctx->len += len;
        return;
    }

    new_len = len - rem_len;
    block_nb = new_len / SHA256_BLOCK_SIZE;

    shifted_message = message + rem_len;

    sha256_transf(ctx, ctx->block, 1);
    sha256_transf(ctx, shifted_message, block_nb);

    rem_len = new_len % SHA256_BLOCK_SIZE;

    memcpy(ctx->block, &shifted_message[block_nb << 6],
           rem_len);

    ctx->len = rem_len;
    ctx->tot_len += (block_nb + 1) << 6;
}

void SHA256_Final(SHA256_CTX *ctx, unsigned char *digest)
{
    unsigned int block_nb;
    unsigned int pm_len;
    unsigned int len_b;

    int i;

    block_nb = (1 + ((SHA256_BLOCK_SIZE - 9)
                     < (ctx->len % SHA256_BLOCK_SIZE)));

    len_b = (ctx->tot_len + ctx->len) << 3;
    pm_len = block_nb << 6;

    memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
    ctx->block[ctx->len] = 0x80;
    UNPACK32(len_b, ctx->block + pm_len - 4);

    sha256_transf(ctx, ctx->block, block_nb);

    for (i = 0 ; i < 8; i++) {
        UNPACK32(ctx->h[i], &digest[i << 2]);
    }
}
// END OF SHA256 https://github.com/sasanj/libsha256/blob/master/src/sha2.c
// Start SHA1 https://github.com/mmxsrup/libsha1/blob/master/sha1.c
void Sha1Core(uint32_t *blk, uint32_t *h); 
static __inline__ void Sha1SwapEndian(uint32_t *dst, const uint32_t *src, uint32_t len);
static __inline__ void Sha1Memcpy(void *pDst, const void *pSrc, uint32_t uiSize);

static const uint32_t K[4] = {
    0x5A827999U,    
    0x6ED9EBA1U,   
    0x8F1BBCDCU,    
    0xCA62C1D6U     
};

#define lrotate(x, bits)   ((x << bits) | (x >> (32-bits)))

void Sha1Core(uint32_t *blk, uint32_t *h)
{
    uint32_t s, temp, a, b, c, d, e;
    a = h[0];
    b = h[1];
    c = h[2];
    d = h[3];
    e = h[4];

    for(uint32_t t = 0; t < 80; t++) {
        s = t & 15;
        if(t>=16) {
             blk[s] = lrotate((blk[(s+13) & 15] ^ blk[(s+8) & 15] ^ blk[(s+2) & 15] ^ blk[s]), 1);
        }
        temp = lrotate(a, 5) + e + blk[s];
        if(t<20) {
            temp += ((b & c) | ((~b) & d)) + K[0];
        } else if(t<40) {
            temp += (b ^ c ^ d) + K[1];
        } else if(t<60) {
            temp += ((b & c) | (b & d) | (c & d)) + K[2];
        } else {
            temp += (b ^ c ^ d) + K[3];
        }
        e = d;
        d = c;
        c = lrotate(b, 30);
        b = a;
        a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;

    return;
}


static __inline__ void Sha1SwapEndian(uint32_t *dst, const uint32_t *src, uint32_t len)
{
    uint32_t i;
    for (i=0; i<len; i++) {
#if defined(__GNUC__)
    union {
        uint32_t data;
        uint8_t uc[4];
    } u;
    uint8_t buf;
    u.data = src[i];
    buf = u.uc[0];
    u.uc[0] = u.uc[3];
    u.uc[3] = buf;
    buf = u.uc[1];
    u.uc[1] = u.uc[2];
    u.uc[2] = buf;
    dst[i] = u.data;
#endif  /* defined(__GNUC__) */
#if defined(__SNC__)
        dst[i] = __rev(src[i]);
#endif  /* defined(__SNC__) */
    }
    return;
}


static __inline__ void Sha1Memcpy(void *pDst, const void *pSrc, uint32_t uiSize)
{
    uint8_t *pd = (uint8_t *)pDst;
    const uint8_t *ps = (const uint8_t *)pSrc;
    uint32_t i;
    for (i=0; i<uiSize; i++) {
        *pd++ = *ps++;
    }
    return;
}


int32_t Sha1Digest(const void *plain, uint32_t len, uint8_t *digest)
{
    Sha1Context ctx;

    if (plain == NULL || digest == NULL) {
        return (SHA1_ERROR_INVALID_POINTER);
    }

    Sha1BlockInit(&ctx);
    Sha1BlockUpdate(&ctx, plain, len);
    Sha1BlockResult(&ctx, digest);
    return (0);
}



int32_t Sha1BlockInit(Sha1Context *pContext)
{
    if (!pContext) {
        return (SHA1_ERROR_INVALID_POINTER);
    }
    pContext->h[0] = 0x67452301U;
    pContext->h[1] = 0xEFCDAB89U;
    pContext->h[2] = 0x98BADCFEU;
    pContext->h[3] = 0x10325476U;
    pContext->h[4] = 0xC3D2E1F0U;
    pContext->usRemains  = 0;
    pContext->usComputed = 0;
    pContext->ullTotalLen = 0;
    pContext->initialized = 1;
    return (0);
}



int32_t Sha1BlockUpdate(Sha1Context *pContext, const void *plain, uint32_t len)
{
    union {
        uint8_t b[SHA1_BLOCK_SIZE];
        uint32_t ui[SHA1_BLOCK_SIZE / 4];
    } blk;
    uint32_t n;

    if (!pContext || !plain) {
        return (SHA1_ERROR_INVALID_POINTER);
    }
    if (pContext->initialized != 1) {
        return (SHA1_ERROR_UNINITIALIZED_CONTEXT);
    }
    if (pContext->usRemains >= SHA1_BLOCK_SIZE) {
        return (SHA1_ERROR_INVALID_STATE);
    }

    pContext->ullTotalLen += len;
    n = pContext->usRemains + len;

    while (n >= SHA1_BLOCK_SIZE) {
        if (pContext->usRemains > 0) {
            uint32_t cont = SHA1_BLOCK_SIZE - pContext->usRemains;
            Sha1Memcpy(blk.b, pContext->buf, pContext->usRemains);
            Sha1Memcpy(&blk.b[pContext->usRemains], plain, cont);
            pContext->usRemains = 0;
            plain = (const uint8_t *)plain + cont;
        } else {
            Sha1Memcpy(blk.b, plain, SHA1_BLOCK_SIZE);
            plain = (const uint8_t *)plain + SHA1_BLOCK_SIZE;
        }
        Sha1SwapEndian(blk.ui, blk.ui, (SHA1_BLOCK_SIZE / 4));
                                                           
        Sha1Core(blk.ui, pContext->h);                  
                                                            
        n -= SHA1_BLOCK_SIZE;
    }
    if (n > 0) {
        Sha1Memcpy(&pContext->buf[pContext->usRemains], plain, (n - pContext->usRemains));
    }
    pContext->usRemains = (uint16_t)n;
    pContext->usComputed = 0;
    return (0);
}




int32_t Sha1BlockResult(Sha1Context *pContext, uint8_t *digest)
{
    if (!pContext || !digest) {
        return (SHA1_ERROR_INVALID_POINTER);
    }
    if (pContext->initialized != 1) {
        return (SHA1_ERROR_UNINITIALIZED_CONTEXT);
    }
    if (pContext->usComputed==0) {
        union {
            uint8_t b[SHA1_BLOCK_SIZE];
            uint32_t ui[SHA1_BLOCK_SIZE / 4];
        } blk;
        uint32_t h[5];

        uint32_t n;
        uint32_t i;
        n = pContext->usRemains;

        for (i=0; i<(SHA1_BLOCK_SIZE / 4); i++) {
            blk.ui[i] = 0;
        }
        Sha1Memcpy(blk.b, pContext->buf, n);
        blk.b[n] = 0x80;

        Sha1Memcpy(h, pContext->h, sizeof(h));
        if (n >= (SHA1_BLOCK_SIZE - 8)) {
            Sha1SwapEndian(blk.ui, blk.ui, (SHA1_BLOCK_SIZE / 4));
                                                            
                                                            
            Sha1Core(blk.ui, h);                        
                                                            
            for (i=0; i<(SHA1_BLOCK_SIZE / 4); i++) {
                blk.ui[i] = 0;
            }
        }
        for (i=0; i<8; i++) {
            blk.b[56 + i] = (uint8_t)((pContext->ullTotalLen<<3) >> ((7 - i)*8));
        }
        Sha1SwapEndian(blk.ui, blk.ui, (SHA1_BLOCK_SIZE / 4));
                                                            
        Sha1Core(blk.ui, h);                            
                                                            
        Sha1SwapEndian(h, h, 5);                                                                            
        Sha1Memcpy(pContext->result, h, SHA1_DIGEST_SIZE);
        pContext->usComputed = 1;
    }
    Sha1Memcpy(digest, pContext->result, SHA1_DIGEST_SIZE);
    return (0);
}

//END OF SHA1 https://github.com/mmxsrup/libsha1/blob/master/sha1.c