/////////////////////////////////
// Header comment place holder //
/////////////////////////////////

/* Library inclusions */
#include "flat_sparse_hashtable.h"

/* Dev level inclusions*/
#include "sparse_hashtable_common.h"

/* Debug Limiters */
//#define DEBUG_HASH_INSERT
//#define DEBUG_HASH_SEARCH
//#define DEBUG_HASH_DELETE

/**************************  Macros and Definitions **************************/

/* Specify the comparator/copier used - Simple but makes the job done */
#if defined(HT_FLAT_INT_KEY)
    #define COMP_KEY_CB(x, y, sz)   ((*((int *)x)) == (*((int *)y)))
    #define COPY_KEY_CB(tar, x, sz) ((*((int *)tar)) = (*((int *)x)))

    /* Verify int is of size 4 */
    #if (INT_MAX == 0x7fffffff)
        #define HASHER_4BYTE
    #endif
#elif defined(HT_FLAT_LONG_INT_KEY)
    #define COMP_KEY_CB(x, y, sz)   ((*((long int *)x)) == (*((long int *)y)))
    #define COPY_KEY_CB(tar, x, sz) ((*((long int *)tar)) = (*((long int *)x)))

    /* Verify long is of size 4 or 8 */
    #if (LONG_MAX == 0x7fffffffffffffff)
        #define HASHER_8BYTE
    #elif (LONG_MAX == 0x7fffffff)
        #define HASHER_4BYTE
    #endif
#elif defined(HT_FLAT_UINT32_KEY)
    #define COMP_KEY_CB(x, y, sz)   ((*((uint32_t *)x)) == (*((uint32_t *)y)))
    #define COPY_KEY_CB(tar, x, sz) ((*((uint32_t *)tar)) = (*((uint32_t *)x)))
#elif defined(HT_FLAT_UINT64_KEY)
    #define COMP_KEY_CB(x, y, sz)   ((*((uint64_t *)x)) == (*((uint64_t *)y)))
    #define COPY_KEY_CB(tar, x, sz) ((*((uint64_t *)tar)) = (*((uint64_t *)x)))
#else
    #define COMP_KEY_CB(x, y, sz) (_ht_comp_key(x, y, sz))
    #define COPY_KEY_CB(x, y, sz) (_ht_copy_key(x, y, sz))
#endif

/* Probing technique
 * Default is linear and the one the original implementation also uses.
 * Quadratic probing works well, when extreme cases of primary clustering
 * happen in some degenerate cases. Bench and confirm only.
 * */
#define SPARSE_LIN_PROBE
//#define SPARSE_QUAD_PROBE

/************************** Private Structures **************************/

/* **** flat_hashtable_struct ****
 *
 * The manager structure for a Swiss Hashtable. The struct is
 * typedef on the header file and remains private to the user,
 * with only access to header file.
 */
struct flat_hashtable_struct
{
    /* Size of the hashtable */
    size_t hashtable_sz;
    size_t group_num;

    /* Related to entries */
    size_t step;
    size_t entry_sz;
    size_t key_sz;

    /* The table that holds entries+keys */
    char *table;

    /* The bitmap used for metadata of the keys */
    uint8_t *bitmap;

    /* Total entries in the map */
    size_t entries;

    /* Used for hashing - Randomizes hash for each run */
    size_t hash_seed;

    /* Iterator sub-structure */
    hashtable_iter_t iterator;
};

/**************************** Private function Prototypes ******************************/

/* Utility sub-routines */
static int _ht_comp_key(const void *key1, const void *key2, const size_t key_sz);
static void _ht_copy_key(const void *key1, const void *key2, const size_t key_sz);
static inline size_t _ht_flat_hasher(const char *key, size_t hash_sz);

/* Sub-routines for the main operations of the hashtable */
static void *_ht_flat_search(flat_hashtable_t *hashtable, const void *key);
static void *_ht_flat_insert(flat_hashtable_t *hashtable, const void *key, const void *entry, char flag);
static int _ht_flat_delete(flat_hashtable_t *hashtable, const void *key);
static int _ht_flat_resize(flat_hashtable_t *hashtable, size_t new_sz);

/* Iterator sub-routine */
static int _ht_iter_valid_group(flat_hashtable_t *hashtable, size_t *start_group, uint16_t *final_group_mask, short int direction);

