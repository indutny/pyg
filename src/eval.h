#ifndef SRC_EVAL_H_
#define SRC_EVAL_H_

#include "src/common.h"

/* Forward declarations */
struct pyg_s;

typedef struct pyg_ast_s pyg_ast_t;
typedef struct pyg_ast_binary_s pyg_ast_binary_t;

enum pyg_ast_binary_op_e {
  kPygAstBinaryEq,
  kPygAstBinaryNotEq,

  kPygAstBinaryLow = kPygAstBinaryNotEq,

  kPygAstBinaryLT,
  kPygAstBinaryGT,
  kPygAstBinaryLTE,
  kPygAstBinaryGTE,

  kPygAstBinaryMiddle = kPygAstBinaryGTE,

  kPygAstBinaryAnd,
  kPygAstBinaryOr,

  kPygAstBinaryAll = kPygAstBinaryOr,

  kPygAstBinaryInvalid
};
typedef enum pyg_ast_binary_op_e pyg_ast_binary_op_t;

struct pyg_ast_binary_s {
  pyg_ast_binary_op_t op;
  pyg_ast_t* left;
  pyg_ast_t* right;
};

enum pyg_ast_type_e {
  kPygAstBinary,
  kPygAstName,
  kPygAstStr,
  kPygAstInt
};
typedef enum pyg_ast_type_e pyg_ast_type_t;

struct pyg_ast_s {
  pyg_ast_type_t type;
  union {
    pyg_ast_binary_t binary;
    pyg_str_t str;
    int num;
  } value;
};

pyg_error_t pyg_ast_parse(const char* str, pyg_ast_t** out);
void pyg_ast_free(pyg_ast_t* ast);

pyg_error_t pyg_eval_test(struct pyg_s* pyg,
                          pyg_hashmap_t* vars,
                          const char* str,
                          int* out);

#endif  /* SRC_EVAL_H_ */
