/////////////////////////////////
// Header comment place holder //
/////////////////////////////////

#ifndef __HASHFUNC_H
#define __HASHFUNC_H

// *** Header Inclusions *** //
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Used to find the Endianess of the machine we are working on */
#ifdef _MSC_VER
    #include <stdlib.h>
    #define bswap_32(x) _byteswap_ulong(x)
    #define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__) /* Mac OS X / Darwin features */
    #include <libkern/OSByteOrder.h>
    #define bswap_32(x) OSSwapInt32(x)
    #define bswap_64(x) OSSwapInt64(x)
#elif defined(__sun) || defined(sun)
    #include <sys/byteorder.h>
    #define bswap_32(x) BSWAP_32(x)
    #define bswap_64(x) BSWAP_64(x)
#elif defined(__FreeBSD__)
    #include <sys/endian.h>
    #define bswap_32(x) bswap32(x)
    #define bswap_64(x) bswap64(x)
#elif defined(__OpenBSD__)
    #include <sys/types.h>
    #define bswap_32(x) swap32(x)
    #define bswap_64(x) swap64(x)
#elif defined(__NetBSD__)
    #include <machine/bswap.h>
    #include <sys/types.h>
    #if defined(__BSWAP_RENAME) && !defined(__bswap_32)
        #define bswap_32(x) bswap32(x)
        #define bswap_64(x) bswap64(x)
    #endif
#else
    #include <byteswap.h>
#endif

/* Use the appropriate - Endianess Declaration */
#ifdef WORDS_BIGENDIAN
    #define uint16_in_expected_order(x) (bswap_16(x))
    #define uint32_in_expected_order(x) (bswap_32(x))
    #define uint64_in_expected_order(x) (bswap_64(x))
#else
    #define uint16_in_expected_order(x) (x)
    #define uint32_in_expected_order(x) (x)
    #define uint64_in_expected_order(x) (x)
#endif

/* Related to the bit endianess of the system - Needed by Siphash and bitmasks */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define _le64toh(x) ((uint64_t)(x))
#elif defined(_WIN32)
    /* Windows is always little endian, unless you're on xbox360
       http://msdn.microsoft.com/en-us/library/b0084kay(v=vs.80).aspx */
    #define _le64toh(x) ((uint64_t)(x))
#elif defined(__APPLE__)
    #define _le64toh(x) OSSwapLittleToHostInt64(x)
#else
    /* See: http://sourceforge.net/p/predef/wiki/Endianness/ */
    #if defined(__NetBSD__) || defined(__OpenBSD__)
        #include <sys/endian.h>
    #else
        #include <endian.h>
    #endif
    #if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN
        #define _le64toh(x) ((uint64_t)(x))
    #else
        #define _le64toh(x) le64toh(x)
    #endif
#endif

// *** Supporting structures *** //
typedef struct uint128_ht_structure
{
    uint64_t low;
    uint64_t high;
} uint128_ht;

/* Base operations for UINT128_HT structure */
static inline uint64_t Uint128Low64(const uint128_ht x)
{
    return x.low;
}
static inline uint64_t Uint128High64(const uint128_ht x)
{
    return x.high;
}

/* We only permute 32-bit numbers so its fine it't not generic */
#define PERMUTE3_UINT32(a, b, c)                                                                                                                     \
    {                                                                                                                                                \
        uint32_t temp;                                                                                                                               \
        temp = a;                                                                                                                                    \
        a = b;                                                                                                                                       \
        b = temp;                                                                                                                                    \
    }                                                                                                                                                \
    {                                                                                                                                                \
        uint32_t temp;                                                                                                                               \
        temp = a;                                                                                                                                    \
        a = c;                                                                                                                                       \
        c = temp;                                                                                                                                    \
    }

#define HALF_ROUND(a, b, c, d, s, t)                                                                                                                 \
    a += b;                                                                                                                                          \
    c += d;                                                                                                                                          \
    b = rotl64(b, s) ^ a;                                                                                                                            \
    d = rotl64(d, t) ^ c;                                                                                                                            \
    a = rotl64(a, 32);

#define DOUBLE_ROUND(v0, v1, v2, v3)                                                                                                                 \
    HALF_ROUND(v0, v1, v2, v3, 13, 16);                                                                                                              \
    HALF_ROUND(v2, v1, v0, v3, 17, 21);                                                                                                              \
    HALF_ROUND(v0, v1, v2, v3, 13, 16);                                                                                                              \
    HALF_ROUND(v2, v1, v0, v3, 17, 21);

// *** Constants *** //
// Some primes between 2^63 and 2^64 for various uses //
static const uint64_t k0 = 0xc3a5c85c97cb3127ULL;
static const uint64_t k1 = 0xb492b66fbe98f273ULL;
static const uint64_t k2 = 0x9ae16a3b2f90404fULL;

// Magic numbers for 32-bit hashing. Copied from Murmur3 //
static const uint32_t c1 = 0xcc9e2d51;
static const uint32_t c2 = 0x1b873593;

/*************** ROTATIONS/MIXES ***************/

/* **** rotl32 ****
 * @ Input arguments:
 *        - uint32_t x   : Number
 *        - int8_t r     : The amount of shift
 * @ Return value:
 *        - uint32_t ret : Result
 * @ Description:
 *
 * Supporting function, perform rotate left operation for a 32-bit number.
 *
 */
static inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return r == 0 ? x : ((x << r) | (x >> (32 - r)));
}

/* **** rotl64 ****
 * @ Input arguments:
 *        - uint64_t x   : Number
 *        - int8_t r     : The amount of shift
 * @ Return value:
 *        - uint64_t ret : Result
 * @ Description:
 *
 * Supporting function, perform rotate left operation for a 64-bit number.
 *
 */
static inline uint64_t rotl64(uint64_t x, int8_t r)
{
    return r == 0 ? x : ((x << r) | (x >> (64 - r)));
}

/* **** fmix32 ****
 * @ Input arguments:
 *        - uint32_t h   : Number to be mixed
 * @ Return value:
 *        - uint32_t ret : Result
 * @ Description:
 *
 * Supporting function, mixes up the bits of a 32-bit number.
 *
 */
static inline uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

/* **** fmix64 ****
 * @ Input arguments:
 *        - uint64_t h   : Number to be mixed
 * @ Return value:
 *        - uint64_t ret : Result
 * @ Description:
 *
 * Supporting function, mixes up the bits of a 64-bit number.
 */
static inline uint64_t fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccd;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53;
    k ^= k >> 33;

    return k;
}

/* **** ShiftMix ****
 * @ Input arguments:
 *        - uint64_t h   : Number to be mixed
 * @ Return value:
 *        - uint64_t ret : Result
 * @ Description:
 *
 * Shift and mix a 64-bit value.
 *
 */
static inline uint64_t ShiftMix(uint64_t val)
{
    return val ^ (val >> 47);
}

/*************** BLOCK/ALIGNS/FETCH ***************/

/* **** getblock32 ****
 * @ Input arguments:
 *        - const uint32_t *p : Number to be alligned
 *        - int r             : Dereference
 * @ Return value:
 *        - uint32_t ret      : Result
 * @ Description:
 *
 * Supporting function, alligns a number to its proper block.
 *
 */
static inline uint32_t getblock32(const uint32_t *p, int i)
{
    return p[i];
}

/* **** getblock64 ****
 * @ Input arguments:
 *        - const uint64_t *p : Number to be alligned
 *        - int r             : Dereference
 * @ Return value:
 *        - uint64_t ret      : Result
 * @ Description:
 *
 * Supporting function, alligns a number to its proper block.
 *
 */
static inline uint64_t getblock64(const uint64_t *p, int i)
{
    return p[i];
}

/* **** UNALIGNED_LOAD32 ****
 * @ Input arguments:
 *        - const void *p : The constant
 * @ Return value:
 *        - uint32_t ret  : Loaded 32bit-Word
 * @ Description:
 *
 * Loads a 32bit-Word.
 *
 */
static inline uint32_t UNALIGNED_LOAD32(const char *p)
{
    uint32_t result;
    memcpy(&result, p, sizeof(result));
    return result;
}

/* **** UNALIGNED_LOAD64 ****
 * @ Input arguments:
 *        - const void *p : The constant
 * @ Return value:
 *        - uint64_t ret  : Loaded 64bit-Word
 * @ Description:
 *
 * Loads a 64bit-Word.
 *
 */
static inline uint64_t UNALIGNED_LOAD64(const char *p)
{
    uint64_t result;
    memcpy(&result, p, sizeof(result));
    return result;
}

/* **** Fetch32 ****
 * @ Input arguments:
 *        - const void *p : The constant
 * @ Return value:
 *        - uint32_t ret  : Fetched 32bit-Word
 * @ Description:
 *
 * Fetches a 32bit-Word.
 *
 */
static inline uint32_t Fetch32(const char *p)
{
    return uint32_in_expected_order(UNALIGNED_LOAD32(p));
}

/* **** Fetch64 ****
 * @ Input arguments:
 *        - const void *p : The constant
 * @ Return value:
 *        - uint64_t ret  : Fetched 64bit-Word
 * @ Description:
 *
 * Fetches a 64bit-Word.
 *
 */
static inline uint64_t Fetch64(const char *p)
{
    return uint64_in_expected_order(UNALIGNED_LOAD64(p));
}

/*************** CONVERTERS/MINIMAL HASHERS ***************/

/* Declare also in order to be seen by others */
static inline uint64_t Hash128to64(const uint128_ht x);
static inline uint128_ht WeakHashLen32WithSeeds(uint64_t w, uint64_t x, uint64_t y, uint64_t z, uint64_t a, uint64_t b);
static inline uint128_ht WeakHashLen32WithSeedsUp(const char *s, uint64_t a, uint64_t b);
static inline uint64_t HashLen16(uint64_t u, uint64_t v, uint64_t mul);
static inline uint64_t HashLen16_128bit(uint64_t u, uint64_t v);
static inline uint32_t Hash32Len0to4(const char *s, size_t len);
static inline uint32_t Hash32Len5to12(const char *s, size_t len);
static inline uint32_t Hash32Len13to24(const char *s, size_t len);
static inline uint64_t HashLen33to64(const char *s, size_t len);
static inline uint64_t HashLen17to32(const char *s, size_t len);
static inline uint64_t HashLen0to16(const char *s, size_t len);
static inline uint32_t mur(uint32_t a, uint32_t h);
static inline uint128_ht CityMurmur(const char *s, size_t len, uint128_ht seed);

