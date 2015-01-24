#include "src/error.h"
#include "src/common.h"

#include <stdarg.h>

pyg_error_t pyg_error(pyg_error_code_t code) {
  return (pyg_error_t) { .code = code, .str = NULL };
}


pyg_error_t pyg_error_str(pyg_error_code_t code, const char* fmt, ...) {
  static char buf[1024];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);

  return (pyg_error_t) { .code = code, .str = buf };
}


#define PYG_ERR_CODE_TO_STR(V) case kPygErr##V: return #V;

const char* pyg_error_code_to_str(pyg_error_code_t code) {
  switch (code) {
    case kPygOk:
      return "ok";
    case kPygErrLast:
      UNREACHABLE();
    PYG_ERROR_ENUM(PYG_ERR_CODE_TO_STR)
  }

  UNREACHABLE();
}


void pyg_error_print(pyg_error_t err, FILE* out) {
  fprintf(out, "Error: %s (%s)\n", pyg_error_code_to_str(err.code), err.str);
}
