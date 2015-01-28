#include "src/unroll.h"
#include "src/common.h"

#include <assert.h>
#include <string.h>

enum pyg_unroll_state_e {
  kPygUnrollLT,
  kPygUnrollParenOpen,
  kPygUnrollName
};
typedef enum pyg_unroll_state_e pyg_unroll_state_t;

static pyg_error_t pyg_unroll_calc_size(pyg_proto_hashmap_t* vars,
                                        pyg_str_t* str,
                                        int* out);
static pyg_error_t pyg_unroll_write(pyg_proto_hashmap_t* vars,
                                    pyg_str_t* str,
                                    char* out);

pyg_error_t pyg_unroll_value(pyg_proto_hashmap_t* vars,
                             pyg_value_t* input,
                             pyg_value_t** out) {
  pyg_error_t err;
  pyg_value_t* res;
  int size;

  /* No string - nothing to unroll */
  if (input->type != kPygValueStr) {
    res = malloc(sizeof(*res));
    if (res == NULL)
      return pyg_error_str(kPygErrNoMem, "Failed to malloc() unroll result");

    *res = *input;
    *out = res;
    return pyg_ok();
  }

  err = pyg_unroll_calc_size(vars, &input->value.str, &size);
  if (!pyg_is_ok(err))
    return err;

  /* Embed string in the structure */
  res = malloc(sizeof(*res) + size + 1);
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "Failed to malloc() unroll result");

  res->value.str.str = (char*) res + sizeof(*res);
  res->value.str.len = size;
  err = pyg_unroll_write(vars, &input->value.str, (char*) res->value.str.str);
  if (!pyg_is_ok(err)) {
    free(res);
    return err;
  }

  *out = res;
  return pyg_ok();
}


pyg_error_t pyg_unroll_str(pyg_proto_hashmap_t* vars,
                           const char* input,
                           char** out) {
  pyg_error_t err;
  pyg_str_t str;
  int size;
  char* res;

  str.str = input;
  str.len = strlen(input);
  err = pyg_unroll_calc_size(vars, &str, &size);
  if (!pyg_is_ok(err))
    return err;

  /* Embed string in the structure */
  res = malloc(size + 1);
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "Failed to malloc() unroll result");

  err = pyg_unroll_write(vars, &str, res);
  if (!pyg_is_ok(err)) {
    free(res);
    return err;
  }

  *out = res;
  return pyg_ok();
}


pyg_error_t pyg_unroll_calc_size(pyg_proto_hashmap_t* vars,
                                 pyg_str_t* str,
                                 int* out) {
  pyg_unroll_state_t st;
  const char* p;
  const char* end;
  const char* mark;
  int sz;

  st = kPygUnrollLT;
  sz = 0;
  end = str->str + str->len;
  for (p = str->str; p != end; p++, sz++) {
    char ch;
    const char* value;

    ch = *p;
    switch (st) {
      case kPygUnrollLT:
        if (ch != '<')
          break;
        st = kPygUnrollParenOpen;
        break;
      case kPygUnrollParenOpen:
        if (ch != '(') {
          st = kPygUnrollLT;
        } else {
          st = kPygUnrollName;
          mark = p + 1;
        }
        break;
      case kPygUnrollName:
        if (ch != ')')
          break;
        st = kPygUnrollLT;
        value = pyg_proto_hashmap_get(vars, mark, p - mark);
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


pyg_error_t pyg_unroll_write(pyg_proto_hashmap_t* vars,
                             pyg_str_t* str,
                             char* out) {
  pyg_unroll_state_t st;
  const char* p;
  const char* end;
  char* pout;
  const char* mark;
  int sz;

  st = kPygUnrollLT;
  sz = 0;
  pout = out;
  end = str->str + str->len;
  for (p = str->str; p != end; p++, sz++) {
    char ch;

    ch = *p;
    switch (st) {
      case kPygUnrollLT:
        if (ch != '<')
          break;
        st = kPygUnrollParenOpen;
        break;
      case kPygUnrollParenOpen:
        if (ch != '(') {
          st = kPygUnrollLT;
        } else {
          st = kPygUnrollName;
          mark = p + 1;
        }
        break;
      case kPygUnrollName:
        {
          pyg_error_t err;
          int len;
          pyg_value_t* value;
          char* str_value;

          if (ch != ')')
            continue;

          value = pyg_proto_hashmap_get(vars, mark, p - mark);
          assert(value != NULL);

          err = pyg_value_to_str(value, &str_value);
          if (!pyg_is_ok(err))
            return err;

          /* Revert `<(` */
          pout -= 2;
          len = strlen(str_value);
          memcpy(pout, str_value, len);
          pout += len;
          free(str_value);

          st = kPygUnrollLT;
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
