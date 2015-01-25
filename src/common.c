#include "src/common.h"
#include "src/error.h"

#include "parson.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>  /* PATH_MAX */

#define PYG_MURMUR3_C1 0xcc9e2d51
#define PYG_MURMUR3_C2 0x1b873593

enum pyg_merge_mode_e {
  kPygMergeStrict,
  kPygMergeAuto,
  kPygMergeReplace,
  kPygMergeCond,
  kPygMergePrepend,
  kPygMergeExclude
};
typedef enum pyg_merge_mode_e pyg_merge_mode_t;

static pyg_error_t pyg_merge_json_inplace(JSON_Value** to,
                                          JSON_Value* from,
                                          pyg_merge_mode_t mode);
static pyg_error_t pyg_merge_json_obj(JSON_Value** to,
                                      JSON_Value* from,
                                      pyg_merge_mode_t mode);
static pyg_error_t pyg_merge_json_arr(JSON_Value** to,
                                      JSON_Value* from,
                                      pyg_merge_mode_t mode);
static JSON_Value* pyg_merge_json_exclude(JSON_Array* to, JSON_Array* from);
static const char* pyg_merge_classify(const char* name, pyg_merge_mode_t* mode);

#ifdef _MSC_VER
static const char dir_sep = '\\';
#else
static const char dir_sep = '/';
#endif


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


