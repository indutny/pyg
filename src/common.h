#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include "src/error.h"

#include "parson.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct pyg_hashmap_s pyg_hashmap_t;
typedef struct pyg_hashmap_item_s pyg_hashmap_item_t;
typedef void (*pyg_hashmap_free_cb)(void*);
typedef pyg_error_t (*pyg_hashmap_iterate_cb)(pyg_hashmap_item_t* item,
                                              void* arg);
typedef pyg_error_t (*pyg_iter_array_cb)(void* val,
                                         size_t i,
                                         size_t count,
                                         void* arg);
typedef void* (*pyg_iter_array_get_cb)(JSON_Array* arr, size_t i);
typedef struct pyg_buf_s pyg_buf_t;

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


struct pyg_buf_s {
  unsigned int off;
  unsigned int size;

  char* buf;
};

pyg_error_t pyg_buf_init(pyg_buf_t* buf, unsigned int size);
void pyg_buf_destroy(pyg_buf_t* buf);
pyg_error_t pyg_buf_put(pyg_buf_t* buf, char* fmt, ...);
void pyg_buf_print(pyg_buf_t* buf, FILE* out);

/* JSON helpers */
pyg_error_t pyg_iter_array(JSON_Array* arr,
                           const char* label,
                           pyg_iter_array_get_cb get,
                           pyg_iter_array_cb cb,
                           void* arg);

char* pyg_dirname(const char* path);
char* pyg_realpath(const char* path);

#define UNREACHABLE() do { abort(); } while (0)

#endif  /* SRC_COMMON_H_ */
