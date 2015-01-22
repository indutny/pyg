#include "src/pyg.h"
#include "src/common.h"
#include "src/generator/base.h"

#include "parson.h"

#include <stdlib.h>

pyg_t* pyg_new(JSON_Value* json) {
  pyg_t* res;

  res = calloc(1, sizeof(*res));
  if (res == NULL)
    return NULL;

  res->json = json;

  return res;
}


void pyg_free(pyg_t* pyg) {
  json_value_free(pyg->json);
  pyg->json = NULL;

  free(pyg);
}


pyg_error_t pyg_translate(pyg_t* pyg, pyg_gen_t* gen, pyg_buf_t* buf) {
  return pyg_ok();
}
