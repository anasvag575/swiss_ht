/////////////////////////////////
// Header comment place holder //
/////////////////////////////////

#ifndef __SPARSE_HASHTABLE_COMMON_H
#define __SPARSE_HASHTABLE_COMMON_H

#include <stddef.h>
#include <stdio.h>
#include <emmintrin.h>
#include <limits.h>
#include <time.h>

/* Hash functors and mixers */
#include "hash_function.h"

/****************************** INTRINSICS/INSTRUCTIONS & COMPILER SETTINGS ******************************/

/* Branch prediction related */
#if defined(__GNUC__) || defined(__clang__)
    #define INSTR_BUILTINS
    #define HT_LIKELY(x)   (__builtin_expect(x, 1))
    #define HT_UNLIKELY(x) (__builtin_expect(x, 0))
#else
    #define HT_LIKELY(x)   (x)
    #define HT_UNLIKELY(x) (x)
#endif

/* SSE instructions to use on bitmasks */
#if defined(__SSE2__) && defined(__SSE__)
    #define SSE_INSTR_ACTIVE
#endif

/* Instructions Related - Not needed for now */
#ifdef INSTR_BUILTINS
    #define BUILTIN_LEADING_ZERO64(x)  (__builtin_clzl(x))
    #define BUILTIN_LEADING_ZERO32(x)  (__builtin_clz(x))
    #define BUILTIN_TRAILING_ZERO32(x) (__builtin_ctz(x))
    #define BUILTIN_COUNT_SET32(x)     (__builtin_popcount(x))
#endif

/* Mask converter in case of different Endianess */
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define reverse_or_nop(x) (_reverse_bits16(x))
#else
    #define reverse_or_nop(x) (x)
#endif

/****************************** OPCODES / CONSTANTS / STRUCTURES ******************************/

/* **** hashtable_error_code - hash_err_t ****
 *
 * Enum that has the error codes that might arise during the operation of the hashtable.
 */
typedef enum hashtable_error_code_enum
{
    HASH_OK = 0,
    HASH_INVALID_SIZE = 1,       // For flat only - Reached maximum entry or key size //
    HASH_WRONG_ARGUMENT = 2,     // Wrong input in function //
    HASH_CREATE_MEM_ALLOC = 3,   // Memory allocation error in creation of a new hashtable //
    HASH_REHASH_MEM_ALLOC = 4,   // Memory allocation error in rehashing operation //
    HASH_ENTRY_NOT_EXISTS = 5,   // Entry does not exist when trying to delete it //
} hash_err_t;

/* Iterator status */
#define ITER_VALID     1
#define ITER_NOT_VALID 0

/* Insertion modes */
#define SEARCH_NO_REPLACE 1
#define NO_SEARCH         0

/* **** hashtable_iter_t ****
 * Current context of an iterator
 */
typedef struct hashtable_iter_struct
{
    size_t cur_group;
    uint16_t cur_group_mask;
    uint8_t iter_state;
} hashtable_iter_t;

/* Opcodes for each entry */
#define ENTRY_VALID   0x00
#define ENTRY_DELETED 0x80
#define ENTRY_EMPTY   0xff

/* These are used for the minimum size of the table and the group size */
#define MIN_POWER_OF_TWO 4
#define GROUP_SIZE       16
#define GROUP_SIZE_SHIFT 4

/* Masks used for the bitmap manipulation during operations */
#define GROUP_H1_SHIFT 7
#define GROUP_H2_MASK  0x7f
#define HIGH_BIT_MASK  0x80
#define LOW_BIT_MASK   0x01

/* Related to the usage of the bitmap */
#define BITMAP_FORCE_ALLIGN_SHIFT 5
#define BITMAP_FORCE_ALLIGN       32

/* Upper and lower factors for resizing */
#define UPPER_LIMIT(x) EXACTLY_85_PERCENT(x)
#define LOWER_LIMIT(x) APPROX_40_PERCENT(x)

/* Upper Limit */
#define EXACTLY_75_PERCENT(x) ((x >> 1) + (x >> 2))
#define EXACTLY_85_PERCENT(x) ((x) - (x >> 3))
#define APPROX_70_PERCENT(x)  (((x)*717) >> 10)

/* Lower Limit */
#define APPROX_40_PERCENT(x)  (((x)*409) >> 10)
#define APPROX_35_PERCENT(x)  (((x)*307) >> 10)
#define EXACTLY_25_PERCENT(x) (x >> 2)

/* Used to get the next power of 2 or to transform the current size into a power of 2. */
static inline size_t _get_next_power_of_two(size_t size)
{
    /* Check if size is already a power of 2 */
    if(!(size & (size - 1)))
        return size;

    /* SWAR Bit-folding algorithm */
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size |= size >> 32;

    return size + 1;
}

