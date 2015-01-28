#ifndef SRC_UNROLL_H_
#define SRC_UNROLL_H_

#include "src/common.h"

/* Forward declarations */
struct pyg_s;

pyg_error_t pyg_unroll_value(struct pyg_s* pyg,
                             pyg_hashmap_t* vars,
                             pyg_value_t* input,
                             pyg_value_t** out);

#endif  /* SRC_UNROLL_H_ */
