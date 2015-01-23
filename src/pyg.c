#include "src/pyg.h"
#include "src/common.h"
#include "src/generator/base.h"

#include "parson.h"

#include <stdlib.h>


static const char* kPygDefaultTargetType = "executable";


static pyg_error_t pyg_translate_target(void* val,
                                        size_t i,
                                        size_t count,
                                        void* arg);

pyg_t* pyg_new(JSON_Object* json, const char* path) {
  pyg_t* res;
  char* tmp;

  res = calloc(1, sizeof(*res));
  if (res == NULL)
    return NULL;

  res->json = json;
  res->path = path;
  tmp = pyg_dirname(res->path);
  if (tmp == NULL)
    goto failed_dirname;

  res->dir = pyg_realpath(tmp);
  free(tmp);
  if (res->dir == NULL)
    goto failed_dirname;

  return res;

failed_dirname:
  free(res);
  return NULL;
}


void pyg_free(pyg_t* pyg) {
  pyg->json = NULL;
  free(pyg->dir);
  pyg->dir = NULL;

  free(pyg);
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
  targets = json_object_get_array(pyg->json, "targets");
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
