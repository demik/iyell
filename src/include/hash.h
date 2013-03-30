/* Copyright 2006 David Crawshaw, released under the new BSD license.
 * Version 2, from http://www.zentus.com/c/hash.html */

#ifndef __HASH__
#define __HASH__

/* Opaque structure used to represent hashtable. */
typedef struct hash_s hash_t;

/* Create new hashtable. */
hash_t * hash_new(unsigned int size);

/* Free hashtable. */
void hash_destroy(hash_t *h);

/* Add key/value pair. Returns non-zero value on error (eg out-of-memory). */
int hash_add(hash_t *h, const char *key, void *value);

/* Return value matching given key. */
void * hash_get(hash_t *h, const char *key);

/* Remove key from table, returning value. */
void * hash_remove(hash_t *h, const char *key);

/* Returns total number of keys in the hashtable. */
unsigned int hash_size(hash_t *h);

#endif
