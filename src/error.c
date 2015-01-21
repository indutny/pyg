#include "src/error.h"
#include "src/common.h"

pyg_error_t pyg_error(pyg_error_code_t code) {
  return (pyg_error_t) { .code = code, .data = { .ret = 0 } };
}


pyg_error_t pyg_error_str(pyg_error_code_t code, const char* str) {
  return (pyg_error_t) { .code = code, .data = { .str = str } };
}


pyg_error_t pyg_error_num(pyg_error_code_t code, int ret) {
  return (pyg_error_t) { .code = code, .data = { .ret = ret } };
}


#define PYG_ERR_CODE_TO_STR(V) case kPygErr##V: return #V;

const char* pyg_error_code_to_str(pyg_error_code_t code) {
  switch (code) {
    case kPygOk:
      return "ok";
    PYG_ERROR_ENUM(PYG_ERR_CODE_TO_STR)
  }

  UNREACHABLE();
}


void pyg_error_print(pyg_error_t err, FILE* out) {
  fprintf(out, "Error: %s\n", pyg_error_code_to_str(err.code));
}
