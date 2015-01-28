#ifndef SRC_EVAL_H_
#define SRC_EVAL_H_

#include "src/common.h"

/* Forward declarations */
struct pyg_s;

pyg_error_t pyg_eval_str(struct pyg_s* pyg,
                         pyg_hashmap_t* vars,
                         const char* str,
                         char** out);

pyg_error_t pyg_eval_test(struct pyg_s* pyg,
                          pyg_hashmap_t* vars,
                          const char* str,
                          int* out);

#endif  /* SRC_EVAL_H_ */
