#ifndef SRC_UNROLL_H_
#define SRC_UNROLL_H_

#include "src/common.h"

pyg_error_t pyg_unroll_value(pyg_proto_hashmap_t* vars,
                             pyg_value_t* input,
                             pyg_value_t** out);
pyg_error_t pyg_unroll_str(pyg_proto_hashmap_t* vars,
                           const char* input,
                           char** out);

#endif  /* SRC_UNROLL_H_ */
