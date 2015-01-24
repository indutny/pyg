#include "src/generator/ninja.h"
#include "src/common.h"
#include "src/pyg.h"

static pyg_error_t pyg_gen_ninja_target_cb(pyg_state_t* state,
                                           pyg_target_t* target);

pyg_gen_t pyg_gen_ninja = {
  .target_cb = pyg_gen_ninja_target_cb
};


pyg_error_t pyg_gen_ninja_target_cb(pyg_state_t* state,
                                    pyg_target_t* target) {
  if (target->pyg->id == 0)
    return pyg_buf_put(state->out, "build %s: phony\n", target->name);

  return pyg_buf_put(state->out,
                     "build %d__%s: phony\n",
                     target->pyg->id,
                     target->name);
}