/************************************ Internal Routines ************************************/

/* Comparator wrapper used for the comparison of the keys */
static int _ht_comp_key(const void *key1, const void *key2, const size_t key_sz)
{
    /* This speeds up our comparisons greatly for common key sizes */
    int ret = 0;

    switch(key_sz)
    {
    case 4:
        ret = ((*((uint32_t *)key1)) == (*((uint32_t *)key2)));
        break;
    case 8:
        ret = ((*((uint64_t *)key1)) == (*((uint64_t *)key2)));
        break;
    default:
        ret = !(memcmp(key1, key2, key_sz));
    }

    return ret;
}

/* Copier wrapper used for the insertion of the keys */
static void _ht_copy_key(const void *tar, const void *x, const size_t key_sz)
{
    /* This speeds up our insertion greatly for common key sizes */
    switch(key_sz)
    {
    case 4:
        ((*((uint32_t *)tar)) = (*((uint32_t *)x)));
        break;
    case 8:
        ((*((uint64_t *)tar)) = (*((uint64_t *)x)));
        break;
    default:
        memcpy((void *)tar, (void *)x, key_sz);
    }
}

/* Hasher wrapper used for the hashing of the keys */
static inline size_t _ht_flat_hasher(const char *key, size_t hash_sz)
{
/* Choose the a function based on specific key_size overide */
#if defined(HASHER_4BYTE) || defined(HT_FLAT_UINT32_KEY)
    return hash_32simp(*((uint32_t *)key));
#elif defined(HASHER_8BYTE) || defined(HT_FLAT_UINT64_KEY)
    return fmix64(*((uint64_t *)key));
#else
    switch(hash_sz)
    {
    case 4:
        return hash_32simp(*((uint32_t *)key));
    case 8:
        return fmix64(*((uint64_t *)key));
    default:
        return hash_MurmurOAAT64(key, hash_sz); /* Default hasher - Good for large payloads */
    }
#endif
}

/* Finds the next valid group in the hashtable - Forward or backward (1 or -1) */
static int _ht_iter_valid_group(flat_hashtable_t *hashtable, size_t *start_group, uint16_t *final_group_mask, short int direction)
{
    while((*start_group) < hashtable->group_num)
    {
        uint8_t *bitmap_pos = &hashtable->bitmap[(*start_group) << GROUP_SIZE_SHIFT];
        uint16_t valid_entries_mask = ~(_ht_and_mask(bitmap_pos, HIGH_BIT_MASK));

        /* Break condition */
        if(valid_entries_mask)
        {
            *final_group_mask = valid_entries_mask;
            return ITER_VALID;
        }

        /* Probe to the next group */
        (*start_group) += direction;
    }

    return ITER_NOT_VALID;
}

/* The main lookup sub-routine */
static void *_ht_flat_search(flat_hashtable_t *hashtable, const void *key)
{
    /* Constants */
    const size_t step = hashtable->step;
    const size_t group_mask = hashtable->group_num - 1; /* Now this becomes a mask */
    const char *table = hashtable->table;

    /* Metadata for the given key */
    const size_t hash = _ht_flat_hasher(key, hashtable->key_sz);
    const uint8_t bitmap_ctrl = hash & GROUP_H2_MASK;

    /* Starting group index */
    size_t group_idx = (hash >> GROUP_H1_SHIFT) & group_mask;
#ifndef SPARSE_LIN_PROBE
    size_t probe_step = 1;
#endif

    while(1)
    {
        /* Create an index for the matches */
        size_t i = group_idx * GROUP_SIZE;

        /* Create the masks */
        uint8_t *bitmap_pos = &hashtable->bitmap[i];
        uint16_t eq_mask = _ht_eq_mask(bitmap_pos, bitmap_ctrl);
        uint16_t empty_mask = _ht_eq_mask(bitmap_pos, ENTRY_EMPTY);

        /* Find matches */
        while(eq_mask)
        {
            size_t pos = _get_first_set_bit_pos(eq_mask);

            /* Found an empty position */
            if(HT_LIKELY(COMP_KEY_CB(&table[(i + pos) * step], key, hashtable->key_sz)))
                return (void *)&table[(i + pos) * step + hashtable->key_sz];

            /* Unset this entry */
            eq_mask ^= 1 << pos;
        }

        /* Search stop condition */
        if(HT_LIKELY(empty_mask))
            return NULL;

/* Probe */
#ifdef SPARSE_LIN_PROBE
        group_idx = (group_idx + 1) & group_mask;
#else
        group_idx = (group_idx + probe_step) & group_mask;
        probe_step++;
#endif
    }

    return NULL; /* Suppress compiler warnings */
}