/* **** Hash128to64 ****
 * @ Input arguments:
 *        - const uint128_ht x : The original hash
 * @ Return value:
 *        - uint64_t ret       : The new 32-bit value
 * @ Description:
 *
 * Hashes an 128bit hash value to an 64bit.
 * Can be used when strings are large enough for it to matter in terms of performance.
 *
 */
static inline uint64_t Hash128to64(const uint128_ht x)
{
    const uint64_t kMul = 0x9ddfea08eb382d69ULL;
    uint64_t a = (Uint128Low64(x) ^ Uint128High64(x)) * kMul;
    a ^= (a >> 47);
    uint64_t b = (Uint128High64(x) ^ a) * kMul;
    b ^= (b >> 47);
    b *= kMul;
    return b;
}

/* **** WeakHashLen32WithSeeds ****
 * @ Input arguments:
 *        - uint64_t w     : 1st Value
 *        - uint64_t x     : 2nd Value
 *        - uint64_t y     : 3rd Value
 *        - uint64_t z     : 4th Value
 *        - uint64_t a     : 5th Value
 *        - uint64_t b     : 6th Value
 * @ Return value:
 *        - uint128_ht ret : Combined value
 * @ Description:
 *
 * Return a 16-byte hash for 48 bytes.  Quick and dirty.
 * Callers do best to use "random-looking" values for a and b.
 *
 */
static inline uint128_ht WeakHashLen32WithSeeds(uint64_t w, uint64_t x, uint64_t y, uint64_t z, uint64_t a, uint64_t b)
{
    uint128_ht ret;

    a += w;
    b = rotl64(b + a + z, 21);
    uint64_t c = a;
    a += x;
    a += y;
    b += rotl64(a, 44);

    /* Create the 128-bit val */
    ret.low = a + z;
    ret.high = b + c;

    return ret;
}

/* **** WeakHashLen32WithSeedsUp ****
 * @ Input arguments:
 *        - const char* s :
 *        - uint64_t x    : 1st Value
 *        - uint64_t y    : 2rd Value
 * @ Return value:
 *        - uint128_ht ret : Combined value
 * @ Description:
 *
 * Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
 */
static inline uint128_ht WeakHashLen32WithSeedsUp(const char *s, uint64_t a, uint64_t b)
{
    return WeakHashLen32WithSeeds(Fetch64(s), Fetch64(s + 8), Fetch64(s + 16), Fetch64(s + 24), a, b);
}

/* **** HashLen16 ****
 * @ Input arguments:
 *        - uint64_t u   : First number
 *        - uint64_t v   : Second number
 *        - uint64_t mul : Multiplier
 * @ Return value:
 *        - uint64_t ret : Combined value
 * @ Description:
 *
 * Mixes 2 values along with multiplication.
 *
 */
static inline uint64_t HashLen16(uint64_t u, uint64_t v, uint64_t mul)
{
    uint64_t a = (u ^ v) * mul;
    a ^= (a >> 47);
    uint64_t b = (v ^ a) * mul;
    b ^= (b >> 47);
    b *= mul;
    return b;
}

/* **** HashLen16_128bit ****
 * @ Input arguments:
 *        - uint64_t u   : First number
 *        - uint64_t v   : Second number
 *        - uint64_t mul : Multiplier
 * @ Return value:
 *        - uint64_t ret : Combined value
 * @ Description:
 *
 * Mixes 2 values along with multiplication.
 *
 */
static inline uint64_t HashLen16_128bit(uint64_t u, uint64_t v)
{
    const uint128_ht val = { u, v };
    return Hash128to64(val);
}

/* **** Hash32Len0to4 ****
 * @ Input arguments:
 *        - const char *s : The object
 *        - size_t len    : The length of the object
 * @ Return value:
 *        - uint32_t ret  : Combined value
 * @ Description:
 *
 * Converts an object from 0 to 4 size .
 *
 */
static inline uint32_t Hash32Len0to4(const char *s, size_t len)
{
    uint32_t b = 0;
    uint32_t c = 9;
    for(size_t i = 0; i < len; i++)
    {
        signed char v = s[i];
        b = b * c1 + v;
        c ^= b;
    }

    return fmix32(mur(b, mur(len, c)));
}

/* **** Hash32Len5to12 ****
 * @ Input arguments:
 *        - const char *s : The object
 *        - size_t len    : The length of the object
 * @ Return value:
 *        - uint32_t ret  : Combined value
 * @ Description:
 *
 * Converts an object from 5 to 12 size .
 *
 */
static inline uint32_t Hash32Len5to12(const char *s, size_t len)
{
    uint32_t a = len, b = len * 5, c = 9, d = b;
    a += Fetch32(s);
    b += Fetch32(s + len - 4);
    c += Fetch32(s + ((len >> 1) & 4));
    return fmix32(mur(c, mur(b, mur(a, d))));
}

/* **** Hash32Len13to24 ****
 * @ Input arguments:
 *        - const char *s : The object
 *        - size_t len    : The length of the object
 * @ Return value:
 *        - uint32_t ret  : Combined value
 * @ Description:
 *
 * Converts an object from 13 to 24 size .
 *
 */
