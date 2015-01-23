#include "src/pyg.h"
#include "src/common.h"
#include "src/generator/base.h"

#include "parson.h"

#include <stdlib.h>
#include <string.h>


static const char* kPygDefaultTargetType = "executable";
static const unsigned kPygChildrenCount = 16;
static const unsigned kPygTargetCount = 16;


static pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out);
static pyg_error_t pyg_free_child(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_free_target(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_load_target(void* val,
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
  res->root = parent == NULL ? res : parent;

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

  if (res->parent == NULL) {
    err = pyg_hashmap_init(&res->children, kPygChildrenCount);
    if (!pyg_is_ok(err))
      goto failed_children_init;
  }

  err = pyg_hashmap_insert(&res->root->children,
                           res->path,
                           strlen(res->path),
                           res);
  if (!pyg_is_ok(err))
    goto failed_children_insert;

  err = pyg_hashmap_init(&res->targets, kPygTargetCount);
  if (!pyg_is_ok(err))
    goto failed_target_init;

  *out = res;
  return pyg_ok();

failed_target_init:
  pyg_hashmap_delete(&res->root->children, res->path, strlen(res->path));

failed_children_insert:
  if (res->parent == NULL)
    pyg_hashmap_destroy(&res->children);

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

  if (pyg->parent == NULL) {
    pyg_hashmap_iterate(&pyg->children, pyg_free_child, pyg);
    pyg_hashmap_destroy(&pyg->children);
  }
  pyg_hashmap_iterate(&pyg->targets, pyg_free_target, NULL);
  pyg_hashmap_destroy(&pyg->targets);

  json_value_free(pyg->json);
  pyg->json = NULL;
  pyg->obj = NULL;

  free(pyg);
}


pyg_error_t pyg_free_child(pyg_hashmap_item_t* item, void* arg) {
  /* Do not free self-reference */
  if (arg != item->value)
    pyg_free(item->value);

  return pyg_ok();
}


pyg_error_t pyg_free_target(pyg_hashmap_item_t* item, void* arg) {
  pyg_target_t* target;

  target = item->value;
  free(target);

  return pyg_ok();
}


pyg_error_t pyg_load(pyg_t* pyg) {
  pyg_error_t err;
  JSON_Array* targets;

  /* TODO(indutny): Support variables and target_defaults */

  /* Visit targets */
  targets = json_object_get_array(pyg->obj, "targets");
  if (targets == NULL)
    return pyg_error_str(kPygErrJSON, "'targets' property not found");

  err = pyg_iter_array(targets,
                       "targets",
                       (pyg_iter_array_get_cb) json_array_get_object,
                       pyg_load_target,
                       pyg);
  if (!pyg_is_ok(err))
    return err;

  return pyg_ok();
}


pyg_error_t pyg_load_target(void* val, size_t i, size_t count, void* arg) {
  JSON_Object* obj;
  const char* name;
  pyg_t* pyg;
  pyg_target_t* target;
  pyg_error_t err;

  obj = val;
  pyg = arg;

  target = calloc(1, sizeof(*target));
  if (target == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_target_t");

  name = json_object_get_string(obj, "target_name");
  if (name == NULL) {
    err = pyg_error_str(kPygErrJSON, "'target_name' not string");
    goto failed_target_name;
  }

  target->name = name;
  target->type = json_object_get_string(obj, "type");
  if (target->type == NULL)
    target->type = kPygDefaultTargetType;

  err = pyg_hashmap_insert(&pyg->targets, name, strlen(name), target);
  if (!pyg_is_ok(err))
    goto failed_target_name;

  return pyg_ok();

failed_target_name:
  free(target);
  return err;
}


pyg_error_t pyg_translate(pyg_t* pyg, pyg_gen_t* gen, pyg_buf_t* buf) {
  return pyg_ok();
}