/* The main insertion sub-routine. Performs different kinds of insertion based on action */
static void *_ht_flat_insert(flat_hashtable_t *hashtable, const void *key, const void *entry, char flag)
{
    /* Resizing = NO_SEARCH | Insert = SEARCH_NO_REPLACE */
    if(flag != NO_SEARCH)
    {
        void *ret = _ht_flat_search(hashtable, key);
        if(ret)
            return ret;
    }

    /* Restart the search - Constants */
    const size_t step = hashtable->step;
    const size_t group_mask = hashtable->group_num - 1; /* Now this is a bitmask */
    const size_t hash = _ht_flat_hasher(key, hashtable->key_sz);

    /* Initial conditions */
    char *table = hashtable->table;
    size_t group_idx = (hash >> GROUP_H1_SHIFT) & group_mask;
#ifndef SPARSE_LIN_PROBE
    size_t probe_step = 1;
#endif

    while(1)
    {
        /* Create the new mask */
        size_t i = group_idx * GROUP_SIZE;
        uint8_t *bitmap_pos = &hashtable->bitmap[i];
        uint16_t empty_or_del_mask = _ht_and_mask(bitmap_pos, HIGH_BIT_MASK);

        /* Found empty spot */
        if(empty_or_del_mask)
        {
            size_t pos = _get_first_set_bit_pos(empty_or_del_mask);

            /* Insertion in the hashtable */
            COPY_KEY_CB(&table[(i + pos) * step], key, hashtable->key_sz);
            memcpy(&table[(i + pos) * step + hashtable->key_sz], entry, hashtable->entry_sz);

            /* Fix the bitmap */
            hashtable->bitmap[i + pos] = hash & (GROUP_H2_MASK);
            hashtable->entries++;

            return NULL;
        }

/* Probe */
#ifdef SPARSE_LIN_PROBE
        group_idx = (group_idx + 1) & group_mask;
#else
        group_idx = (group_idx + probe_step) & group_mask;
        probe_step++;
#endif
    }

    return NULL; /* Suppress compiler warnings */
}

/* The main delete sub-routine */
static int _ht_flat_delete(flat_hashtable_t *hashtable, const void *key)
{
    /* Constants */
    const size_t step = hashtable->step;
    const size_t group_mask = hashtable->group_num - 1; /* Now this becomes a mask */
    const size_t hash = _ht_flat_hasher(key, hashtable->key_sz);
    const uint8_t bitmap_ctrl = hash & GROUP_H2_MASK;

    /* Starting group index */
    char *table = hashtable->table;
    size_t group_idx = (hash >> GROUP_H1_SHIFT) & group_mask;
#ifndef SPARSE_LIN_PROBE
    size_t probe_step = 1;
#endif

    while(1)
    {
        /* Index for the <key, entries> table */
        size_t i = group_idx * GROUP_SIZE;

        /* Create the new masks */
        uint8_t *bitmap_pos = &hashtable->bitmap[i];
        uint16_t eq_mask = _ht_eq_mask(bitmap_pos, bitmap_ctrl);
        uint16_t empty_mask = _ht_eq_mask(bitmap_pos, ENTRY_EMPTY);

        while(eq_mask)
        {
            /* Found an empty position */
            size_t pos = _get_first_set_bit_pos(eq_mask);

            if(COMP_KEY_CB(&table[(i + pos) * step], key, hashtable->key_sz))
            {
                /* Put a tombstone only if there are no empty entries in the group */
                hashtable->bitmap[i + pos] = (empty_mask) ? ENTRY_EMPTY : ENTRY_DELETED;
                hashtable->entries--;

                return HASH_OK;
            }

            /* Unset this entry */
            eq_mask ^= 1 << pos;
        }

        /* Stop condition */
        if(empty_mask)
            return HASH_ENTRY_NOT_EXISTS;

/* Probe */
#ifdef SPARSE_LIN_PROBE
        group_idx = (group_idx + 1) & group_mask;
#else
        group_idx = (group_idx + probe_step) & group_mask;
        probe_step++;
#endif
    }
}