static inline uint32_t Hash32Len13to24(const char *s, size_t len)
{
    uint32_t a = Fetch32(s - 4 + (len >> 1));
    uint32_t b = Fetch32(s + 4);
    uint32_t c = Fetch32(s + len - 8);
    uint32_t d = Fetch32(s + (len >> 1));
    uint32_t e = Fetch32(s);
    uint32_t f = Fetch32(s + len - 4);
    uint32_t h = len;

    return fmix32(mur(f, mur(e, mur(d, mur(c, mur(b, mur(a, h)))))));
}

/* **** HashLen33to64 ****
 * @ Input arguments:
 *        - const char *s : The object
 *        - size_t len    : The length of the object
 * @ Return value:
 *        - uint32_t ret  : Combined value
 * @ Description:
 *
 * Converts an object from 33 to 64 size .
 *
 */
static inline uint64_t HashLen33to64(const char *s, size_t len)
{
    uint64_t mul = k2 + len * 2;
    uint64_t a = Fetch64(s) * k2;
    uint64_t b = Fetch64(s + 8);
    uint64_t c = Fetch64(s + len - 24);
    uint64_t d = Fetch64(s + len - 32);
    uint64_t e = Fetch64(s + 16) * k2;
    uint64_t f = Fetch64(s + 24) * 9;
    uint64_t g = Fetch64(s + len - 8);
    uint64_t h = Fetch64(s + len - 16) * mul;
    uint64_t u = rotl64(a + g, 43) + (rotl64(b, 30) + c) * 9;
    uint64_t v = ((a + g) ^ d) + f + 1;
    uint64_t w = bswap_64((u + v) * mul) + h;
    uint64_t x = rotl64(e + f, 42) + c;
    uint64_t y = (bswap_64((v + w) * mul) + g) * mul;
    uint64_t z = e + f + c;
    a = bswap_64((x + z) * mul + y) + b;
    b = ShiftMix((z + a) * mul + d + h) * mul;
    return b + x;
}

/* **** HashLen17to32 ****
 * @ Input arguments:
 *        - const char *s : The object
 *        - size_t len    : The length of the object
 * @ Return value:
 *        - uint64_t ret  : Combined value
 * @ Description:
 *
 * Converts an object from 17 to 32 size .
 *
 */
static inline uint64_t HashLen17to32(const char *s, size_t len)
{
    uint64_t mul = k2 + len * 2;
    uint64_t a = Fetch64(s) * k1;
    uint64_t b = Fetch64(s + 8);
    uint64_t c = Fetch64(s + len - 8) * mul;
    uint64_t d = Fetch64(s + len - 16) * k2;
    return HashLen16(rotl64(a + b, 43) + rotl64(c, 30) + d, a + rotl64(b + k2, 18) + c, mul);
}

/* **** HashLen0to16 ****
 * @ Input arguments:
 *        - const char *s : The object
 *        - size_t len    : The length of the object
 * @ Return value:
 *        - uint64_t ret  : Combined value
 * @ Description:
 *
 * Converts an object from 0 to 16 size .
 *
 */
static inline uint64_t HashLen0to16(const char *s, size_t len)
{
    if(len >= 8)
    {
        uint64_t mul = k2 + len * 2;
        uint64_t a = Fetch64(s) + k2;
        uint64_t b = Fetch64(s + len - 8);
        uint64_t c = rotl64(b, 37) * mul + a;
        uint64_t d = (rotl64(a, 25) + b) * mul;
        return HashLen16(c, d, mul);
    }
    if(len >= 4)
    {
        uint64_t mul = k2 + len * 2;
        uint64_t a = Fetch32(s);
        return HashLen16(len + (a << 3), Fetch32(s + len - 4), mul);
    }
    if(len > 0)
    {
        uint8_t a = s[0];
        uint8_t b = s[len >> 1];
        uint8_t c = s[len - 1];
        uint32_t y = ((uint32_t)(a)) + (((uint32_t)(b)) << 8);   // Casts are important here //
        uint32_t z = len + (((uint32_t)(c)) << 2);
        return ShiftMix(y * k2 ^ z * k0) * k2;
    }
    return k2;
}

/* **** mur ****
 * @ Input arguments:
 *        - uint32_t a   : The first value
 *        - uint32_t h   : The second value
 * @ Return value:
 *        - uint32_t ret : Combined value
 * @ Description:
 *
 * Helper function used in murmur and city hash for combining 32-bit values.
 *
 */
static inline uint32_t mur(uint32_t a, uint32_t h)
{
    a *= c1;
    a = rotl32(a, 17);
    a *= c2;
    h ^= a;
    h = rotl32(h, 19);
    return h * 5 + 0xe6546b64;
}

/* **** CityMurmur ****
 * @ Input arguments:
 *        - const char* s   : The key to be hashed
 *        - size_t len      : The size of the key
 *        - uint128_ht seed : The seed
 * @ Return value:
 *        - uint64_t hash  : Resulting hash
 * @ Description:
 *
 * Sub-routine used for CityHash128 routines.
 */
