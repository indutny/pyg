#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include "src/error.h"

#include "parson.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef __linux__
# include <linux/limits.h>  /* PATH_MAX */
#else
# include <limits.h>  /* PATH_MAX */
#endif  /* __linux__ */

typedef struct pyg_hashmap_s pyg_hashmap_t;
typedef struct pyg_proto_hashmap_s pyg_proto_hashmap_t;
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
typedef struct pyg_str_s pyg_str_t;
typedef struct pyg_value_s pyg_value_t;

struct pyg_str_s {
  const char* str;
  int len;
};

enum pyg_value_type_e {
  kPygValueStr,
  kPygValueInt,
  kPygValueBool
};
typedef enum pyg_value_type_e pyg_value_type_t;

struct pyg_value_s {
  pyg_value_type_t type;
  union {
    pyg_str_t str;
    int num;
  } value;
};

struct pyg_hashmap_s {
  pyg_hashmap_item_t* space;
  unsigned int size;
};

struct pyg_proto_hashmap_s {
  pyg_hashmap_t map;
  pyg_proto_hashmap_t* parent;
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
void pyg_hashmap_delete(pyg_hashmap_t* hashmap,
                        const char* key,
                        unsigned int key_len);
void* pyg_hashmap_get(pyg_hashmap_t* hashmap,
                      const char* key,
                      unsigned int key_len);
void* pyg_proto_hashmap_get(pyg_proto_hashmap_t* hashmap,
                            const char* key,
                            unsigned int key_len);
pyg_error_t pyg_hashmap_iterate(pyg_hashmap_t* hashmap,
                                pyg_hashmap_iterate_cb cb,
                                void* arg);

#define pyg_hashmap_cinsert(h, k, v)                                          \
    pyg_hashmap_insert((h), (k), strlen((k)), (v))
#define pyg_hashmap_cdelete(h, k) pyg_hashmap_delete((h), (k), strlen((k)))
#define pyg_hashmap_cget(h, k) pyg_hashmap_get((h), (k), strlen((k)))
#define pyg_proto_hashmap_cget(h, k)                                          \
    pyg_proto_hashmap_get((h), (k), strlen((k)))


struct pyg_buf_s {
  unsigned int off;
  unsigned int size;

  char* buf;
};

pyg_error_t pyg_buf_init(pyg_buf_t* buf, unsigned int size);
void pyg_buf_destroy(pyg_buf_t* buf);
pyg_error_t pyg_buf_put(pyg_buf_t* buf, char* fmt, ...);
void pyg_buf_print(pyg_buf_t* buf, FILE* out);

const char* pyg_basename(const char* path);
char* pyg_filename(const char* path);
char* pyg_dirname(const char* path);
char* pyg_realpath(const char* path);
char* pyg_resolve(const char* p1, const char* p2);
char* pyg_nresolve(const char* p1, int len1, const char* p2, int len2);

int pyg_value_to_bool(pyg_value_t* val);
pyg_error_t pyg_value_to_str(pyg_value_t* val, char** out);

#define UNREACHABLE() do { abort(); } while (0)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define container_of(ptr, type, member) \
((type *) ((char *) (ptr) - offsetof(type, member)))

#endif  /* SRC_COMMON_H_ */