/* The main resize sub-routine. Performs up and down resizes. */
static int _ht_flat_resize(flat_hashtable_t *hashtable, size_t new_sz)
{
    /* Constants */
    const size_t key_sz = hashtable->key_sz, step = hashtable->step;
    const size_t num_of_groups = hashtable->hashtable_sz >> GROUP_SIZE_SHIFT;

    /* New tables */
    char *new_table = malloc((new_sz * step) * sizeof(char));
    uint8_t *new_bitmap = aligned_alloc(BITMAP_FORCE_ALLIGN, new_sz * sizeof(uint8_t));

    if(!new_table || !new_bitmap)
    {
        free(new_table);
        free(new_bitmap);
        return HASH_REHASH_MEM_ALLOC;
    }

    /* Set every entry as empty in the new bitmap */
    memset(new_bitmap, ENTRY_EMPTY, new_sz * sizeof(uint8_t));

    /* Setup variables - Old table parameters */
    char *old_table = hashtable->table;
    uint8_t *old_bitmap = hashtable->bitmap;

    /* Update the new_table parameters */
    hashtable->bitmap = new_bitmap;
    hashtable->table = new_table;
    hashtable->hashtable_sz = new_sz;
    hashtable->group_num = new_sz >> GROUP_SIZE_SHIFT;
    hashtable->entries = 0;

    /* Iterate over the old table */
    for(size_t i = 0; i < num_of_groups; i++)
    {
        /* Find empty_or_deleted -> Invert for the valid entries */
        uint16_t valid_entries_mask = ~(_ht_and_mask(&old_bitmap[i * GROUP_SIZE], HIGH_BIT_MASK));
        size_t cur_idx = i * GROUP_SIZE * step;

        /* This iteration step is trivial - No entries in this group */
        if(!valid_entries_mask)
            continue;

        /* Extract every <key, entry> pair and insert in the new table */
        while(valid_entries_mask)
        {
            size_t pos = _get_first_set_bit_pos(valid_entries_mask);
            size_t tmp = cur_idx + pos * step;

            /* Insert safely - Each key is unique */
            _ht_flat_insert(hashtable, &old_table[tmp], &old_table[tmp + key_sz], NO_SEARCH);

            /* Unset this entry */
            valid_entries_mask ^= 1 << pos;
        }
    }

    /* Re-initialise iterator */
    hashtable->iterator.iter_state = ITER_NOT_VALID;

    /* Free the old tables */
    free(old_bitmap);
    free(old_table);

    return HASH_OK;
}

/************************************ Main Routines for Flat ************************************/

flat_hashtable_t *ht_flat_create(size_t hashtable_sz, size_t entry_sz, size_t key_sz, int *error_code)
{
    /* Set the error code */
    *error_code = HASH_OK;

    /* Check input by the user - Necessary inputs */
    if(!hashtable_sz || !entry_sz || !key_sz)
    {
        *error_code = HASH_WRONG_ARGUMENT;
        return NULL;
    }

    /* Fix the size for the hashtable
     * Minimum is 32 entries (2 groups) since alligned alloc requires multiples of
     * allignment (address sanitizer report ?) of 32. Not much of a difference 32 is a pretty good
     * starting size. This is actually a problem with the standard (there are 3 versions of
     * what to do in this situatin)so just allocate more, eh.
     * TODO -> Maybe this could be also moved to a better C99 solution ?*/
    hashtable_sz = (hashtable_sz > (2 * GROUP_SIZE)) ? _get_next_power_of_two(hashtable_sz) : (2 * GROUP_SIZE);

    /* Find the bucket size along with the bitmap size */
    size_t bucket_sz = key_sz + entry_sz;

    /* Try to allocate memory for the structure */
    flat_hashtable_t *hashtable = malloc(sizeof(flat_hashtable_t));

    if(!hashtable)
    {
        *error_code = HASH_CREATE_MEM_ALLOC;
        return NULL;
    }

    hashtable->table = malloc((hashtable_sz * bucket_sz) * sizeof(char));
    hashtable->bitmap = aligned_alloc(BITMAP_FORCE_ALLIGN, hashtable_sz * sizeof(uint8_t));

    /* Final assertions */
    if(!hashtable->table || !hashtable->bitmap)
    {
        *error_code = HASH_CREATE_MEM_ALLOC;
        free(hashtable->table);
        free(hashtable->bitmap);
        free(hashtable);
        return NULL;
    }

    /* Initialize the bitmap - Set it to 0xFF everywhere */
    memset(hashtable->bitmap, ENTRY_EMPTY, hashtable_sz * sizeof(uint8_t));

    /* Main parameters of the hashtable - Initialize all the sizes and the seed */
    hashtable->hashtable_sz = hashtable_sz;
    hashtable->group_num = hashtable_sz >> GROUP_SIZE_SHIFT;
    hashtable->entry_sz = entry_sz;
    hashtable->key_sz = key_sz;
    hashtable->step = bucket_sz;
    hashtable->entries = 0;

    /* Seeding of the hashtable */
    //  srand(time(NULL));
    //  hashtable->hash_seed = rand();

    /* Initialize iterator */
    hashtable->iterator.iter_state = ITER_NOT_VALID;

    return hashtable;
}

