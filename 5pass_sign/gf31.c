#include <assert.h>
#include <immintrin.h>
#include "params.h"
#include "KeccakHash.h"
#include "gf31.h"

#ifdef REFERENCE

void vgf31_unique(gf31 *out, gf31 *in)
{
    int i;

    for (i = 0; i < N; i++) {
        out[i] = (1 - (in[i] == 31)) * in[i];
    }
}

void vgf31_shorten_unique(gf31 *out, gf31 *in)
{
    int i;

    for (i = 0; i < N; i++) {
        out[i] = in[i] % 31;
    }
}

#else

/* This function acts on vectors with 64 gf31 elements. */
void vgf31_unique(gf31 *out, gf31 *in)
{
    __m256i x;
    __m256i _w31 = _mm256_set1_epi16(31);
    int i;

    for (i = 0; i < 4; ++i) {
        x = _mm256_loadu_si256((__m256i const *) (in + 16*i));
        x = _mm256_xor_si256(x, _mm256_and_si256(_w31, _mm256_cmpeq_epi16(x, _w31)));
        _mm256_storeu_si256((__m256i*)(out + i*16), x);
    }
}

/* This function acts on vectors with 64 gf31 elements.
It performs one reduction step and guarantees output in [0, 30],
but requires input to be in [0, 32768). */
void vgf31_shorten_unique(gf31 *out, gf31 *in)
{
    __m256i x;
    __m256i _w2114 = _mm256_set1_epi32(2114*65536 + 2114);
    __m256i _w31 = _mm256_set1_epi16(31);
    int i;

    for (i = 0; i < 4; ++i) {
        x = _mm256_loadu_si256((__m256i const *) (in + 16*i));
        x = _mm256_sub_epi16(x, _mm256_mullo_epi16(_w31, _mm256_mulhi_epi16(x, _w2114)));
        x = _mm256_xor_si256(x, _mm256_and_si256(_w31, _mm256_cmpeq_epi16(x, _w31)));
        _mm256_storeu_si256((__m256i*)(out + i*16), x);
    }
}

#endif

void gf31_nrand(gf31 *out, const int len, const unsigned char *seed, const int seedlen)
{
    int i = 0, j;
    const int buflen = 128;
    unsigned char buf[buflen];
    Keccak_HashInstance keccak;

    Keccak_HashInitialize_SHAKE128(&keccak);
    Keccak_HashUpdate(&keccak, seed, seedlen);
    Keccak_HashFinal(&keccak, buf);

    // TODO should not only use the 5 low bits of a byte!
    while (i < len) {
        Keccak_HashSqueeze(&keccak, buf, buflen * 8);
        for (j = 0; j < buflen && i < len; j++) {
            if ((buf[j] & 31) != 31) {
                out[i] = (buf[j] & 31);
                i++;
            }
        }
    }
}

void gf31_nrand_schar(signed char *out, const int len, const unsigned char *seed, const int seedlen)
{
    int i = 0, j;
    const int buflen = 128;
    unsigned char buf[buflen];
    Keccak_HashInstance keccak;

    Keccak_HashInitialize_SHAKE128(&keccak);
    Keccak_HashUpdate(&keccak, seed, seedlen);
    Keccak_HashFinal(&keccak, buf);

    // TODO should not only use the 5 low bits of a byte!
    while (i < len) {
        Keccak_HashSqueeze(&keccak, buf, buflen * 8);
        for (j = 0; j < buflen && i < len; j++) {
            if ((buf[j] & 31) != 31) {
                out[i] = (buf[j] & 31) - 15;
                i++;
            }
        }
    }
}

/* Unpacks an array of packed GF31 elements to one element per gf31.
This function assumes that there is sufficient empty space available at the end
of the array to unpack. Can perform in-place. */
void gf31_nunpack(gf31 *out, const unsigned char *in, const int n)
{
    int i;
    int j = ((n * 5) >> 3) - 1;
    int d = 0;

    for (i = n-1; i >= 0; i--) {
        out[i] = (in[j] >> d) & 31;
        d += 5;
        if (d > 8) {
            d -= 8;
            j--;
            out[i] ^= (in[j] << (5 - d)) & 31;
        }
    }
}

/* Packs an array of GF31 elements from gf31's to concatenated 5-bit values.
This function assumes that there is sufficient space available to unpack.
Can perform in-place. */
void gf31_npack(unsigned char *out, const gf31 *in, const int n)
{
    int i = 0;
    int j;
    int d = 3;

    for (j = 0; j < n; j++) {
        assert(in[j] < 31);
    }

    for (j = 0; j < n; j++) {
        if (d < 0) {
            d += 8;
            out[i] = (out[i] & (255 << (d - 3))) |
                      ((in[j] >> (8 - d)) & ~(255 << (d - 3)));
            i++;
        }
        out[i] = (out[i] & ~(31 << d)) | ((in[j] << d) & (31 << d));
        d -= 5;
    }
}