void pyg_hashmap_delete(pyg_hashmap_t* hashmap,
                        const char* key,
                        unsigned int key_len) {
  pyg_hashmap_item_t* item;

  item = pyg_hashmap_get_int(hashmap, key, key_len, 0);
  if (item == NULL)
    return;

  item->key = NULL;
  item->key_len = 0;
  item->value = NULL;
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


pyg_error_t pyg_buf_init(pyg_buf_t* buf, unsigned int size) {
  buf->size = size;
  buf->off = 0;

  /* Trailing zero in snprintf */
  buf->buf = malloc(size);
  if (buf->buf == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_buf_t init");

  return pyg_ok();
}


void pyg_buf_destroy(pyg_buf_t* buf) {
  free(buf->buf);
  buf->buf = NULL;
}


pyg_error_t pyg_buf_put(pyg_buf_t* buf, char* fmt, ...) {
  va_list ap_orig;
  va_list ap;
  int r;

  va_start(ap_orig, fmt);

  do {
    char* tmp;

    /* Copy the vararg to retry writing in case of failure */
    va_copy(ap, ap_orig);

    r = vsnprintf(buf->buf + buf->off, buf->size - buf->off, fmt, ap);
    assert(r >= 0);

    /* Whole string was written */
    if ((unsigned int) r + 1 <= buf->size - buf->off)
      break;

    /* Realloc is needed */
    tmp = malloc(buf->size * 2);
    if (tmp == NULL)
      return pyg_error_str(kPygErrNoMem, "pyg_buf_t buffer");

    memcpy(tmp, buf->buf, buf->size);
    free(buf->buf);
    buf->buf = tmp;
    buf->size = buf->size * 2;

    /* Retry */
    va_end(ap);
  } while (1);

  /* Success */
  va_end(ap);
  va_end(ap_orig);

  buf->off += r;

  return pyg_ok();
}


void pyg_buf_print(pyg_buf_t* buf, FILE* out) {
  fprintf(out, "%.*s", buf->size, buf->buf);
}


pyg_error_t pyg_iter_array(JSON_Array* arr,
                           const char* label,
                           pyg_iter_array_get_cb get,
                           pyg_iter_array_cb cb,
                           void* arg) {
  size_t i;
  size_t count;

  count = json_array_get_count(arr);
  for (i = 0; i < count; i++) {
    void* val;
    pyg_error_t err;

    val = get(arr, i);
    if (val == NULL) {
      static char msg[1024];

      snprintf(msg,
               sizeof(msg),
               "Invalid array item during iteration of `%s`[%d]",
               label,
               (int) i);

      return pyg_error_str(kPygErrJSON, msg);
    }

    err = cb(val, i, count, arg);
    if (!pyg_is_ok(err))
      return err;
  }

  return pyg_ok();
}


char* pyg_dirname(const char* path) {
  const char* c;
  char* res;

  for (c = path + strlen(path); c != path; c--)
    if (*c == dir_sep)
      break;

  if (c == path) {
    res = malloc(2);
    if (res == NULL)
      return NULL;

    // filename => "."
    memcpy(res, ".", 2);
  } else {
    res = malloc(c - path + 1);
    if (res == NULL)
      return NULL;

    memcpy(res, path, c - path);
    res[c - path] = '\0';
  }

  return res;
}


char* pyg_realpath(const char* path) {
  return realpath(path, NULL);
}


char* pyg_resolve(const char* p1, const char* p2) {
  return pyg_nresolve(p1, strlen(p1), p2, strlen(p2));
}


char* pyg_nresolve(const char* p1, int len1, const char* p2, int len2) {
  char buf[PATH_MAX];

  /* Absolute path */
  if (len2 >= 1 && p2[0] == dir_sep)
    return pyg_realpath(p2);

  /* Library :( */
  if (len2 >= 1 && (p2[0] == '-' || p2[0] == '$'))
    return strdup(p2);

  /* Relative path */
  snprintf(buf, sizeof(buf), "%.*s%c%.*s", len1, p1, dir_sep, len2, p2);
  return pyg_realpath(buf);
}


const char* pyg_basename(const char* path) {
  const char* p;

  p = strrchr(path, '/');
  if (p == NULL)
    return path;

  return p + 1;
}


char* pyg_filename(const char* path) {
  const char* base;
  const char* p;
  char* res;

  base = pyg_basename(path);
  p = strrchr(base, '.');
  if (p == NULL)
    return strdup(base);

  res = malloc(p - base + 1);
  if (res == NULL)
    return NULL;

  memcpy(res, base, p - base);
  res[p - base] = '\0';
  return res;
}


pyg_error_t pyg_merge_json_inplace(JSON_Value** to,
                                   JSON_Value* from,
                                   pyg_merge_mode_t mode) {
  /* Copy non-null primitives */
  if (json_value_get_type(*to) != JSONObject &&
      json_value_get_type(*to) != JSONArray &&
      json_value_get_type(from) != JSONNull) {
    *to = from;
    return pyg_ok();
  }

  if (json_value_get_type(*to) != json_value_get_type(from))
    return pyg_ok();

  if (json_value_get_type(from) == JSONObject)
    return pyg_merge_json_obj(to, from, mode);
  else if (json_value_get_type(from) == JSONArray)
    return pyg_merge_json_arr(to, from, mode);

  return pyg_ok();
}


const char* pyg_merge_classify(const char* name, pyg_merge_mode_t* mode) {
  static char buf[1024];
  int len;

  if (*mode == kPygMergeStrict)
    goto skip;

  len = strlen(name);
  if (len < 1) {
    *mode = kPygMergeAuto;
    goto skip;
  }

  switch (name[len - 1]) {
    case '=': *mode = kPygMergeReplace; break;
    case '?': *mode = kPygMergeCond; break;
    case '+': *mode = kPygMergePrepend; break;
    case '!': *mode = kPygMergeExclude; break;
    default: *mode = kPygMergeAuto; goto skip;
  }

  /* TODO(indutny): error on overflow */
  snprintf(buf, sizeof(buf), "%.*s", len - 1, name);
  return buf;

skip:
  return name;
}


pyg_error_t pyg_merge_json_obj(JSON_Value** to,
                               JSON_Value* from,
                               pyg_merge_mode_t mode) {
  size_t i;
  size_t count;
  JSON_Status st;
  pyg_error_t err;
  JSON_Object* from_obj;
  JSON_Object* to_obj;

  from_obj = json_value_get_object(from);
  to_obj = json_value_get_object(*to);

  count = json_object_get_count(from_obj);
  for (i = 0; i < count; i++) {
    const char* name;
    JSON_Value* from_value;
    JSON_Value* to_value;

    name = json_object_get_name(from_obj, i);
    name = pyg_merge_classify(name, &mode);

    from_value = json_object_get_value(from_obj, name);
    to_value = json_object_get_value(to_obj, name);

    /* New property */
    if (to_value == NULL) {
      st = json_object_set_value(to_obj, name, from_value);
      if (st != JSONSuccess)
        return pyg_error_str(kPygErrNoMem, "Failed to merge JSON (%s)", name);

      continue;
    }

    err = pyg_merge_json_inplace(&to_value, from_value, mode);
    if (!pyg_is_ok(err))
      return err;

    st = json_object_set_value(to_obj, name, to_value);
    if (st != JSONSuccess)
      return pyg_error_str(kPygErrNoMem, "Failed to merge JSON (%s)", name);
  }

  return pyg_ok();
}


pyg_error_t pyg_merge_json_arr(JSON_Value** to,
                               JSON_Value* from,
                               pyg_merge_mode_t mode) {
  size_t i;
  size_t count;
  JSON_Status st;
  JSON_Array* from_arr;
  JSON_Array* to_arr;

  if (mode == kPygMergeReplace) {
    *to = json_value_deep_copy(from);
    if (*to == NULL)
      return pyg_error_str(kPygErrNoMem, "Failed to deep copy array");
    return pyg_ok();
  }

  /* `to` is non-empty, conditiona failed anyway */
  if (mode == kPygMergeCond)
    return pyg_ok();

  from_arr = json_value_get_array(from);
  to_arr = json_value_get_array(*to);

  if (mode == kPygMergeExclude) {
    *to = pyg_merge_json_exclude(to_arr, from_arr);
    if (*to == NULL)
      return pyg_error_str(kPygErrNoMem, "Failed to exclude copy array");
    return pyg_ok();
  }

  /* Swap from and to */
  if (mode == kPygMergePrepend) {
    JSON_Value* tmp;

    tmp = json_value_deep_copy(from);
    if (tmp == NULL)
      return pyg_error_str(kPygErrNoMem, "Failed to deep copy array");

    from = *to;
    *to = tmp;

    from_arr = json_value_get_array(from);
    to_arr = json_value_get_array(*to);
  }

  count = json_array_get_count(from_arr);
  for (i = 0; i < count; i++) {
    JSON_Value* from_value;

    from_value = json_array_get_value(from_arr, i);

    /* TODO(indutny): de-duplicate? */
    st = json_array_append_value(to_arr, from_value);
    if (st != JSONSuccess)
      return pyg_error_str(kPygErrNoMem, "Failed to merge JSON (%d)", (int) i);
  }

  return pyg_ok();
}


JSON_Value* pyg_merge_json_exclude(JSON_Array* to, JSON_Array* from) {
  size_t i;
  size_t j;
  size_t from_count;
  size_t to_count;
  JSON_Value* dest;

  dest = json_value_init_array();
  if (dest == NULL)
    return NULL;

  to_count = json_array_get_count(to);
  from_count = json_array_get_count(from);
  for (i = 0; i < from_count; i++) {
    JSON_Status st;
    const char* val;

    val = json_array_get_string(from, i);
    if (val == NULL)
      continue;
    for (j = 0; j < to_count; j++) {
      const char* other;

      other = json_array_get_string(to, i);
      if (other == NULL)
        continue;

      if (strcmp(other, val) == 0)
        break;
    }

    /* Match found - skip */
    if (j != to_count)
      continue;

    st = json_array_append_string(json_value_get_array(dest), val);
    if (st != JSONSuccess) {
      json_value_free(dest);
      return NULL;
    }
  }

  return dest;
}


pyg_error_t pyg_merge_json(JSON_Value* to, JSON_Value* from, int strict) {
  return pyg_merge_json_inplace(&to,
                                from,
                                strict ? kPygMergeStrict : kPygMergeAuto);
}


pyg_error_t pyg_eval_str(const char* str, pyg_hashmap_t* vars) {
  return pyg_ok();
}