void *ht_flat_search(flat_hashtable_t *hashtable, const void *key)
{
    /* Check user input */
    if(HT_UNLIKELY(!key || !hashtable))
        return NULL;

    return _ht_flat_search(hashtable, key);
}

void *ht_flat_insert(flat_hashtable_t *hashtable, const void *key, const void *entry, int *error_code)
{
    /* Check user input */
    if(HT_UNLIKELY(!key || !entry || !hashtable || !error_code))
    {
        *error_code = HASH_WRONG_ARGUMENT;
        return NULL;
    }

    /* Set error code */
    *error_code = HASH_OK;

    void *ret = _ht_flat_insert(hashtable, key, entry, SEARCH_NO_REPLACE);

    /* Also check if there is a need for rehashing */
    if(hashtable->entries > UPPER_LIMIT(hashtable->hashtable_sz))
    {
        /* Inforn in case of rehashing error - Resize by doubling */
        *error_code = _ht_flat_resize(hashtable, hashtable->hashtable_sz << 1);
    }

    return ret;
}

int ht_flat_emplace(flat_hashtable_t *hashtable, const void *key, const void *entry)
{
    int status = HASH_OK;

    /* Check user input */
    if(HT_UNLIKELY(!key || !entry || !hashtable))
        return HASH_WRONG_ARGUMENT;

    void *ret = _ht_flat_insert(hashtable, key, entry, SEARCH_NO_REPLACE);

    /* In case of replace we can directly replace the entry after the search */
    if(ret)
        memcpy(ret, entry, hashtable->entry_sz);

    /* Also check if there is a need for rehashing */
    if(hashtable->entries > UPPER_LIMIT(hashtable->hashtable_sz))
    {
        /* Inforn in case of rehashing error - Resize by doubling */
        status = _ht_flat_resize(hashtable, hashtable->hashtable_sz << 1);
    }

    return status;
}

int ht_flat_delete(flat_hashtable_t *hashtable, const void *key)
{
    int ret = HASH_OK;

    /* Check user input - We go for experienced users */
    if(HT_UNLIKELY(!key || !hashtable))
        return HASH_WRONG_ARGUMENT;

    ret = _ht_flat_delete(hashtable, key);

    /* Also check if there is a need for rehashing */
    if(hashtable->entries < LOWER_LIMIT(hashtable->hashtable_sz))
    {
        /* Inforn in case of rehashing error - Resize by halving */
        if(hashtable->hashtable_sz != (2 * GROUP_SIZE))
            ret = _ht_flat_resize(hashtable, hashtable->hashtable_sz >> 1);
    }

    return ret;
}

void ht_flat_free(flat_hashtable_t *hashtable)
{
    /* Nothing to free, return */
    if(!hashtable)
        return;

    /* Free the tables and the structure itself */
    free(hashtable->bitmap);
    free(hashtable->table);
    free(hashtable);
}

