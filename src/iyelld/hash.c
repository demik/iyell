/* modified by Michel Depeige for use in iyell */
/* Copyright 2006 David Crawshaw, released under the new BSD license.
 * Version 2, from http://www.zentus.com/c/hash.html */

#include <stdlib.h>
#include <string.h>
#include "hash.h"

/* Table is sized by primes to minimise clustering.
   See: http://planetmath.org/encyclopedia/GoodHashTablePrimes.html */
static const unsigned int sizes[] = {
    53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
};
static const int sizes_count = sizeof(sizes) / sizeof(sizes[0]);
static const float load_factor = 0.65;

struct record_s {
    unsigned int hash;
    const char *key;
    void *value;
};

struct hash_s {
    struct record_s *records;
    unsigned int records_count;
    unsigned int size_index;
};

static int hash_grow(hash_t *h)
{
    int i;
    struct record_s *old_recs;
    unsigned int old_recs_length;

    old_recs_length = sizes[h->size_index];
    old_recs = h->records;

    if (h->size_index == (unsigned int)sizes_count - 1) return -1;
    if ((h->records = calloc(sizes[++h->size_index],
                    sizeof(struct record_s))) == NULL) {
        h->records = old_recs;
        return -1;
    }

    h->records_count = 0;

    // rehash table
    for (i=0; i < (int)old_recs_length; i++)
        if (old_recs[i].hash && old_recs[i].key)
            hash_add(h, old_recs[i].key, old_recs[i].value);

    free(old_recs);

    return 0;
}

/* algorithm djb2 */
static unsigned int strhash(const char *str)
{
    int c;
    int hash = 5381;
    while ((c = *str++))
        hash = hash * 33 + c;
    return hash == 0 ? 1 : hash;
}


hash_t * hash_new(unsigned int capacity) {
    struct hash_s *h;
    int i, sind = 0;

    capacity /= load_factor;

    for (i=0; i < sizes_count; i++)
        if (sizes[i] > capacity) { sind = i; break; }

    if ((h = malloc(sizeof(struct hash_s))) == NULL) return NULL;
    if ((h->records = calloc(sizes[sind], sizeof(struct record_s))) == NULL) {
        free(h);
        return NULL;
    }

    h->records_count = 0;
    h->size_index = sind;

    return h;
}

void hash_destroy(hash_t *h)
{
    free(h->records);
    free(h);
}

int hash_add(hash_t *h, const char *key, void *value)
{
    struct record_s *recs;
    int rc;
    unsigned int off, ind, size, code;

    if (key == NULL || *key == '\0') return -2;
    if (h->records_count > sizes[h->size_index] * load_factor) {
        rc = hash_grow(h);
        if (rc) return rc;
    }

    code = strhash(key);
    recs = h->records;
    size = sizes[h->size_index];

    ind = code % size;
    off = 0;

    while (recs[ind].key) {
        off++;
	ind = (code + (off * off)) % size;
    }

    recs[ind].hash = code;
    recs[ind].key = key;
    recs[ind].value = value;

    h->records_count++;

    return 0;
}

void * hash_get(hash_t *h, const char *key)
{
    struct record_s *recs;
    unsigned int off, ind, size;
    unsigned int code = strhash(key);

    recs = h->records;
    size = sizes[h->size_index];
    ind = code % size;
    off = 0;

    // search on hash which remains even if a record has been removed,
    // so hash_remove() does not need to move any collision records
    while (recs[ind].hash) {
        if ((code == recs[ind].hash) && recs[ind].key &&
                strcmp(key, recs[ind].key) == 0)
            return recs[ind].value;
        off++;
	ind = (code + (off * off)) % size;
    }

    return NULL;
}

void * hash_remove(hash_t *h, const char *key)
{
    unsigned int code = strhash(key);
    struct record_s *recs;
    void * value;
    unsigned int off, ind, size;

    recs = h->records;
    size = sizes[h->size_index];
    ind = code % size;
    off = 0;

    while (recs[ind].hash) {
        if ((code == recs[ind].hash) && recs[ind].key &&
                strcmp(key, recs[ind].key) == 0) {
            // do not erase hash, so probes for collisions succeed
            value = recs[ind].value;
            recs[ind].key = 0;
            recs[ind].value = 0;
            h->records_count--;
            return value;
        }
        off++;
	ind = (code + (off * off)) % size;
    }

    return NULL;
}

unsigned int hash_size(hash_t *h)
{
    return h->records_count;
}
