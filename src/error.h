#ifndef SRC_ERROR_H_
#define SRC_ERROR_H_

#include <stdio.h>

typedef struct pyg_error_s pyg_error_t;

#define PYG_ERROR_ENUM(X)                                                     \
    X(NoMem)                                                                  \
    X(JSON)                                                                   \

#define PYG_DEFINE_ERROR(V) kPygErr##V,

enum pyg_error_code_s {
  kPygOk,

  PYG_ERROR_ENUM(PYG_DEFINE_ERROR)

  kPygErrLast
};

#undef PYG_DEFINE_ERROR

/* No forward declaration of enums in pedantic */
typedef enum pyg_error_code_s pyg_error_code_t;

struct pyg_error_s {
  pyg_error_code_t code;
  const char* str;
};


#define pyg_is_ok(err) ((err).code == kPygOk)
#define pyg_ok() ((pyg_error_t) { .code = kPygOk, .str = 0 })

pyg_error_t pyg_error(pyg_error_code_t code);
pyg_error_t pyg_error_str(pyg_error_code_t code, const char* str);
const char* pyg_error_code_to_str(pyg_error_code_t code);
void pyg_error_print(pyg_error_t err, FILE* out);

#endif  /* SRC_ERROR_H_ */