flat_hashtable_tuple_t ht_flat_start_it(flat_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    flat_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };

    hashtable->iterator.iter_state = ITER_NOT_VALID;

    if(hashtable->entries)
    {
        /* Get the next valid group */
        hashtable->iterator.cur_group = 0;
        hashtable->iterator.cur_group_mask = 0;
        hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, 1);

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = ((hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos) * hashtable->step;
        ret_iter.key = &hashtable->table[idx];
        ret_iter.entry = ((char *)ret_iter.key) + hashtable->key_sz;
    }

    return ret_iter;
}

flat_hashtable_tuple_t ht_flat_next_it(flat_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    flat_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };

    if(hashtable->iterator.iter_state == ITER_VALID)
    {
        /* Find the next group */
        if(!hashtable->iterator.cur_group_mask)
        {
            /* Get the next valid group */
            hashtable->iterator.cur_group++;
            hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, 1);

            /* We reached the end */
            if(hashtable->iterator.iter_state == ITER_NOT_VALID)
            {
                return ret_iter;
            }
        }

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = ((hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos) * hashtable->step;
        ret_iter.key = &hashtable->table[idx];
        ret_iter.entry = ((char *)ret_iter.key) + hashtable->key_sz;
    }

    return ret_iter;
}

flat_hashtable_tuple_t ht_flat_prev_it(flat_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    flat_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };

    if(hashtable->iterator.iter_state == ITER_VALID)
    {
        if(!hashtable->iterator.cur_group_mask)
        {
            /* We reached the end */
            if(!hashtable->iterator.cur_group)
            {
                return ret_iter;
            }

            /* Go to the previous group */
            hashtable->iterator.cur_group--;
            hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, -1);
        }

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = ((hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos) * hashtable->step;
        ret_iter.key = &hashtable->table[idx];
        ret_iter.entry = ((char *)ret_iter.key) + hashtable->key_sz;
    }

    return ret_iter;
}

flat_hashtable_tuple_t ht_flat_end_it(flat_hashtable_t *hashtable)
{
    /* Iterator standard initialization */
    flat_hashtable_tuple_t ret_iter = { .entry = NULL, .key = NULL };

    hashtable->iterator.iter_state = ITER_VALID;

    if(hashtable->entries)
    {
        /* Get the first valid group - Always returns valid */
        hashtable->iterator.cur_group = hashtable->group_num - 1;
        hashtable->iterator.cur_group_mask = 0;
        hashtable->iterator.iter_state = _ht_iter_valid_group(hashtable, &hashtable->iterator.cur_group, &hashtable->iterator.cur_group_mask, -1);

        size_t pos = _get_first_set_bit_pos(hashtable->iterator.cur_group_mask);
        hashtable->iterator.cur_group_mask ^= 1 << pos;

        size_t idx = ((hashtable->iterator.cur_group << GROUP_SIZE_SHIFT) + pos) * hashtable->step;
        ret_iter.key = &hashtable->table[idx];
        ret_iter.entry = ((char *)ret_iter.key) + hashtable->key_sz;
    }

    return ret_iter;
}

size_t ht_flat_get_entries(flat_hashtable_t *hashtable)
{
    return hashtable->entries;
}

size_t ht_flat_get_capacity(flat_hashtable_t *hashtable)
{
    return hashtable->hashtable_sz;
}

double ht_flat_get_load_factor(flat_hashtable_t *hashtable)
{
    return ((double)hashtable->entries) / hashtable->hashtable_sz;
}

void ht_flat_print_mem_usage(flat_hashtable_t *hashtable)
{
    size_t bitmap_sz = hashtable->hashtable_sz;
    size_t hashtable_sz = hashtable->hashtable_sz * (hashtable->key_sz + hashtable->entry_sz);
    size_t manager_structure_mem = sizeof(flat_hashtable_t);

    size_t valid_entries = hashtable->entries * (hashtable->key_sz + hashtable->entry_sz);
    size_t valid_bitmap = hashtable->entries;

    size_t total_memory = bitmap_sz + hashtable_sz + manager_structure_mem;
    size_t used_memory = valid_entries + valid_bitmap + manager_structure_mem;

    printf("******SPARSE_NODE_STAT INFO******\n");
    printf("Entries: %ld - Capacity: %ld\n", hashtable->entries, hashtable->hashtable_sz);
    printf("Total mem used (bytes): %ld\n", total_memory);
    printf("Effective mem used(bytes): %ld\n", used_memory);
    printf("Memory util (bytes): %f\n", (double)used_memory / total_memory);
}
