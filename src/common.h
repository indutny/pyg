#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include "src/error.h"

#include <stdlib.h>

typedef struct pyg_hashmap_s pyg_hashmap_t;
typedef struct pyg_hashmap_item_s pyg_hashmap_item_t;
typedef void (*pyg_hashmap_free_cb)(void*);
typedef pyg_error_t (*pyg_hashmap_iterate_cb)(pyg_hashmap_item_t* item,
                                              void* arg);

struct pyg_hashmap_s {
  pyg_hashmap_item_t* space;
  unsigned int size;
};

struct pyg_hashmap_item_s {
  const char* key;
  unsigned int key_len;
  void* value;
};

pyg_error_t pyg_hashmap_init(pyg_hashmap_t* hashmap, unsigned int size);
void pyg_hashmap_destroy(pyg_hashmap_t* hashmap);

pyg_error_t pyg_hashmap_insert(pyg_hashmap_t* hashmap,
                               const char* key,
                               unsigned int key_len,
                               void* value);
void* pyg_hashmap_get(pyg_hashmap_t* hashmap,
                      const char* key,
                      unsigned int key_len);
pyg_error_t pyg_hashmap_iterate(pyg_hashmap_t* hashmap,
                                pyg_hashmap_iterate_cb cb,
                                void* arg);

#define UNREACHABLE() do { abort(); } while (0)

#endif  /* SRC_COMMON_H_ */
