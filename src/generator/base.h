#ifndef SRC_GENERATOR_BASE_H_
#define SRC_GENERATOR_BASE_H_

#include "src/error.h"

/* Forward declarations */
struct pyg_s;
struct pyg_settings_s;
struct pyg_target_s;

typedef struct pyg_gen_s pyg_gen_t;
typedef pyg_error_t (*pyg_gen_target_cb)(struct pyg_target_s* target,
                                         struct pyg_settings_s* settings);

struct pyg_gen_s {
  pyg_gen_target_cb target_cb;
};

#endif  /* SRC_GENERATOR_BASE_H_ */
