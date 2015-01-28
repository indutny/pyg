#include "src/json.h"
#include "src/common.h"
#include "src/unroll.h"

#include "parson.h"

#include <string.h>

static pyg_error_t pyg_merge_json_inplace(JSON_Value** to,
                                          JSON_Value* from,
                                          pyg_merge_mode_t mode);
static pyg_error_t pyg_merge_json_arr(JSON_Value** to,
                                      JSON_Value* from,
                                      pyg_merge_mode_t mode);
static JSON_Value* pyg_merge_json_exclude(JSON_Array* to, JSON_Array* from);
static const char* pyg_merge_classify(const char* name, pyg_merge_mode_t* mode);


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


pyg_error_t pyg_merge_json_inplace(JSON_Value** to,
                                   JSON_Value* from,
                                   pyg_merge_mode_t mode) {
  /* Copy non-null primitives */
  if (json_value_get_type(*to) != JSONObject &&
      json_value_get_type(*to) != JSONArray &&
      json_value_get_type(from) != JSONNull) {
    JSON_Value* tmp;
    tmp = json_value_deep_copy(from);
    if (tmp == NULL)
      return pyg_error_str(kPygErrNoMem, "json_value_deep_copy");
    *to = tmp;
    return pyg_ok();
  }

  if (json_value_get_type(*to) != json_value_get_type(from))
    return pyg_ok();

  if (json_value_get_type(from) == JSONObject)
    return pyg_merge_json_obj(json_object(*to), json_object(from), mode);
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


pyg_error_t pyg_merge_json_obj(JSON_Object* to,
                               JSON_Object* from,
                               pyg_merge_mode_t mode) {
  size_t i;
  size_t count;
  JSON_Status st;
  pyg_error_t err;

  count = json_object_get_count(from);
  for (i = 0; i < count; i++) {
    const char* name;
    JSON_Value* from_value;
    JSON_Value* to_value;
    JSON_Value* new_to_value;

    name = json_object_get_name(from, i);
    from_value = json_object_get_value(from, name);

    to_value = json_object_get_value(to, pyg_merge_classify(name, &mode));

    /* New property */
    if (to_value == NULL) {
      err = pyg_clone_json(from_value, mode, &from_value);
      if (!pyg_is_ok(err))
        return err;

      st = json_object_set_value(to,
                                 pyg_merge_classify(name, &mode),
                                 from_value);
      if (st != JSONSuccess) {
        json_value_free(from_value);
        return pyg_error_str(kPygErrNoMem, "Failed to merge JSON (%s)", name);
      }

      continue;
    }

    new_to_value = to_value;
    err = pyg_merge_json_inplace(&new_to_value, from_value, mode);
    if (!pyg_is_ok(err))
      return err;

    if (new_to_value == to_value)
      continue;

    st = json_object_set_value(to, name, to_value);
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

  /* `to` is non-empty, conditional failed anyway */
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
    pyg_error_t err;

    err = pyg_clone_json(json_array_get_value(from_arr, i),
                         mode,
                         &from_value);
    if (!pyg_is_ok(err)) {
      if (mode == kPygMergePrepend)
        json_value_free(*to);
      return err;
    }

    /* TODO(indutny): de-duplicate? */
    st = json_array_append_value(to_arr, from_value);
    if (st != JSONSuccess) {
      if (mode == kPygMergePrepend)
        json_value_free(*to);
      return pyg_error_str(kPygErrNoMem, "Failed to merge JSON (%d)", (int) i);
    }
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


pyg_error_t pyg_clone_json(JSON_Value* value,
                           pyg_merge_mode_t mode,
                           JSON_Value** out) {
  pyg_error_t err;
  JSON_Value_Type type;
  JSON_Value* res;

  if (mode == kPygMergeStrict) {
    res = json_value_deep_copy(value);
    goto done;
  }

  /* Primitive - non clonable */
  type = json_value_get_type(value);
  if ((type != JSONObject && type != JSONArray) ||
      (type == JSONArray && mode == kPygMergeCond)) {
    res = json_value_deep_copy(value);
    goto done;
  }

  if (type == JSONObject)
    res = json_value_init_object();
  else
    res = json_value_init_array();
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "json_value_init_object/array()");

  err = pyg_merge_json(res, value, mode);
  if (!pyg_is_ok(err)) {
    json_value_free(res);
    return err;
  }

done:
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "Failed to allocate clone result");

  *out = res;
  return pyg_ok();
}


pyg_error_t pyg_merge_json(JSON_Value* to,
                           JSON_Value* from,
                           pyg_merge_mode_t mode) {
  return pyg_merge_json_inplace(&to, from, mode);
}


pyg_error_t pyg_unroll_json(pyg_proto_hashmap_t* vars, JSON_Value** out) {
  pyg_error_t err;
  JSON_Value* value;

  value = *out;
  if (json_value_get_type(value) == JSONString) {
    const char* str;
    char* estr;

    str = json_string(value);
    err = pyg_unroll_str(vars, str, &estr);
    if (!pyg_is_ok(err))
      return err;

    *out = json_value_init_string(estr);
    free(estr);

    if (*out == NULL)
      return pyg_error_str(kPygErrNoMem, "failed to alloc string");
  } else if (json_value_get_type(value) == JSONString) {
    size_t i;
    size_t count;
    JSON_Array* arr;

    arr = json_value_get_array(value);
    count = json_array_get_count(arr);
    for (i = 0; i < count; i++) {
      JSON_Value* sub;
      JSON_Value* new_sub;

      sub = json_array_get_value(arr, i);
      new_sub = sub;
      pyg_unroll_json(vars, &new_sub);
      if (sub == new_sub)
        continue;

      json_array_replace_value(arr, i, new_sub);
    }
  }

  return pyg_ok();
}


pyg_error_t pyg_unroll_json_key(pyg_proto_hashmap_t* vars,
                                JSON_Object* json,
                                const char* key) {
  pyg_error_t err;
  JSON_Value* value;
  JSON_Value* new_value;

  value = json_object_get_value(json, key);
  if (value == NULL)
    return pyg_ok();

  new_value = value;
  err = pyg_unroll_json(vars, &new_value);
  if (!pyg_is_ok(err))
    return err;

  if (new_value != value)
    json_object_set_value(json, key, new_value);

  return pyg_ok();
}
