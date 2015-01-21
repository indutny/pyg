#include "src/common.h"
#include "src/error.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PYG_MURMUR3_C1 0xcc9e2d51
#define PYG_MURMUR3_C2 0x1b873593


static uint32_t pyg_murmur3(const char* key, uint32_t len) {
  uint32_t hash;
  const uint32_t* chunks;
  int chunk_count;
  int i;
  uint32_t tail;

  hash = 0;

  /* FIXME(indutny): this leads to unaligned loads for some keys */
  chunks = (const uint32_t*) key;
  chunk_count = len / 4;
  for (i = 0; i < chunk_count; i++) {
    uint32_t k;

    k = chunks[i];
    k *= PYG_MURMUR3_C1;
    k = (k << 15) | (k >> 17);
    k *= PYG_MURMUR3_C2;

    hash ^= k;
    hash = (hash << 13) | (hash >> 19);
    hash *= 5;
    hash += 0xe6546b64;
  }

  tail = 0;
  chunk_count *= 4;
  for (i = len - 1; i >= chunk_count; i--) {
    tail <<= 8;
    tail += key[i];
  }
  if (tail != 0) {
    tail *= PYG_MURMUR3_C1;
    tail = (tail << 15) | (tail >> 17);
    tail *= PYG_MURMUR3_C2;

    hash ^= tail;
  }

  hash ^= len;

  hash ^= hash >> 16;
  hash *= 0x85ebca6b;
  hash ^= hash >> 13;
  hash *= 0xc2b2ae35;
  hash ^= hash >> 16;

  return hash;
}


#undef PYG_MURMUR3_C1
#undef PYG_MURMUR3_C2


pyg_error_t pyg_hashmap_init(pyg_hashmap_t* hashmap, unsigned int size) {
  hashmap->size = size;
  hashmap->space = calloc(size, sizeof(*hashmap->space));
  if (hashmap->space == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_hashmap_item_t");

  return pyg_ok();
}


void pyg_hashmap_destroy(pyg_hashmap_t* hashmap) {
  if (hashmap->space == NULL)
    return;

  free(hashmap->space);
  hashmap->space = NULL;
}


/* A bit sparse, but should be fast */
#define PYG_HASHMAP_MAX_ITER 3
#define PYG_HASHMAP_GROW_DELTA 1024


static pyg_hashmap_item_t* pyg_hashmap_get_int(pyg_hashmap_t* hashmap,
                                               const char* key,
                                               unsigned int key_len,
                                               int insert) {
  do {
    uint32_t i;
    uint32_t iter;
    pyg_hashmap_item_t* space;
    unsigned int size;
    pyg_hashmap_t old_map;

    i = pyg_murmur3(key, key_len) % hashmap->size;
    for (iter = 0;
         iter < PYG_HASHMAP_MAX_ITER;
         iter++, i = (i + 1) % hashmap->size) {
      if (hashmap->space[i].key == NULL)
        break;
      if (!insert) {
        if (hashmap->space[i].key_len == key_len &&
            memcmp(hashmap->space[i].key, key, key_len) == 0) {
          break;
        }
      }
    }

    if (!insert && hashmap->space[i].key == NULL)
      return NULL;

    /* Found a spot */
    if (iter != PYG_HASHMAP_MAX_ITER)
      return &hashmap->space[i];

    /* No match */
    if (!insert)
      return NULL;

    /* Grow and retry */
    size = hashmap->size += PYG_HASHMAP_GROW_DELTA;
    space = calloc(size, sizeof(*space));
    if (space == NULL)
      return NULL;

    /* Rehash */
    old_map = *hashmap;
    hashmap->space = space;
    hashmap->size = size;
    for (i = 0; i < old_map.size; i++) {
      pyg_hashmap_item_t* item;
      pyg_error_t err;

      item = &old_map.space[i];
      err = pyg_hashmap_insert(hashmap, item->key, item->key_len, item->value);
      if (!pyg_is_ok(err)) {
        free(space);
        *hashmap = old_map;
        return NULL;
      }
    }

  /* Retry */
  } while (1);
}


#undef PYG_HASHMAP_GROW_DELTA
#undef PYG_HASHMAP_MAX_ITER


pyg_error_t pyg_hashmap_insert(pyg_hashmap_t* hashmap,
                               const char* key,
                               unsigned int key_len,
                               void* value) {
  pyg_hashmap_item_t* item;

  item = pyg_hashmap_get_int(hashmap, key, key_len, 1);
  if (item == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_hashmap_t space");

  item->key = key;
  item->key_len = key_len;
  item->value = value;

  return pyg_ok();
}


void* pyg_hashmap_get(pyg_hashmap_t* hashmap,
                      const char* key,
                      unsigned int key_len) {
  pyg_hashmap_item_t* item;

  item = pyg_hashmap_get_int(hashmap, key, key_len, 0);
  if (item == NULL)
    return NULL;

  return item->value;
}


pyg_error_t pyg_hashmap_iterate(pyg_hashmap_t* hashmap,
                                pyg_hashmap_iterate_cb cb,
                                void* arg) {
  pyg_error_t err;
  unsigned int i;

  if (hashmap->space == NULL)
    return pyg_ok();

  for (i = 0; i < hashmap->size; i++) {
    if (hashmap->space[i].key != NULL) {
      err = cb(&hashmap->space[i], arg);
      if (!pyg_is_ok(err))
        return err;
    }
  }

  return pyg_ok();
}