static inline uint128_ht CityMurmur(const char *s, size_t len, uint128_ht seed)
{
    uint64_t a = Uint128Low64(seed);
    uint64_t b = Uint128High64(seed);
    uint64_t c = 0;
    uint64_t d = 0;
    signed long l = len - 16;
    uint128_ht ret;

    if(l <= 0)   // len <= 16
    {
        a = ShiftMix(a * k1) * k1;
        c = b * k1 + HashLen0to16(s, len);
        d = ShiftMix(a + (len >= 8 ? Fetch64(s) : c));
    }
    else   // len > 16
    {
        c = HashLen16_128bit(Fetch64(s + len - 8) + k1, a);
        d = HashLen16_128bit(b + len, c + Fetch64(s + len - 16));
        a += d;
        do
        {
            a ^= ShiftMix(Fetch64(s) * k1) * k1;
            a *= k1;
            b ^= a;
            c ^= ShiftMix(Fetch64(s + 8) * k1) * k1;
            c *= k1;
            d ^= c;
            s += 16;
            l -= 16;
        } while(l > 0);
    }
    a = HashLen16_128bit(a, c);
    b = HashLen16_128bit(d, b);

    ret.low = a ^ b;
    ret.high = HashLen16_128bit(b, a);

    return ret;
}

/********************************** STRINGS HASH FUNCTIONS **********************************/

/* **** hash_jenkins_one_at_a_time ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 * @ Return value:
 *        - unsigned long hash    : The hash of the given key
 * @ Description:
 * A hash function that employs the hash_jenkins_one_at_a_time hash function.
 *
 */
