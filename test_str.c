/////////////////////////////////
// Header comment place holder //
/////////////////////////////////
#include "node_sparse_hashtable.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Our testcases */
char **dictionary = NULL;
char **duplicates = NULL;
int dict_size = 0;
int dupl_size = 0;

typedef struct test_entry_data_struct
{
    char *key;
    int num;
} test_entry;

/* Destructor function */
void destruct(void *data, void *key)
{
    test_entry *temp = (test_entry *)data;

    free(temp->key);
    free(temp);
}

/* Destructor function */
int comp(const void *a, const void *b)
{
    char *temp_a = (char *)a;
    char *temp_b = (char *)b;

    return !strcmp(temp_a, temp_b);
}

/* Printer function */
size_t hash(const void *key)
{
    char *temp_key = (char *)key;
    return strlen(temp_key);
}

/* Utility - Parses testcase file */
int parse_testcases(char *file)
{
    FILE *fp = NULL;
    int iters = 0;
    char *readline = NULL;
    size_t linelen = 0;
    size_t bytesread = 1;

    fp = fopen(file, "r");

    if(!fp)
    {
        printf("File does not exist\n");
        printf("Usage\n./test_str input_filename\n");
        return -1;
    }

    /* Till failure parse the file */
    while(bytesread)
    {
        char *temp = NULL;
        char *del_char = NULL;
        iters++;

        bytesread = getline(&readline, &linelen, fp);

        if(bytesread == 0 || bytesread == -1)
        {
            printf("Finished parsing of file\n**********************\n");
            break;
        }

        dictionary = realloc(dictionary, (dict_size + 1) * sizeof(char *));
        temp = malloc((bytesread + 1) * sizeof(char));

        strcpy(temp, readline);

        /* Remove the newline return */
        del_char = strchr(temp, '\n');
        if(del_char)
            *del_char = '\0';

        /* Remove the return character (Some files have it) */
        del_char = strchr(temp, '\r');
        if(del_char)
            *del_char = '\0';

        //    printf("Entry %s\n", temp);

        dictionary[dict_size] = temp;
        dict_size++;
    }

    /* Free the allocated pointers */
    free(readline);
    fclose(fp);

    return 1;
}

/* Utility - Frees memory used to create testcases */
void free_testcase(int flag)
{
    /* Free the main table */
    if(flag)
    {
        for(int i = 0; i < dict_size; i++)
            free(dictionary[i]);
    }

    free(dictionary);

    /* Free the duplicate entry table */
    for(int i = 0; i < dupl_size; i++)
        free(duplicates[i]);

    free(duplicates);
}

int main(int argc, char **argv)
{
    /* PART 1 - Parse the testcases */
    if(parse_testcases(argv[1]) == -1)
        return -1;

    /*************************************************************************************************/

    /* PART 2 - Set the main parameters */
    /* Main parameters */
    node_hashtable_t *hashtable = NULL;
    const int hashtable_size = dict_size;
    const int test_size = dict_size;
    const int search_factor = 20;
    const int delete_factor = 2;

    /* Checker values */
    int err_code;
    int fail_insertions = 0, fail_searches = 0, fail_deletes = 0;

    /* Timers for each part */
    clock_t insert_s, insert_e;
    clock_t search_s, search_e;
    clock_t delete_s, delete_e;

    /*************************************************************************************************/

    /* PART 3 - Create the hashatable and perform insertions */
    /* Time from creation along with rehashing and insertions */
    insert_s = clock();

    hashtable = ht_node_create(hashtable_size, comp, destruct, hash, &err_code);

    for(size_t i = 0; i < test_size; i++)
    {
        test_entry *entry = (test_entry *)malloc(sizeof(test_entry));
        entry->key = dictionary[i];
        entry->num = i;

        if(ht_node_insert(hashtable, entry->key, entry, &err_code))
        {
            fail_insertions++;

            /* Insert it at the duplicate table */
            duplicates = realloc(duplicates, (dupl_size + 1) * sizeof(char *));
            duplicates[dupl_size] = dictionary[i];
            dupl_size++;

            /* Free the entry itself */
            free(entry);
        }
    }

    /*************************************************************************************************/

    /* PART 4 - Create a search sequence and perform a round of searches */
    /* Create a random testcase */
    int *search_idx = malloc(search_factor * test_size * sizeof(int));

    /* Limit the testcase in the range of our test_size */
    for(int i = 0; i < search_factor * test_size; i++)
        search_idx[i] = i % test_size;

    /* Time for searches */
    insert_e = clock() - insert_s;

    /* Time for search_factor * num_of_testcases searches */
    search_s = clock();

    for(size_t i = 0; i < search_factor * test_size; i++)
    {
        if(!ht_node_search(hashtable, dictionary[search_idx[i]]))
            fail_searches++;
    }

    /* Time for searches */
    search_e = clock() - search_s;

    /* Free the random access search idxs */
    free(search_idx);

    /*************************************************************************************************/

    /* PART 5 - Perform a round of deletes */

    /* Time for deletes */
    delete_s = clock();

    /* Delete from start of dictionary - Random access is not permited might cause SEGF */
    for(size_t i = 0; i < test_size / delete_factor; i++)
    {
        if(ht_node_delete(hashtable, dictionary[i]) != 0)
            fail_deletes++;
    }

    /* Time for deletes */
    delete_e = clock() - delete_s;

    /* Print statistics */
    ht_node_print_mem_usage(hashtable);

    printf("******** TIME statistics for {%s} with size {%d} ********\n", argv[1], dict_size - dupl_size);
    printf("Part 3 {#%d Insertions - #%d Insert Fails}: %f\n", dict_size - fail_insertions, fail_insertions, (double)insert_e / CLOCKS_PER_SEC);
    printf("Part 4 {#%d Searches - #%d Search Fails}: %f\n", search_factor * dict_size, fail_searches, (double)search_e / CLOCKS_PER_SEC);
    printf("Part 5 {#%d Deletes - #%d Delete Fails}: %f\n", dict_size / delete_factor, fail_deletes, (double)delete_e / CLOCKS_PER_SEC);

    /* FINAL PART - Free the hashtable and redundant duplicates */
    ht_node_free(hashtable);
    free_testcase(0);

    return 1;
}
