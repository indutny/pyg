#include "src/eval.h"
#include "src/common.h"
#include "src/pyg.h"

#include <assert.h>
#include <string.h>

enum pyg_eval_state_e {
  kPygEvalLT,
  kPygEvalParenOpen,
  kPygEvalName
};
typedef enum pyg_eval_state_e pyg_eval_state_t;

enum pyg_lex_type_e {
  kPygLexNone,
  kPygLexWS,
  kPygLexName,
  kPygLexBinary,
  kPygLexDStr,
  kPygLexSStr,
  kPygLexInt
};
typedef enum pyg_lex_type_e pyg_lex_type_t;

static pyg_error_t pyg_eval_calc_size(pyg_t* pyg,
                                      pyg_hashmap_t* vars,
                                      const char* str,
                                      int* out);
static pyg_error_t pyg_eval_write(pyg_t* pyg,
                                  pyg_hashmap_t* vars,
                                  const char* str,
                                  char* out);
static pyg_error_t pyg_ast_lex(const char* str,
                               pyg_lex_type_t* out,
                               int* out_len);
static pyg_error_t pyg_ast_consume_lex(const char** str,
                                       const char** out,
                                       pyg_lex_type_t* out_type,
                                       int* out_len);
static pyg_error_t pyg_ast_parse_expr(const char** str, pyg_ast_t** out);
static pyg_error_t pyg_ast_classify_binary(const char* op,
                                           int len,
                                           pyg_ast_binary_op_t* out);
static pyg_error_t pyg_ast_parse_binary(const char** str,
                                        pyg_ast_binary_op_t priority,
                                        pyg_ast_t** out);
static pyg_error_t pyg_ast_parse_literal(const char** str, pyg_ast_t** out);


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


pyg_error_t pyg_ast_lex(const char* str, pyg_lex_type_t* out, int* out_len) {
  const char* p;
  pyg_lex_type_t st;

  st = kPygLexNone;
  for (p = str; *p != '\0'; p++) {
    char ch;

    ch = *p;
    switch (st) {
      case kPygLexNone:
        switch (ch) {
          case ' ':
          case '\t':
            st = kPygLexWS;
            continue;
          case '!':
          case '=':
          case '>':
          case '<':
          case '|':
          case '&':
          case 'o':  /* or */
          case 'a':  /* and */
            st = kPygLexBinary;
            continue;
          case '"':
            st = kPygLexDStr;
            continue;
          case '\'':
            st = kPygLexSStr;
            continue;
          default:
            break;
        }

        if (ch == '-' || (ch >= '0' && ch <= '9'))
          st = kPygLexInt;
        else
          st = kPygLexName;
        break;

      case kPygLexWS:
        if (ch != ' ' && ch != '\t')
          goto done;
        break;

      case kPygLexBinary:
        switch (p[-1]) {
          case 'o':
            if (ch != 'r') {
              st = kPygLexName;
              goto done;
            }
            break;
          case 'r':
            goto done;
          case 'a':
            if (ch != 'n') {
              st = kPygLexName;
              goto done;
            }
            break;
          case 'n':
            if (ch != 'd') {
              st = kPygLexName;
              goto done;
            }
            break;
          case 'd':
            goto done;
          default:
            if (ch != '!' && ch != '=' && ch != '>' && ch != '<' &&
                ch != '&' && ch != '|') {
              goto done;
            }
            break;
        }
        break;

      case kPygLexDStr:
        if (ch == '"') {
          p++;
          goto done;
        }
        break;

      case kPygLexSStr:
        if (ch == '\'') {
          p++;
          goto done;
        }
        break;

      case kPygLexInt:
        if (!(ch >= '0' && ch <= '9'))
          goto done;
        break;

      case kPygLexName:
        if (ch == ' ' || ch == '\t' || ch == '!' || ch == '>' || ch == '<' ||
            ch == '=' || ch == '|' || ch == '&' || ch == '"' || ch == '\'') {
          goto done;
        }
        break;
    }
  }

done:
  *out_len = p - str;
  *out = st;
  return pyg_ok();
}


pyg_error_t pyg_ast_consume_lex(const char** str,
                                const char** out,
                                pyg_lex_type_t* out_type,
                                int* out_len) {
  pyg_error_t err;

  do {
    *out = *str;
    err = pyg_ast_lex(*str, out_type, out_len);
    if (!pyg_is_ok(err))
      return err;

    *str += *out_len;
  } while (*out_type == kPygLexWS);

  return pyg_ok();
}


pyg_error_t pyg_ast_parse_expr(const char** str, pyg_ast_t** out) {
  return pyg_ast_parse_binary(str, kPygAstBinaryAll, out);
}


