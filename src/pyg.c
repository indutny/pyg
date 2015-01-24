#include "src/pyg.h"
#include "src/common.h"
#include "src/generator/base.h"
#include "src/queue.h"

#include "parson.h"

#include <stdlib.h>
#include <string.h>


static const unsigned kPygChildrenCount = 16;
static const unsigned kPygTargetCount = 16;


static pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out);
static pyg_error_t pyg_free_child(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_free_target(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_load(pyg_t* pyg);
static pyg_error_t pyg_load_target(void* val,
                                   size_t i,
                                   size_t count,
                                   void* arg);
static pyg_error_t pyg_load_target_deps(pyg_target_t* target);
static pyg_error_t pyg_load_target_dep(void* val,
                                       size_t i,
                                       size_t count,
                                       void* arg);
static pyg_error_t pyg_resolve_json(pyg_t* pyg,
                                    JSON_Object* json,
                                    const char* key);
static pyg_error_t pyg_target_type_from_str(const char* type,
                                            pyg_target_type_t* out);

pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out) {
  pyg_error_t err;
  pyg_t* res;
  char* rpath;

  rpath = pyg_realpath(path);
  if (rpath == NULL)
    return pyg_error_str(kPygErrFS, "pyg_realpath");

  /* Try looking up the path */
  if (parent != NULL) {
    pyg_t* existing;

    existing = pyg_hashmap_cget(&parent->root->children.map, rpath);

    /* Child found! */
    if (existing != NULL) {
      *out = existing;
      return pyg_ok();
    }
  }

  res = calloc(1, sizeof(*res));
  if (res == NULL) {
    err = pyg_error_str(kPygErrNoMem, "pyg_t");
    goto failed_calloc;
  }

  res->id = 0;
  res->child_count = 0;
  res->parent = parent;
  res->root = res->parent == NULL ? res : res->parent->root;

  if (parent != NULL)
    res->id = ++parent->child_count;

  res->json = json_parse_file_with_comments(path);
  if (res->json == NULL) {
    err = pyg_error_str(kPygErrJSON, "Failed to parse JSON in file: %s", path);
    goto failed_parse_file;
  }

  res->obj = json_object(res->json);
  if (res->obj == NULL) {
    err = pyg_error_str(kPygErrJSON, "JSON not object: %s", path);
    goto failed_to_object;
  }

  res->path = rpath;
  res->dir = pyg_dirname(res->path);
  if (res->dir == NULL) {
    err = pyg_error_str(kPygErrFS, "pyg_dirname");
    goto failed_to_object;
  }

  QUEUE_INIT(&res->member);

  if (res->parent == NULL) {
    QUEUE_INIT(&res->children.list);

    err = pyg_hashmap_init(&res->children.map, kPygChildrenCount);
    if (!pyg_is_ok(err))
      goto failed_children_init;
  }

  /* For easier iteration - push self to the list anyway */
  QUEUE_INSERT_TAIL(&res->root->children.list, &res->member);

  err = pyg_hashmap_cinsert(&res->root->children.map, res->path, res);
  if (!pyg_is_ok(err))
    goto failed_children_insert;

  err = pyg_hashmap_init(&res->target.map, kPygTargetCount);
  if (!pyg_is_ok(err))
    goto failed_target_init;

  QUEUE_INIT(&res->target.list);

  err = pyg_load(res);
  if (!pyg_is_ok(err)) {
    pyg_free(res);
    return err;
  }

  *out = res;
  return pyg_ok();

failed_target_init:
  pyg_hashmap_cdelete(&res->root->children.map, res->path);

failed_children_insert:
  if (res->parent == NULL)
    pyg_hashmap_destroy(&res->children.map);

failed_children_init:
  free(res->dir);
  res->dir = NULL;

failed_to_object:
  json_value_free(res->json);
  res->json = NULL;

failed_parse_file:
  free(res);

failed_calloc:
  free(rpath);
  return err;
}


pyg_error_t pyg_new(const char* path, pyg_t** out) {
  return pyg_new_child(path, NULL, out);
}


void pyg_free(pyg_t* pyg) {
  free(pyg->dir);
  pyg->dir = NULL;

  if (pyg->parent == NULL) {
    pyg_hashmap_iterate(&pyg->children.map, pyg_free_child, pyg);
    pyg_hashmap_destroy(&pyg->children.map);
  }
  pyg_hashmap_iterate(&pyg->target.map, pyg_free_target, NULL);
  pyg_hashmap_destroy(&pyg->target.map);

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
  QUEUE_REMOVE(&target->member);
  free(target);

  return pyg_ok();
}


pyg_error_t pyg_load(pyg_t* pyg) {
  pyg_error_t err;
  JSON_Array* targets;
  QUEUE* q;

  /* TODO(indutny): Support variables and target_defaults */

  /* Visit targets */
  targets = json_object_get_array(pyg->obj, "targets");
  if (targets == NULL)
    return pyg_error_str(kPygErrJSON, "'targets' property not found");

  /* Load local targets */
  err = pyg_iter_array(targets,
                       "targets",
                       (pyg_iter_array_get_cb) json_array_get_object,
                       pyg_load_target,
                       pyg);
  if (!pyg_is_ok(err))
    return err;

  /* Load their deps */
  QUEUE_FOREACH(q, &pyg->target.list) {
    pyg_target_t* target;

    target = container_of(q, pyg_target_t, member);
    err = pyg_load_target_deps(target);
    if (!pyg_is_ok(err))
      return err;

    /* Resolve various path arrays in JSON */
    err = pyg_resolve_json(pyg, target->json, "sources");
    if (pyg_is_ok(err))
      err = pyg_resolve_json(pyg, target->json, "include_dirs");
    if (!pyg_is_ok(err))
      return err;
  }

  return pyg_ok();
}


pyg_error_t pyg_load_target(void* val, size_t i, size_t count, void* arg) {
  JSON_Object* obj;
  const char* name;
  const char* type;
  pyg_t* pyg;
  pyg_target_t* target;
  pyg_error_t err;

  obj = val;
  pyg = arg;

  target = calloc(1, sizeof(*target));
  if (target == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_target_t");

  target->pyg = pyg;
  target->json = obj;
  QUEUE_INIT(&target->member);

  /* Allocate space for dependencies */
  target->deps.count = json_array_get_count(json_object_get_array(obj, "deps"));
  target->deps.list = calloc(target->deps.count, sizeof(*target->deps.list));
  if (target->deps.list == NULL) {
    err = pyg_error_str(kPygErrNoMem, "pyg_target_t.deps");
    goto failed_alloc_deps;
  }

  /* Load name/type */
  name = json_object_get_string(obj, "target_name");
  if (name == NULL) {
    err = pyg_error_str(kPygErrJSON, "'target_name' not string");
    goto failed_target_name;
  }

  target->name = name;
  type = json_object_get_string(obj, "type");
  err = pyg_target_type_from_str(type, &target->type);
  if (!pyg_is_ok(err))
    goto failed_target_name;

  err = pyg_hashmap_cinsert(&pyg->target.map, name, target);
  if (!pyg_is_ok(err))
    goto failed_target_name;
  QUEUE_INSERT_TAIL(&pyg->target.list, &target->member);

  return pyg_ok();

failed_target_name:
  free(target->deps.list);
  target->deps.list = NULL;

failed_alloc_deps:
  free(target);

  return err;
}


pyg_error_t pyg_target_type_from_str(const char* type, pyg_target_type_t* out) {
  if (type == NULL) {
    *out = kPygTargetDefaultType;
    return pyg_ok();
  }

  if (strcmp(type, "executable") == 0)
    *out = kPygTargetExecutable;
  else if (strcmp(type, "static_library") == 0)
    *out = kPygTargetStatic;
  else if (strcmp(type, "static_shared") == 0)
    *out = kPygTargetShared;
  else
    return pyg_error_str(kPygErrJSON, "Invalid target.type: %s", type);

  return pyg_ok();
}


pyg_error_t pyg_load_target_deps(pyg_target_t* target) {
  JSON_Value* val;
  JSON_Array* deps;

  val = json_object_get_value(target->json, "deps");
  /* Assume no deps */
  if (val == NULL)
    return pyg_ok();

  deps = json_value_get_array(val);
  if (deps == NULL)
    return pyg_error_str(kPygErrJSON, "deps not array");

  return pyg_iter_array(deps,
                        "deps",
                        (pyg_iter_array_get_cb) json_array_get_string,
                        pyg_load_target_dep,
                        target);
}


pyg_error_t pyg_load_target_dep(void* val, size_t i, size_t count, void* arg) {
  pyg_error_t err;
  const char* dep;
  pyg_target_t* target;
  pyg_target_t* dep_target;
  pyg_t* child;
  pyg_target_t* child_target;
  const char* colon;
  char* dep_path;

  dep = val;
  target = arg;

  /* Local or external dep? */
  colon = strchr(dep, ':');

  /* Local! */
  if (colon == NULL) {
    dep_target = pyg_hashmap_cget(&target->pyg->target.map, dep);
    if (dep_target == NULL)
      return pyg_error_str(kPygErrGYP, "Dependency not found %s", dep);

    target->deps.list[i] = dep_target;
    return pyg_ok();
  }

  /* Non-local! */
  dep_path = pyg_nresolve(target->pyg->dir,
                          strlen(target->pyg->dir),
                          dep,
                          colon - dep);
  if (dep_path == NULL) {
    return pyg_error_str(kPygErrFS,
                         "Failed to resolve %.*s",
                         colon - dep,
                         dep);
  }

  err = pyg_new_child(dep_path, target->pyg, &child);
  free(dep_path);
  if (!pyg_is_ok(err))
    return err;

  child_target = pyg_hashmap_cget(&child->target.map, colon + 1);
  if (child_target == NULL) {
    return pyg_error_str(kPygErrGYP,
                         "Child %s not found in %s",
                         colon + 1,
                         child->path);
  }

  target->deps.list[i] = child_target;

  return pyg_ok();
}


pyg_error_t pyg_resolve_json(pyg_t* pyg, JSON_Object* json, const char* key) {
  JSON_Value* val;
  JSON_Array* arr;
  size_t i;
  size_t count;

  val = json_object_get_value(json, key);
  /* No values */
  if (val == NULL)
    return pyg_ok();

  arr = json_value_get_array(val);
  /* Not array */
  if (arr == NULL)
    return pyg_error_str(kPygErrJSON, "`%s` not array", key);

  count = json_array_get_count(arr);
  for (i = 0; i < count; i++) {
    const char* path;
    char* resolved;
    JSON_Status status;

    path = json_array_get_string(arr, i);
    if (path == NULL)
      return pyg_error_str(kPygErrJSON, "`%s`[%lu] not string", key, i);

    resolved = pyg_resolve(pyg->dir, path);
    if (resolved == NULL)
      return pyg_error_str(kPygErrFS, "pyg_resolve");

    status = json_array_replace_string(arr, i, resolved);
    free(resolved);
    if (status != JSONSuccess)
      return pyg_error_str(kPygErrJSON, "Failed to insert string into array");
  }

  return pyg_ok();
}


pyg_error_t pyg_translate(pyg_t* pyg, pyg_gen_t* gen, pyg_buf_t* out) {
  QUEUE* q;
  pyg_state_t st;

  st.pyg = pyg;
  st.gen = gen;
  st.out = out;

  /* Post order target traverse */
  QUEUE_FOREACH(q, &pyg->children.list) {
    pyg_t* p;
    QUEUE* qt;

    p = container_of(q, pyg_t, member);

    QUEUE_FOREACH(qt, &p->target.list) {
      pyg_target_t* target;

      target = container_of(qt, pyg_target_t, member);

      gen->target_cb(&st, target);
    }
  }

  return pyg_ok();
}