static inline unsigned long hash_jenkins_one_at_a_time(const void *key, size_t len)
{
    size_t i = 0;
    unsigned long hash = 0;
    unsigned const char *us = (unsigned const char *)key;

    while(i != len)
    {
        hash += us[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

/* **** hash_sdbm ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 *        - size_t hashtable_size : The size of the hashtable
 * @ Return value:
 *        - unsigned long hash    : The hash of the given key
 * @ Description:
 * A hash function that employs the simple hash = hash * 65599 + str[i] function.
 * Works well on most cases, given a good hashtable size.
 */
static inline unsigned long hash_sdbm(const void *key, size_t len)
{
    unsigned const char *us = (unsigned const char *)key;
    unsigned long hash = 0;
    size_t i = 0;

    while(i != len)
    {
        hash = (hash << 6) + (hash << 16) - hash + us[i];
        i++;
    }

    return hash;
}

/* **** hash_sdbm ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 * @ Return value:
 *        - unsigned long hash    : The hash of the given key
 * @ Description:
 * A hash function that employs the simple hash = hash * 33 + str[i] function.
 * Works well on most cases, given a good hashtable size.
 */
static inline unsigned long hash_djb2(const void *key, size_t len)
{
    unsigned const char *us = (unsigned const char *)key;
    unsigned long hash = 5381;
    size_t i = 0;

    while(i != len)
    {
        hash = ((hash << 5) - hash) + us[i];
        i++;
    }

    return hash;
}

/********************************** INTEGER HASH FUNCTIONS **********************************/

/* **** hash_jenkins_integer ****
 * @ Input arguments:
 *        - uint32_t  key         : The key to be hashed
 * @ Return value:
 *        - uint32_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the Robert jenkins integer function.
 * Works well on most cases, given a good hashtable size.
 */
static inline uint32_t hash_jenkins_integer(uint32_t key)
{
    key = (key + 0x7ed55d16) + (key << 12);
    key = (key ^ 0xc761c23c) ^ (key >> 19);
    key = (key + 0x165667b1) + (key << 5);
    key = (key + 0xd3a2646c) ^ (key << 9);
    key = (key + 0xfd7046c5) + (key << 3);
    key = (key ^ 0xb55a4f09) ^ (key >> 16);

    return key;
}

/* **** hash_jenkins_integer ****
 * @ Input arguments:
 *        - uint32_t  key         : The key to be hashed
 * @ Return value:
 *        - uint32_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the Thomas wang integer function.
 * Works well on most cases, given a good hashtable size.
 */
static inline uint32_t hash_thomas_wang_integer(uint32_t key)
{
    key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * 0x27d4eb2d;
    key = key ^ (key >> 15);

    return key;
}

/* **** hash_32shift ****
 * @ Input arguments:
 *        - uint32_t  key         : The key to be hashed
 * @ Return value:
 *        - uint32_t hash         : The hash of the given key
 * @ Description:
 *
 * A simple hash function for integers.
 * Works well on most cases, given a good hashtable size.
 */
static inline uint32_t hash_32shift(uint32_t key)
{
    key = ~key + (key << 15);
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 2057;
    key = key ^ (key >> 16);

    return key;
}

/* **** hash_32_simp ****
 * @ Input arguments:
 *        - uint32_t  key         : The key to be hashed
 * @ Return value:
 *        - uint32_t hash         : The hash of the given key
 * @ Description:
 *
 * A simple hash function for integers.
 * Works well on most cases, given a good hashtable size.
 */
static inline uint32_t hash_32simp(uint32_t key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;

    return key;
}

/********************************** GENERIC HASH FUNCTIONS **********************************/

/* **** hash_murmur3_32 ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 *        - size_t seed           : The seed for the key
 * @ Return value:
 *        - uint32_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the MurmurHash3 function.
 *
 */
static inline uint32_t hash_murmur3_32(const void *key, size_t len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t *)key;
    const int nblocks = len / 4;
    uint32_t hash = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    /* Body part */
    const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

    for(int i = -nblocks; i; i++)
    {
        uint32_t k1 = getblock32(blocks, i);

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        hash ^= k1;
        hash = rotl32(hash, 13);
        hash = hash * 5 + 0xe6546b64;
    }

    /* Tail part */
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch(len & 3)
    {
    case 3:
    {
        k1 ^= tail[2] << 16;
    }
    case 2:
    {
        k1 ^= tail[1] << 8;
    }
    case 1:
    {
        k1 ^= tail[0];
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        hash ^= k1;
    }
    };

    /* Finalization part */
    hash ^= len;
    hash = fmix32(hash);

    return hash;
}

/* **** hash_MurmurHash64A ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 *        - size_t seed           : A seed used to randomize hash
 * @ Return value:
 *        - uint64_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the MurmurHash64A hash function.
 */
static inline uint64_t hash_MurmurHash64A(const void *key, size_t len, size_t seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = seed ^ (len * m);
    const uint64_t *data = (const uint64_t *)key;
    const uint64_t *end = data + (len / 8);

    while(data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char *data2 = (const unsigned char *)data;

    switch(len & 7)
    {
    case 7:
        h ^= (uint64_t)(data2[6]) << 48;
    case 6:
        h ^= (uint64_t)(data2[5]) << 40;
    case 5:
        h ^= (uint64_t)(data2[4]) << 32;
    case 4:
        h ^= (uint64_t)(data2[3]) << 24;
    case 3:
        h ^= (uint64_t)(data2[2]) << 16;
    case 2:
        h ^= (uint64_t)(data2[1]) << 8;
    case 1:
        h ^= (uint64_t)(data2[0]);
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

/* **** hash_MurmurOAAT64 ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 * @ Return value:
 *        - uint64_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the MurmurOAAT64 hash function.
 *
 */
static inline uint64_t hash_MurmurOAAT64(const void *key, size_t len)
{
    uint64_t hash = 525201411107845655;
    size_t i = 0;
    unsigned const char *us = (unsigned const char *)key;

    while(i != len)
    {
        hash ^= us[i];
        hash *= 0x5bd1e9955bd1e995;
        hash ^= hash >> 47;
        i++;
    }

    return hash;
}

/* **** hash_FNV1a ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 *        - size_t seed           : A seed used to randomize hash
 * @ Return value:
 *        - uint64_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the hash_FNV1a hash function.
 */
static inline uint32_t hash_FNV1a(const void *key, size_t key_len, uint32_t seed)
{
    const unsigned char *data = key;
    uint32_t hv = seed; /*2166136261U;*/
    size_t i;

    for(i = 0; i < key_len; i++)
    {
        hv ^= (uint32_t)data[i];
        hv += (hv << 1) + (hv << 4) + (hv << 7) + (hv << 8) + (hv << 24);
    }

    return hv;
}

/* **** hash_cityHash32 ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 * @ Return value:
 *        - uint32_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the CityHash32 hash function.
 *
 */
static inline uint32_t hash_cityHash32(const void *s, size_t len)
{
    if(len <= 24)
    {
        return len <= 12 ? (len <= 4 ? Hash32Len0to4(s, len) : Hash32Len5to12(s, len)) : Hash32Len13to24(s, len);
    }

    // len > 24
    uint32_t h = len, g = c1 * len, f = g;
    uint32_t a0 = rotl32(Fetch32(s + len - 4) * c1, 17) * c2;
    uint32_t a1 = rotl32(Fetch32(s + len - 8) * c1, 17) * c2;
    uint32_t a2 = rotl32(Fetch32(s + len - 16) * c1, 17) * c2;
    uint32_t a3 = rotl32(Fetch32(s + len - 12) * c1, 17) * c2;
    uint32_t a4 = rotl32(Fetch32(s + len - 20) * c1, 17) * c2;
    h ^= a0;
    h = rotl32(h, 19);
    h = h * 5 + 0xe6546b64;
    h ^= a2;
    h = rotl32(h, 19);
    h = h * 5 + 0xe6546b64;
    g ^= a1;
    g = rotl32(g, 19);
    g = g * 5 + 0xe6546b64;
    g ^= a3;
    g = rotl32(g, 19);
    g = g * 5 + 0xe6546b64;
    f += a4;
    f = rotl32(f, 19);
    f = f * 5 + 0xe6546b64;
    size_t iters = (len - 1) / 20;
    do
    {
        uint32_t a0 = rotl32(Fetch32(s) * c1, 17) * c2;
        uint32_t a1 = Fetch32(s + 4);
        uint32_t a2 = rotl32(Fetch32(s + 8) * c1, 17) * c2;
        uint32_t a3 = rotl32(Fetch32(s + 12) * c1, 17) * c2;
        uint32_t a4 = Fetch32(s + 16);
        h ^= a0;
        h = rotl32(h, 18);
        h = h * 5 + 0xe6546b64;
        f += a1;
        f = rotl32(f, 19);
        f = f * c1;
        g += a2;
        g = rotl32(g, 18);
        g = g * 5 + 0xe6546b64;
        h ^= a3 + a1;
        h = rotl32(h, 19);
        h = h * 5 + 0xe6546b64;
        g ^= a4;
        g = bswap_32(g) * 5;
        h += a4 * 5;
        h = bswap_32(h);
        f += a0;
        PERMUTE3_UINT32(f, h, g);
        s += 20;
    } while(--iters != 0);
    g = rotl32(g, 11) * c1;
    g = rotl32(g, 17) * c1;
    f = rotl32(f, 11) * c1;
    f = rotl32(f, 17) * c1;
    h = rotl32(h + g, 19);
    h = h * 5 + 0xe6546b64;
    h = rotl32(h, 17) * c1;
    h = rotl32(h + f, 19);
    h = h * 5 + 0xe6546b64;
    h = rotl32(h, 17) * c1;
    return h;
}

/* **** hash_cityHash64 ****
 * @ Input arguments:
 *        - void *key             : The key to be hashed
 *        - size_t len            : The length of the key
 * @ Return value:
 *        - uint64_t hash         : The hash of the given key
 * @ Description:
 *
 * A hash function that employs the CityHash64 hash function.
 *
 */
static inline uint64_t hash_cityHash64(const char *s, size_t len)
{
    /* Exit early in case of small length */
    if(len <= 32)
    {
        if(len <= 16)
        {
            return HashLen0to16(s, len);
        }
        else
        {
            return HashLen17to32(s, len);
        }
    }
    else if(len <= 64)
    {
        return HashLen33to64(s, len);
    }

    /* For strings over 64 bytes we hash the end first, and then as we */
    /* Loop we keep 56 bytes of state: v, w, x, y, and z. */
    uint64_t x = Fetch64(s + len - 40);
    uint64_t y = Fetch64(s + len - 16) + Fetch64(s + len - 56);
    uint64_t z = HashLen16_128bit(Fetch64(s + len - 48) + len, Fetch64(s + len - 24));
    uint128_ht v = WeakHashLen32WithSeedsUp(s + len - 64, len, z);
    uint128_ht w = WeakHashLen32WithSeedsUp(s + len - 32, y + k1, x);
    x = x * k1 + Fetch64(s);

    // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
    len = (len - 1) & ~((size_t)(63));
    do
    {
        x = rotl64(x + y + v.low + Fetch64(s + 8), 37) * k1;
        y = rotl64(y + v.high + Fetch64(s + 48), 42) * k1;
        x ^= w.high;
        y += v.low + Fetch64(s + 40);
        z = rotl64(z + w.low, 33) * k1;
        v = WeakHashLen32WithSeedsUp(s, v.high * k1, x + w.low);
        w = WeakHashLen32WithSeedsUp(s + 32, z + w.high, y + Fetch64(s + 16));

        /* Swap */
        uint64_t temp = z;
        z = x;
        x = temp;

        s += 64;
        len -= 64;
    } while(len != 0);

    return HashLen16_128bit(HashLen16_128bit(v.low, w.low) + ShiftMix(y) * k1 + z, HashLen16_128bit(v.high, w.high) + x);
}

/* **** hash_CityHash128WithSeed ****
 * @ Input arguments:
 *        - const char* s   : The key to be hashed
 *        - size_t len      : The size of the key
 *        - uint128_ht seed : The seed
 * @ Return value:
 *        - uint128_t hash  : Resulting hash
 * @ Description:
 *
 * A hash function that employs the CityHash128 (seeded) hash function.
 */
static inline uint128_ht hash_CityHash128WithSeed(const char *s, size_t len, uint128_ht seed)
{
    /* For small objects perform early exit */
    if(len < 128)
        return CityMurmur(s, len, seed);

    // We expect len >= 128 to be the common case.  Keep 56 bytes of state:
    // v, w, x, y, and z.
    uint128_ht v, w;
    uint64_t x = Uint128Low64(seed);
    uint64_t y = Uint128High64(seed);
    uint64_t z = len * k1;
    v.low = rotl64(y ^ k1, 49) * k1 + Fetch64(s);
    v.high = rotl64(v.low, 42) * k1 + Fetch64(s + 8);
    w.low = rotl64(y + z, 35) * k1 + x;
    w.high = rotl64(x + Fetch64(s + 88), 53) * k1;

    // This is the same inner loop as CityHash64(), manually unrolled.
    do
    {
        x = rotl64(x + y + v.low + Fetch64(s + 8), 37) * k1;
        y = rotl64(y + v.high + Fetch64(s + 48), 42) * k1;
        x ^= w.high;
        y += v.low + Fetch64(s + 40);
        z = rotl64(z + w.low, 33) * k1;
        v = WeakHashLen32WithSeedsUp(s, v.high * k1, x + w.low);
        w = WeakHashLen32WithSeedsUp(s + 32, z + w.high, y + Fetch64(s + 16));

        /* Swap */
        uint64_t temp = z;
        z = x;
        x = temp;

        s += 64;
        x = rotl64(x + y + v.low + Fetch64(s + 8), 37) * k1;
        y = rotl64(y + v.high + Fetch64(s + 48), 42) * k1;
        x ^= w.high;
        y += v.low + Fetch64(s + 40);
        z = rotl64(z + w.low, 33) * k1;
        v = WeakHashLen32WithSeedsUp(s, v.high * k1, x + w.low);
        w = WeakHashLen32WithSeedsUp(s + 32, z + w.high, y + Fetch64(s + 16));
        /* Swap */
        temp = z;
        z = x;
        x = temp;

        s += 64;
        len -= 128;
    } while(len >= 128);

    x += rotl64(v.low + z, 49) * k0;
    y = y * k0 + rotl64(w.high, 37);
    z = z * k0 + rotl64(w.low, 27);
    w.low *= 9;
    v.low *= k0;

    // If 0 < len < 128, hash up to 4 chunks of 32 bytes each from the end of s.
    for(size_t tail_done = 0; tail_done < len;)
    {
        tail_done += 32;
        y = rotl64(x + y, 42) * k0 + v.high;
        w.low += Fetch64(s + len - tail_done + 16);
        x = x * k0 + w.low;
        z += w.high + Fetch64(s + len - tail_done);
        w.high += v.low;
        v = WeakHashLen32WithSeedsUp(s + len - tail_done, v.low + z, v.high);
        v.low *= k0;
    }
    // At this point our 56 bytes of state should contain more than
    // enough information for a strong 128-bit hash.  We use two
    // different 56-byte-to-8-byte hashes to get a 16-byte final result.
    x = HashLen16_128bit(x, v.low);
    y = HashLen16_128bit(y + z, w.low);

    uint128_ht ret = { HashLen16_128bit(x + v.high, w.high) + y, HashLen16_128bit(x + w.high, y + v.high) };

    return ret;
}

/* **** hash_CityHash128 ****
 * @ Input arguments:
 *        - const char* s  : The key to be hashed
 *        - size_t len     : The size of the key
 * @ Return value:
 *        - uint128_t hash  : Resulting hash
 * @ Description:
 *
 * A hash function that employs the CityHash128 hash function.
 */
static inline uint128_ht hash_CityHash128(const char *s, size_t len)
{
    uint128_ht x;

    if(len >= 16)
    {
        x.low = Fetch64(s);
        x.high = Fetch64((s + 8) + k0);
        return hash_CityHash128WithSeed(s + 16, len - 16, x);
    }
    else
    {
        x.low = k0;
        x.high = k1;
        return hash_CityHash128WithSeed(s, len, x);
    }
}

/* **** hash_CityHash64WithSeeds ****
 * @ Input arguments:
 *        - const char* s  : The key to be hashed
 *        - size_t len     : The size of the key
 *        - uint64_t seed0 : The first seed
 *        - uint64_t seed1 : The second seed
 * @ Return value:
 *        - uint64_t hash  : Resulting hash
 * @ Description:
 *
 * A hash function that employs the CityHash64 (Doubly Seeded version).
 */
static inline uint64_t hash_CityHash64WithSeeds(const char *s, size_t len, uint64_t seed0, uint64_t seed1)
{
    return HashLen16_128bit(hash_cityHash64(s, len) - seed0, seed1);
}

/* **** hash_CityHash64WithSeed ****
 * @ Input arguments:
 *        - const char* s  : The key to be hashed
 *        - size_t len     : The size of the key
 *        - uint64_t seed  : The seed
 * @ Return value:
 *        - uint64_t hash  : Resulting hash
 * @ Description:
 *
 * A hash function that employs the CityHash64 (Seeded version).
 */
static inline uint64_t hash_CityHash64WithSeed(const char *s, size_t len, uint64_t seed)
{
    return hash_CityHash64WithSeeds(s, len, k2, seed);
}

/* **** siphash24 ****
 * @ Input arguments:
 *        - void *src             : The src(key) to be hashed
 *        - size_t len            : The length of the key
 *        - size_t key (seed)     : A seed used to randomize hash
 *                                  or our key for cryptographical use
 * @ Return value:
 *        - uint64_t hash         : The hash of the given src
 * @ Description:
 *
 * A hash function that employs the MurmurHash64A hash function.
 */
static inline uint64_t siphash24(const void *src, unsigned long src_sz, const char key[16])
{
    const uint64_t *_key = (uint64_t *)key;
    uint64_t k0 = _le64toh(_key[0]);
    uint64_t k1 = _le64toh(_key[1]);
    uint64_t b = (uint64_t)src_sz << 56;
    const uint64_t *in = (uint64_t *)src;

    uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
    uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
    uint64_t v3 = k1 ^ 0x7465646279746573ULL;

    while(src_sz >= 8)
    {
        uint64_t mi = _le64toh(*in);
        in += 1;
        src_sz -= 8;
        v3 ^= mi;
        DOUBLE_ROUND(v0, v1, v2, v3);
        v0 ^= mi;
    }

    uint64_t t = 0;
    uint8_t *pt = (uint8_t *)&t;
    uint8_t *m = (uint8_t *)in;
    switch(src_sz)
    {
    case 7:
        pt[6] = m[6];
    case 6:
        pt[5] = m[5];
    case 5:
        pt[4] = m[4];
    case 4:
        *((uint32_t *)&pt[0]) = *((uint32_t *)&m[0]);
        break;
    case 3:
        pt[2] = m[2];
    case 2:
        pt[1] = m[1];
    case 1:
        pt[0] = m[0];
    }
    b |= _le64toh(t);

    v3 ^= b;
    DOUBLE_ROUND(v0, v1, v2, v3);
    v0 ^= b;
    v2 ^= 0xff;
    DOUBLE_ROUND(v0, v1, v2, v3);
    DOUBLE_ROUND(v0, v1, v2, v3);
    return (v0 ^ v1) ^ (v2 ^ v3);
}

#endif