pyg_error_t pyg_ast_classify_binary(const char* op,
                                    int len,
                                    pyg_ast_binary_op_t* out) {
  pyg_ast_binary_op_t res;

  /* TODO(indutny): could be moved to lexer */
  res = kPygAstBinaryInvalid;
  if (len == 1) {
    if (op[0] == '<')
      res = kPygAstBinaryLT;
    else if (op[0] == '>')
      res = kPygAstBinaryGT;
  } else if (len == 2) {
    if (strncmp(op, "==", 2) == 0)
      res = kPygAstBinaryEq;
    else if (strncmp(op, "!=", 2) == 0)
      res = kPygAstBinaryNotEq;
    else if (strncmp(op, "<=", 2) == 0)
      res = kPygAstBinaryLTE;
    else if (strncmp(op, ">=", 2) == 0)
      res = kPygAstBinaryGTE;
    else if (strncmp(op, "||", 2) == 0 || strncmp(op, "or", 2) == 0)
      res = kPygAstBinaryOr;
    else if (strncmp(op, "&&", 2) == 0)
      res = kPygAstBinaryAnd;
  } else if (len == 3) {
    if (strncmp(op, "and", 3) == 0)
      res = kPygAstBinaryAnd;
  }

  if (res == kPygAstBinaryInvalid) {
    return pyg_error_str(kPygErrASTFatal, "Invalid binary token: %.*s",
                         len,
                         op);
  }

  *out = res;
  return pyg_ok();
}


pyg_error_t pyg_ast_parse_binary(const char** str,
                                 pyg_ast_binary_op_t priority,
                                 pyg_ast_t** out) {
  pyg_error_t err;
  pyg_ast_t* left;
  pyg_ast_t* right;

  err = pyg_ast_parse_literal(str, &left);
  if (!pyg_is_ok(err))
    return err;

  /* Grow the tree */
  for (;;) {
    pyg_lex_type_t lex;
    const char* op_str;
    int op_len;
    pyg_ast_binary_op_t op;
    const char* tmp;
    pyg_ast_t* join;
    pyg_ast_binary_op_t next;

    tmp = *str;
    err = pyg_ast_consume_lex(&tmp, &op_str, &lex, &op_len);
    if (!pyg_is_ok(err))
      goto fatal_after_left;

    /* End of the string */
    if (lex == kPygLexNone) {
      *out = left;
      return pyg_ok();
    }

    /* Invalid token */
    if (lex != kPygLexBinary) {
      err = pyg_error_str(kPygErrASTWarn, "Invalid token when expected binary");
      goto fatal_after_left;
    }

    err = pyg_ast_classify_binary(op_str, op_len, &op);
    if (!pyg_is_ok(err))
      goto fatal_after_left;

    /* Higher priority will parse this */
    if (op > priority) {
      *out = left;
      return pyg_ok();
    }

    /* Consume token */
    *str = tmp;

    /* Get the right side */
    if (op <= kPygAstBinaryLow)
      next = kPygAstBinaryLow;
    else if (op <= kPygAstBinaryMiddle)
      next = kPygAstBinaryMiddle;
    else
      next = op;
    err = pyg_ast_parse_binary(str, next, &right);
    if (!pyg_is_ok(err))
      goto fatal_after_left;

    join = malloc(sizeof(*join));
    if (join == NULL) {
      err = pyg_error_str(kPygErrNoMem, "pyg_ast_t");
      goto fatal_after_right;
    }

    join->type = kPygAstBinary;
    join->value.binary.op = op;
    join->value.binary.left = left;
    join->value.binary.right = right;

    left = join;
  }

  *out = left;
  return pyg_ok();

fatal_after_right:
  pyg_ast_free(right);

fatal_after_left:
  pyg_ast_free(left);
  return err;
}


pyg_error_t pyg_ast_parse_literal(const char** str, pyg_ast_t** out) {
  pyg_error_t err;
  const char* op_str;
  pyg_lex_type_t lex;
  int op_len;
  pyg_ast_t* res;

  err = pyg_ast_consume_lex(str, &op_str, &lex, &op_len);
  if (!pyg_is_ok(err))
    return err;

  res = malloc(sizeof(*res));
  if (res == NULL)
    return pyg_error_str(kPygErrNoMem, "pyg_ast_t");

  switch (lex) {
    case kPygLexName:
      res->type = kPygAstName;
      res->value.str.str = op_str;
      res->value.str.len = op_len;
      break;
    case kPygLexDStr:
    case kPygLexSStr:
      res->type = kPygAstStr;
      res->value.str.str = op_str + 1;
      res->value.str.len = op_len - 2;
      break;
    case kPygLexInt:
      {
        char st[1024];

        res->type = kPygAstInt;
        snprintf(st, sizeof(st), "%.*s", op_len, op_str);
        res->value.num = atoi(st);
        break;
      }
    default:
      return pyg_error_str(kPygErrASTFatal,
                           "Invalid literal/name token: %.*s",
                           op_len,
                           op_str);
  }

  *out = res;
  return pyg_ok();
}


pyg_error_t pyg_ast_parse(const char* str, pyg_ast_t** out) {
  return pyg_ast_parse_expr(&str, out);
}


void pyg_ast_free(pyg_ast_t* ast) {
  if (ast->type == kPygAstBinary) {
    pyg_ast_free(ast->value.binary.left);
    pyg_ast_free(ast->value.binary.right);
  }
  free(ast);
}


pyg_error_t pyg_eval_test(pyg_t* pyg,
                          pyg_hashmap_t* vars,
                          const char* str,
                          int* out) {
  pyg_error_t err;
  pyg_ast_t* ast;

  err = pyg_ast_parse(str, &ast);
  if (!pyg_is_ok(err))
    return err;

  *out = 1;
  return pyg_ok();
}
