#ifndef _SHAHASH_H
#define _SHAHASH_H

#include <stdint.h>
#include <sys/types.h>

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
#if !defined(__LANGUAGE_ASSEMBLY)
extern "C" {
#endif	
#endif	

#define BLOCK_BYTE_SIZE 64
// left rotate
#define lrol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define UNPACK32(x, str)                      \
{                                             \
    *((str) + 3) = (uint8_t) ((x)      );       \
    *((str) + 2) = (uint8_t) ((x) >>  8);       \
    *((str) + 1) = (uint8_t) ((x) >> 16);       \
    *((str) + 0) = (uint8_t) ((x) >> 24);       \
}

#define PACK32(str, x)                        \
{                                             \
    *(x) =   ((uint32_t) *((str) + 3)      )    \
           | ((uint32_t) *((str) + 2) <<  8)    \
           | ((uint32_t) *((str) + 1) << 16)    \
           | ((uint32_t) *((str) + 0) << 24);   \
}

#define SHA256_SCR(i)                         \
{                                             \
    w[i] =  SHA256_F4(w[i -  2]) + w[i -  7]  \
          + SHA256_F3(w[i - 15]) + w[i - 16]; \
}


#define SHA256_DIGEST_SIZE ( 256 / 8)
#define SHA256_BLOCK_SIZE  ( 512 / 8)

#define SHFR(x, n)    (x >> n)
#define ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define CH(x, y, z)  ((x & y) ^ (~x & z))
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

#define SHA256_F1(x) (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SHA256_F2(x) (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SHA256_F3(x) (ROTR(x,  7) ^ ROTR(x, 18) ^ SHFR(x,  3))
#define SHA256_F4(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHFR(x, 10))


#define SHA1_ERROR_INVALID_POINTER			-2142764288	
#define SHA1_ERROR_INVALID_STATE			-2142764287	
#define SHA1_ERROR_UNINITIALIZED_CONTEXT	-2142764286	

#define SHA1_BLOCK_SIZE		64U
#define SHA1_DIGEST_SIZE	20U

typedef struct Sha1Context {
	uint32_t		h[5];
	uint16_t		usRemains;
	uint16_t		usComputed;
	uint64_t		ullTotalLen;
	uint8_t			buf[SHA1_BLOCK_SIZE];
	uint8_t			result[SHA1_DIGEST_SIZE];
	uint8_t			initialized;
	uint8_t			pad[3];         
} Sha1Context;

typedef struct {
    unsigned int tot_len;
    unsigned int len;
    unsigned char block[2 * SHA256_BLOCK_SIZE];
    uint32_t h[8];
} SHA256_CTX;

extern uint32_t sha256_k[64];

#if !defined(__LANGUAGE_ASSEMBLY)

//SHA1
int32_t Sha1Digest(const void *plain, uint32_t len, uint8_t *digest);
int32_t Sha1BlockInit(Sha1Context *pContext);
int32_t Sha1BlockUpdate(Sha1Context *pContext, const void *plain, uint32_t len);
int32_t Sha1BlockResult(Sha1Context *pContext, uint8_t *digest);

//SHA256
void SHA256_Init(SHA256_CTX* ctx);
void SHA256_Update(SHA256_CTX* ctx, const unsigned char* message, unsigned int len);
void SHA256_Final(SHA256_CTX* ctx, unsigned char *digest);
void SHA256(const unsigned char *message, unsigned int len, unsigned char* digest);

#endif	

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
#if !defined(__LANGUAGE_ASSEMBLY)
}
#endif	
#endif	

#endif	