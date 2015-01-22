#ifndef SRC_GENERATOR_BASE_H_
#define SRC_GENERATOR_BASE_H_

#include "src/error.h"

/* Forward declarations */
struct pyg_s;
struct pyg_buf_s;
struct pyg_target_s;

typedef struct pyg_gen_s pyg_gen_t;
typedef struct pyg_gen_state_s pyg_gen_state_t;
typedef pyg_error_t (*pyg_gen_target_cb)(pyg_gen_state_t* state,
                                         struct pyg_target_s* target);

struct pyg_gen_s {
  pyg_gen_target_cb target_cb;
};

struct pyg_gen_state_s {
  struct pyg_s* pyg;
  pyg_gen_t* gen;
  struct pyg_buf_s* buf;
};

#endif  /* SRC_GENERATOR_BASE_H_ */
