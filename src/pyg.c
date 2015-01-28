#include "src/pyg.h"
#include "src/pyg-internal.h"
#include "src/common.h"
#include "src/eval.h"
#include "src/generator/base.h"
#include "src/json.h"
#include "src/queue.h"
#include "src/unroll.h"

#include "parson.h"

#include <stdlib.h>
#include <string.h>


static const unsigned kPygChildrenCount = 16;
static const unsigned kPygTargetCount = 16;
static const unsigned kPygVarCount = 16;


static pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out);
static pyg_error_t pyg_free_child(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_free_target(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_free_var(pyg_hashmap_item_t* item, void* arg);
static pyg_error_t pyg_load(pyg_t* pyg);
static pyg_error_t pyg_load_variables(pyg_t* pyg,
                                      JSON_Object* json,
                                      pyg_hashmap_t* out);
static pyg_error_t pyg_eval_conditions(pyg_t* pyg,
                                       JSON_Object* json,
                                       pyg_hashmap_t* vars);
static pyg_error_t pyg_load_targets(pyg_t* pyg);
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
static pyg_error_t pyg_create_sources(pyg_target_t* target);


pyg_error_t pyg_new_child(const char* path, pyg_t* parent, pyg_t** out) {
  pyg_error_t err;
  pyg_t* res;
  char* rpath;
  JSON_Value* clone;

  rpath = pyg_realpath(path);
  if (rpath == NULL)
    return pyg_error_str(kPygErrFS, "pyg_realpath(%s)", path);

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

  err = pyg_clone_json(res->json, kPygMergeAuto, &clone);
  if (!pyg_is_ok(err))
    goto failed_clone_json;
  res->clone = clone;

  res->obj = json_object(res->clone);
  if (res->obj == NULL) {
    err = pyg_error_str(kPygErrJSON, "JSON not object: %s", path);
    goto failed_to_object;
  }

  res->path = rpath;
  res->dir = pyg_dirname(res->path);
  if (res->dir == NULL) {
    err = pyg_error_str(kPygErrFS, "pyg_dirname(%s)", res->path);
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

  err = pyg_hashmap_init(&res->vars, kPygVarCount);
  if (!pyg_is_ok(err))
    goto failed_vars_init;

  err = pyg_load(res);
  if (!pyg_is_ok(err)) {
    pyg_hashmap_cdelete(&res->root->children.map, res->path);
    pyg_free(res);
    return err;
  }

  *out = res;
  return pyg_ok();

failed_vars_init:
  pyg_hashmap_destroy(&res->target.map);

failed_target_init:
  pyg_hashmap_cdelete(&res->root->children.map, res->path);

failed_children_insert:
  if (res->parent == NULL)
    pyg_hashmap_destroy(&res->children.map);

failed_children_init:
  free(res->dir);
  res->dir = NULL;

failed_clone_json:
  json_value_free(res->json);
  res->json = NULL;

failed_to_object:
  json_value_free(res->clone);
  res->clone = NULL;

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

  pyg_hashmap_iterate(&pyg->vars, pyg_free_var, NULL);
  pyg_hashmap_destroy(&pyg->vars);

  json_value_free(pyg->json);
  json_value_free(pyg->clone);
  pyg->json = NULL;
  pyg->clone = NULL;
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
  unsigned int i;

  target = item->value;
  QUEUE_REMOVE(&target->member);

  for (i = 0; i < target->source.count; i++) {
    free(target->source.list[i].out);
    free(target->source.list[i].filename);
  }

  free(target->source.list);
  free(target->deps.list);
  free(target);

  return pyg_ok();
}


pyg_error_t pyg_free_var(pyg_hashmap_item_t* item, void* arg) {
  free(item->value);

  return pyg_ok();
}


pyg_error_t pyg_load(pyg_t* pyg) {
  pyg_error_t err;

  /* TODO(indutny): Support target_defaults */
  err = pyg_load_variables(pyg, pyg->obj, &pyg->vars);
  if (!pyg_is_ok(err))
    return err;

  err = pyg_eval_conditions(pyg, pyg->obj, &pyg->vars);
  if (!pyg_is_ok(err))
    return err;

  return pyg_load_targets(pyg);
}


pyg_error_t pyg_load_variables(pyg_t* pyg,
                               JSON_Object* json,
                               pyg_hashmap_t* out) {
  size_t i;
  size_t count;
  JSON_Value* val;
  JSON_Object* vars;

  val = json_object_get_value(json, "variables");
  if (val == NULL)
    return pyg_ok();
  vars = json_value_get_object(val);
  if (vars == NULL)
    return pyg_error_str(kPygErrGYP, "`variables` not object");

  count = json_object_get_count(vars);
  for (i = 0; i < count; i++) {
    pyg_error_t err;
    JSON_Value* prop;
    const char* name;
    pyg_value_t val;

    name = json_object_get_name(vars, i);
    prop = json_object_get_value(vars, name);

    switch (json_value_get_type(prop)) {
      case JSONString:
        {
          const char* str;

          str = json_value_get_string(prop);
          val.type = kPygValueStr;
          val.value.str.str = str;
          val.value.str.len = strlen(str);
        }
        break;
      case JSONNumber:
        val.type = kPygValueInt;
        val.value.num = json_value_get_number(prop);
        break;
      default:
        return pyg_error_str(kPygErrGYP,
                             "`variables`[%d] is not string/integer",
                             (int) i);
    }

    err = pyg_add_var(pyg, out, name, &val);
    if (!pyg_is_ok(err))
      return err;
  }

  return pyg_ok();
}


pyg_error_t pyg_add_var(pyg_t* pyg,
                        pyg_hashmap_t* vars,
                        const char* key,
                        pyg_value_t* val) {
  pyg_error_t err;
  char key_st[1024];
  const char* ekey;
  int len;
  pyg_value_t* dup_val;

  /* Evaluate variable using all known variables at the point */
  /* TODO(indutny): ./pyg ... -D... -D... - how should this handle it? */
  err = pyg_unroll_value(pyg, vars, val, &dup_val);
  if (!pyg_is_ok(err))
    return err;

  len = strlen(key);

  /* Default value */
  if (key[len - 1] == '%') {
    ekey = key_st;
    snprintf(key_st, sizeof(key_st), "%.*s", len - 1, key);

    if (pyg_hashmap_cget(vars, ekey) != NULL)
      return pyg_ok();
    if (pyg_hashmap_cget(&pyg->vars, ekey) != NULL)
      return pyg_ok();
  } else {
    ekey = key;
  }

  err = pyg_hashmap_cinsert(vars, ekey, dup_val);
  if (!pyg_is_ok(err))
    free(dup_val);
  return err;
}


pyg_error_t pyg_eval_conditions(pyg_t* pyg,
                                JSON_Object* json,
                                pyg_hashmap_t* vars) {
  size_t i;
  JSON_Value* val;
  JSON_Array* conds;

  val = json_object_get_value(json, "conditions");
  if (val == NULL)
    return pyg_ok();
  conds = json_value_get_array(val);
  if (conds == NULL)
    return pyg_error_str(kPygErrGYP, "`conditions` not array");

  for (i = 0; i < json_array_get_count(conds); i++) {
    pyg_error_t err;
    JSON_Array* pair;
    size_t pair_size;
    const char* test;
    int btest;
    JSON_Object* branch;

    pair = json_array_get_array(conds, i);
    if (pair == NULL)
      return pyg_error_str(kPygErrGYP, "`conditions`[%d] not array", (int) i);

    pair_size = json_array_get_count(pair);
    if (!(pair_size == 2 || pair_size == 3)) {
      return pyg_error_str(kPygErrGYP,
                           "`conditions`[%d] has invalid length", (int) i);
    }

    test = json_array_get_string(pair, 0);

    err = pyg_eval_test(pyg, vars, test, &btest);
    if (!pyg_is_ok(err))
      return err;

    /* No else branch */
    if (btest == 0 && pair_size == 2)
      continue;

    branch = json_array_get_object(pair, btest == 0 ? 2 : 1);
    if (branch == NULL) {
      return pyg_error_str(kPygErrGYP,
                           "`conditions`[%d] branch not object",
                           (int) i);
    }

    err = pyg_merge_json_obj(json, branch, kPygMergeAuto);
    if (!pyg_is_ok(err))
      return err;

    /* Update variables from condition branch */
    err = pyg_load_variables(pyg, branch, vars);
    if (!pyg_is_ok(err))
      return err;
  }

  return pyg_ok();
}


pyg_error_t pyg_load_targets(pyg_t* pyg) {
  pyg_error_t err;
  JSON_Array* targets;
  QUEUE* q;

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

    /* Create list of source/type/output structs */
    err = pyg_create_sources(target);
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

  err = pyg_hashmap_init(&target->vars, kPygVarCount);
  if (!pyg_is_ok(err))
    goto failed_init_vars;

  err = pyg_load_variables(pyg, target->json, &target->vars);
  if (!pyg_is_ok(err))
    goto failed_load_vars;

  /* Need to eval it early because of the source list */
  err = pyg_eval_conditions(pyg, target->json, &target->vars);
  if (!pyg_is_ok(err))
    goto failed_load_vars;

  /* Allocate space for dependencies */
  target->deps.count =
      json_array_get_count(json_object_get_array(obj, "dependencies"));
  target->deps.list = calloc(target->deps.count, sizeof(*target->deps.list));
  if (target->deps.list == NULL) {
    err = pyg_error_str(kPygErrNoMem, "pyg_target_t.deps");
    goto failed_load_vars;
  }

  /* Allocate space for source files */
  target->source.count =
      json_array_get_count(json_object_get_array(obj, "sources"));
  target->source.list = calloc(target->source.count,
                               sizeof(*target->source.list));
  if (target->source.list == NULL) {
    err = pyg_error_str(kPygErrNoMem, "pyg_target_t.source");
    goto failed_alloc_source;
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
  free(target->source.list);
  target->source.list = NULL;

failed_alloc_source:
  free(target->deps.list);
  target->deps.list = NULL;

failed_load_vars:
  pyg_hashmap_destroy(&target->vars);

failed_init_vars:
  free(target);

  return err;
}


pyg_error_t pyg_target_type_from_str(const char* type, pyg_target_type_t* out) {
  if (type == NULL) {
    *out = kPygTargetDefaultType;
    return pyg_ok();
  }

  if (strcmp(type, "none") == 0)
    *out = kPygTargetNone;
  else if (strcmp(type, "executable") == 0)
    *out = kPygTargetExecutable;
  else if (strcmp(type, "static_library") == 0)
    *out = kPygTargetStatic;
  else if (strcmp(type, "shared_library") == 0)
    *out = kPygTargetShared;
  else
    return pyg_error_str(kPygErrJSON, "Invalid target.type: %s", type);

  return pyg_ok();
}


pyg_error_t pyg_load_target_deps(pyg_target_t* target) {
  JSON_Value* val;
  JSON_Array* deps;

  val = json_object_get_value(target->json, "dependencies");
  /* Assume no deps */
  if (val == NULL)
    return pyg_ok();

  deps = json_value_get_array(val);
  if (deps == NULL)
    return pyg_error_str(kPygErrJSON, "dependencies not array");

  return pyg_iter_array(deps,
                        "dependencies",
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
      return pyg_error_str(kPygErrGYP, "Dependency `%s` not found", dep);

    goto done;
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

  dep_target = pyg_hashmap_cget(&child->target.map, colon + 1);
  if (dep_target == NULL) {
    return pyg_error_str(kPygErrGYP,
                         "Child %s not found in %s",
                         colon + 1,
                         child->path);
  }

done:
  if (dep_target->type == kPygTargetExecutable) {
    return pyg_error_str(kPygErrGYP,
                         "Dependency `%s` has non-linkable type",
                         dep);
  }

  target->deps.list[i] = dep_target;
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
      return pyg_error_str(kPygErrJSON, "`%s`[%d] not string", key, (int) i);

    resolved = pyg_resolve(pyg->dir, path);
    if (resolved == NULL)
      return pyg_error_str(kPygErrFS, "pyg_resolve(%s, %s)", pyg->dir, path);

    status = json_array_replace_string(arr, i, resolved);
    free(resolved);
    if (status != JSONSuccess)
      return pyg_error_str(kPygErrJSON, "Failed to insert string into array");
  }

  return pyg_ok();
}


pyg_error_t pyg_create_sources(pyg_target_t* target) {
  size_t i;
  JSON_Array* arr;

  arr = json_object_get_array(target->json, "sources");
  for (i = 0; i < target->source.count; i++) {
    pyg_source_t* src;
    const char* ext;
    int n;

    src = &target->source.list[i];

    src->path = json_array_get_string(arr, i);
    ext = strrchr(src->path, '.');

    /* No extension - skip */
    if (ext == NULL) {
      src->type = kPygSourceSkip;
    } else {
      ext++;
      if (strcmp(ext, "c") == 0)
        src->type = kPygSourceC;
      else if (strcmp(ext, "cc") == 0 || strcmp(ext, "cpp") == 0)
        src->type = kPygSourceCXX;
      else if (strcmp(ext, "m") == 0)
        src->type = kPygSourceObjC;
      else if (strcmp(ext, "mm") == 0)
        src->type = kPygSourceObjCXX;
      else if (strcmp(ext, "o") == 0 || strcmp(ext, "so") == 0)
        src->type = kPygSourceLink;
      else if (strcmp(ext, "dylib") == 0 || strcmp(ext, "dll") == 0)
        src->type = kPygSourceLink;
      else
        src->type = kPygSourceSkip;
    }

    target->source.types |= src->type;

    /* Unknown extension or non-compilable - no output */
    if (src->type == kPygSourceSkip || src->type == kPygSourceLink)
      continue;

    src->filename = pyg_filename(src->path);
    if (src->filename == NULL)
      return pyg_error_str(kPygErrNoMem, "target.sources.out");

    /* Alloc enough space for output path */
    n = snprintf(NULL, 0, "%s_%d.o", src->filename, (int) i);
    src->out = malloc(n + 1);
    if (src->out == NULL)
      return pyg_error_str(kPygErrNoMem, "target.sources.out");

    /* Garbled name, but won't conflict with others */
    snprintf(src->out, n + 1, "%s_%d.o", src->filename, (int) i);
  }

  return pyg_ok();
}


pyg_error_t pyg_translate(pyg_t* pyg, pyg_settings_t* settings) {
  QUEUE* q;

  settings->gen->prologue_cb(settings);

  /* Post order target traverse */
  QUEUE_FOREACH(q, &pyg->children.list) {
    pyg_t* p;
    QUEUE* qt;

    p = container_of(q, pyg_t, member);

    QUEUE_FOREACH(qt, &p->target.list) {
      pyg_target_t* target;

      target = container_of(qt, pyg_target_t, member);

      settings->gen->target_cb(target, settings);
    }
  }

  settings->gen->epilogue_cb(settings);

  return pyg_ok();
}