/* Used to get position of the rightmost set bit (Indexes start at 0) */
static inline size_t _get_first_set_bit_pos(uint32_t x)
{
#ifdef INSTR_BUILTINS
    return BUILTIN_TRAILING_ZERO32(x);
#else
    const uint32_t a = 0x05f66a47; /* magic number, found by brute force */
    static const unsigned decode[32]
    = { 0, 1, 2, 26, 23, 3, 15, 27, 24, 21, 19, 4, 12, 16, 28, 6, 31, 25, 22, 14, 20, 18, 11, 5, 30, 13, 17, 10, 29, 9, 8, 7 };
    x = a * (x & (-x));
    return decode[x >> 27];
#endif
}

/* Reverses the bit ordering of a 16-bit integer (Same algorithm can be applied for larger though) */
static inline uint16_t _reverse_bits16(uint16_t x)
{
    x = (((x & 0xaaaa) >> 1) | ((x & 0x5555) << 1));
    x = (((x & 0xcccc) >> 2) | ((x & 0x3333) << 2));
    x = (((x & 0xf0f0) >> 4) | ((x & 0x0f0f) << 4));
    x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));

    return x;
}

/* Reverses the bit ordering of a 32-bit integer (Same algorithm can be applied for larger though) */
static inline uint32_t _reverse_bits32(uint32_t x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

    return ((x >> 16) | (x << 16));
}

/* Creates mask for the 16 entry metadata comparison (equality operation during search) */
static inline uint16_t _ht_eq_mask(uint8_t *group_meta, uint8_t ctrl_data)
{
    /* First we have to have a mask of 2-64bit numbers.
     * Then we set each byte for the mask with the ctrl_data value (all 16bytes).
     * Example {Group meta} = {0xff, 0x75, 0x53, ..(zeros).. , 0x00}
     *         {ctrl_data } = 0x75
     *
     * Once done the new vector will produce:
     * Example = {0x00, 0xFF, .. (zeros).., 0x00}
     *
     * Means that the 2nd byte matches, so we collapse all these into a mask
     * which will be a 16bit value (32-bit due to the instruction) where:
     * Example = 16b01..(zeros)..00
     *
     * For the vector _mm_cmpeq_epi8() call and (*(__m128i *)
     *
     * The order is in that way since the entries are stored in reverse in the bitmap, i.e:
     * Taking an arbitrary group in the bitmap -> {G1, G2, G3, G4 ...}
     *
     * G1 for example will be 16bytes of memory since each meta info
     * is a byte and a group has 16 entries, so:
     *
     * G1 -> {e15, e14, e13, e12, e11, e10, e9, e8 ,e7, e6, e5, e4, e3, e2, e1, e0}
     * This will happen since we represent from e7 - e0 as a uint64_t and the same for the
     * second part of the group when in reality have the same ordering in .
     *
     */
#ifdef SSE_INSTR_ACTIVE

    uint16_t mask;

    /* Create the vectors */
    __m128i ctrl_vec = _mm_set1_epi8(ctrl_data);
    __m128i *groum_vec = (__m128i *)group_meta;

    /* First we perform the compare operation and then collapse into a 16-bit mask */
    mask = _mm_movemask_epi8(_mm_cmpeq_epi8(*groum_vec, ctrl_vec));

    /* Return the mask and reverse if need be (Big Endian) */
    return reverse_or_nop(mask);
#else
    /* This method is basically raw bit manipulation - So we just compare each uint and set the appropriate
     * mask */
    mask = 0;

    /* We compare the vector one by one - Faster way ? */
    for(uint8_t i = 0; i < GROUP_SIZE; i++)
    {
        /* We compare each character - Set the bit on match */
        if(group_meta[i] == ctrl_data)
            mask |= 1 << i;
    }

    return mask;
#endif
}

/* Creates mask for the 16 entry metadata comparison (AND operation during resize) */
static inline uint16_t _ht_and_mask(uint8_t *group_meta, uint8_t ctrl_mask)
{
    /* Basically the SSE instructions follows the same rules as compare but instead of direct comparison
     * we perform an 128-bit AND operation between 2 vectors.
     * For the ordering and some limitations check _ht_eq_mask().
     *
     * This routine is used to perform the empty or deleted entry operation,
     * which returns the positions of empty or deleted entries in the hashtable.
     */
#ifdef SSE_INSTR_ACTIVE
    uint16_t mask;

    /* Create the vectors */
    __m128i ctrl_vec = _mm_set1_epi8(ctrl_mask);
    __m128i *group_vec = (__m128i *)group_meta;

    /* First we perform the compare operation and then collapse into a 16-bit mask */
    mask = _mm_movemask_epi8(_mm_and_si128(*group_vec, ctrl_vec));

    /* Return the mask and reverse if need be (Big Endian) */
    return reverse_or_nop(mask);
#else
    /* This method is basically raw bit manipulation - So we just AND each uint and set the appropriate mask
     */
    mask = 0;

    /* We compare the vector one by one - Faster way ? */
    for(uint8_t i = 0; i < GROUP_SIZE; i++)
    {
        /* We compare each character - Set the bit on match */
        if(group_meta[i] & ctrl_mask)
            mask |= 1 << i;
    }

    return mask;
#endif
}

#endif   // _HASHTABLE_COMMON_H //
