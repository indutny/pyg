#include "src/eval.h"
#include "src/common.h"
#include "src/pyg.h"

#include <string.h>


pyg_error_t pyg_eval_str(pyg_t* pyg,
                         pyg_hashmap_t* vars,
                         const char* str,
                         char** out) {
  char* res;

  res = strdup(str);
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "Failed to strdup() eval string");

  *out = res;
  return pyg_ok();
}
