#include "src/eval.h"
#include "src/common.h"
#include "src/pyg.h"

#include <assert.h>
#include <string.h>

enum pyg_eval_state_s {
  kPygEvalLT,
  kPygEvalParenOpen,
  kPygEvalName
};
typedef enum pyg_eval_state_s pyg_eval_state_t;

static pyg_error_t pyg_eval_calc_size(pyg_t* pyg,
                                      pyg_hashmap_t* vars,
                                      const char* str,
                                      int* out);
static pyg_error_t pyg_eval_write(pyg_t* pyg,
                                  pyg_hashmap_t* vars,
                                  const char* str,
                                  char* out);


pyg_error_t pyg_eval_str(pyg_t* pyg,
                         pyg_hashmap_t* vars,
                         const char* str,
                         char** out) {
  pyg_error_t err;
  char* res;
  int size;

  err = pyg_eval_calc_size(pyg, vars, str, &size);
  if (!pyg_is_ok(err))
    return err;

  res = malloc(size + 1);
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "Failed to malloc() eval result");

  err = pyg_eval_write(pyg, vars, str, res);
  if (!pyg_is_ok(err)) {
    free(res);
    return err;
  }

  *out = res;
  return pyg_ok();
}


pyg_error_t pyg_eval_calc_size(pyg_t* pyg,
                               pyg_hashmap_t* vars,
                               const char* str,
                               int* out) {
  pyg_eval_state_t st;
  const char* p;
  const char* mark;
  int sz;

  st = kPygEvalLT;
  sz = 0;
  for (p = str; *p != '\0'; p++, sz++) {
    char ch;
    const char* value;

    ch = *p;
    switch (st) {
      case kPygEvalLT:
        if (ch != '<')
          break;
        st = kPygEvalParenOpen;
        break;
      case kPygEvalParenOpen:
        if (ch != '(') {
          st = kPygEvalLT;
        } else {
          st = kPygEvalName;
          mark = p + 1;
        }
        break;
      case kPygEvalName:
        if (ch != ')')
          break;
        st = kPygEvalLT;
        value = pyg_hashmap_get(vars, mark, p - mark);
        if (value == NULL && vars != &pyg->vars)
          value = pyg_hashmap_get(&pyg->vars, mark, p - mark);
        if (value == NULL) {
          return pyg_error_str(kPygErrGYP,
                               "variable `%.*s` not found",
                               p - mark,
                               mark);
        }
        sz -= (p - mark) + 3;
        sz += strlen(value);
        break;
    }
  }

  *out = sz;
  return pyg_ok();
}


pyg_error_t pyg_eval_write(pyg_t* pyg,
                           pyg_hashmap_t* vars,
                           const char* str,
                           char* out) {
  pyg_eval_state_t st;
  const char* p;
  char* pout;
  const char* mark;
  int sz;

  st = kPygEvalLT;
  sz = 0;
  pout = out;
  for (p = str; *p != '\0'; p++, sz++) {
    char ch;

    ch = *p;
    switch (st) {
      case kPygEvalLT:
        if (ch != '<')
          break;
        st = kPygEvalParenOpen;
        break;
      case kPygEvalParenOpen:
        if (ch != '(') {
          st = kPygEvalLT;
        } else {
          st = kPygEvalName;
          mark = p + 1;
        }
        break;
      case kPygEvalName:
        {
          int len;
          const char* value;
          if (ch != ')')
            continue;

          value = pyg_hashmap_get(vars, mark, p - mark);
          if (value == NULL && vars != &pyg->vars)
            value = pyg_hashmap_get(&pyg->vars, mark, p - mark);
          assert(value != NULL);

          /* Revert `<(` */
          pout -= 2;
          len = strlen(value);
          memcpy(pout, value, len);
          pout += len;

          st = kPygEvalLT;
          /* Skip copying `)` */
          continue;
        }
    }

    *pout = ch;
    pout++;
  }
  *pout = '\0';

  return pyg_ok();
}


pyg_error_t pyg_eval_test(pyg_t* pyg,
                          pyg_hashmap_t* vars,
                          const char* str,
                          int* out) {
  *out = 1;
  return pyg_ok();
}
