#include "src/generator/ninja.h"
#include "src/common.h"
#include "src/pyg.h"

#define CHECKED_PRINT(...)                                                    \
    do {                                                                      \
      pyg_error_t err;                                                        \
      err = pyg_buf_put(state->out, __VA_ARGS__);                             \
      if (!pyg_is_ok(err))                                                    \
        return err;                                                           \
    } while (0)

static pyg_error_t pyg_gen_ninja_target_cb(pyg_state_t* state,
                                           pyg_target_t* target);

pyg_gen_t pyg_gen_ninja = {
  .target_cb = pyg_gen_ninja_target_cb
};


pyg_error_t pyg_gen_ninja_target_cb(pyg_state_t* state,
                                    pyg_target_t* target) {
  CHECKED_PRINT("build %d__%s: phony\n", target->pyg->id, target->name);

  /* Root targets should be reachable by plain name */
  if (target->pyg->id == 0)
    CHECKED_PRINT("build %s: phony 0__%s\n", target->name, target->name);

  return pyg_ok();
}
