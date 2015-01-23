#include "src/pyg.h"
#include "src/common.h"
#include "src/generator/base.h"

#include "parson.h"

#include <stdlib.h>
#include <string.h>


static const char* kPygDefaultTargetType = "executable";
static const unsigned kPygChildrenCount = 128;


static pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out);
static pyg_error_t pyg_free_child(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_translate_target(void* val,
                                        size_t i,
                                        size_t count,
                                        void* arg);

pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out) {
  static char msg[1024];

  pyg_error_t err;
  pyg_t* res;

  res = calloc(1, sizeof(*res));
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_t");

  res->id = 0;
  res->child_count = 0;
  res->parent = parent;

  if (parent != NULL)
    res->id = ++parent->child_count;

  res->json = json_parse_file_with_comments(path);
  if (res->json == NULL) {
    snprintf(msg, sizeof(msg), "Failed to parse JSON in file: %s", path);
    err = pyg_error_str(kPygErrJSON, msg);
    goto failed_parse_file;
  }

  res->obj = json_object(res->json);
  if (res->obj == NULL) {
    snprintf(msg, sizeof(msg), "JSON not object: %s", path);
    err = pyg_error_str(kPygErrJSON, msg);
    goto failed_to_object;
  }

  res->path = pyg_realpath(path);
  if (res->path == NULL) {
    err = pyg_error_str(kPygErrNoMem, "pyg_realpath");
    goto failed_to_object;
  }

  res->dir = pyg_dirname(res->path);
  if (res->dir == NULL) {
    err = pyg_error_str(kPygErrNoMem, "pyg_dirname");
    goto failed_dirname;
  }

  if (res->id == 0) {
    err = pyg_hashmap_init(&res->children, kPygChildrenCount);
  } else {
    err = pyg_hashmap_insert(&parent->children,
                             res->path,
                             strlen(res->path),
                             res);
  }
  if (!pyg_is_ok(err))
    goto failed_children_init;

  *out = res;
  return pyg_ok();

failed_children_init:
  free(res->dir);
  res->dir = NULL;

failed_dirname:
  free(res->path);
  res->path = NULL;

failed_to_object:
  json_value_free(res->json);
  res->json = NULL;

failed_parse_file:
  free(res);
  return err;
}


pyg_error_t pyg_new(const char* path, pyg_t** out) {
  return pyg_new_child(path, NULL, out);
}


void pyg_free(pyg_t* pyg) {
  free(pyg->dir);
  pyg->dir = NULL;

  if (pyg->id == 0) {
    pyg_hashmap_iterate(&pyg->children, pyg_free_child, NULL);
    pyg_hashmap_destroy(&pyg->children);
  }

  json_value_free(pyg->json);
  pyg->json = NULL;
  pyg->obj = NULL;

  free(pyg);
}


pyg_error_t pyg_free_child(pyg_hashmap_item_t* item, void* arg) {
  pyg_free(item->value);

  return pyg_ok();
}


pyg_error_t pyg_translate(pyg_t* pyg, pyg_gen_t* gen, pyg_buf_t* buf) {
  pyg_state_t st;
  pyg_error_t err;
  JSON_Array* targets;

  /* Initialize generator state */
  st.pyg = pyg;
  st.gen = gen;
  st.buf = buf;

  /* TODO(indutny): Support variables and target_defaults */

  /* Visit targets */
  targets = json_object_get_array(pyg->obj, "targets");
  if (targets == NULL)
    return pyg_error_str(kPygErrJSON, "'targets' property not found");

  err = pyg_iter_array(targets,
                       "targets",
                       (pyg_iter_array_get_cb) json_array_get_object,
                       pyg_translate_target,
                       &st);
  if (!pyg_is_ok(err))
    return err;

  return pyg_ok();
}


pyg_error_t pyg_translate_target(void* val, size_t i, size_t count, void* arg) {
  JSON_Object* obj;
  const char* name;
  pyg_state_t* st;
  pyg_target_t target;

  obj = val;
  st = arg;

  name = json_object_get_string(obj, "target_name");
  if (name == NULL)
    return pyg_error_str(kPygErrJSON, "'target_name' not string");

  target.name = name;
  target.type = json_object_get_string(obj, "type");
  if (target.type == NULL)
    target.type = kPygDefaultTargetType;
  st->gen->target_cb(st, &target);

  return pyg_ok();
}
