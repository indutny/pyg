#include "src/generator/ninja.h"
#include "src/common.h"
#include "src/pyg.h"

#define CHECKED_PRINT(...)                                                    \
    do {                                                                      \
      pyg_error_t err;                                                        \
      err = pyg_buf_put(settings->out, __VA_ARGS__);                          \
      if (!pyg_is_ok(err))                                                    \
        return err;                                                           \
    } while (0)

static pyg_error_t pyg_gen_ninja_target_cb(pyg_target_t* target,
                                           pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_print_build(pyg_target_t* target,
                                             pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_print_link(pyg_target_t* target,
                                            pyg_settings_t* settings);

pyg_gen_t pyg_gen_ninja = {
  .target_cb = pyg_gen_ninja_target_cb
};


pyg_error_t pyg_gen_ninja_target_cb(pyg_target_t* target,
                                    pyg_settings_t* settings) {
  pyg_error_t err;

  err = pyg_gen_ninja_print_build(target, settings);
  if (!pyg_is_ok(err))
    return err;

  return pyg_gen_ninja_print_link(target, settings);
}


pyg_error_t pyg_gen_ninja_print_build(pyg_target_t* target,
                                      pyg_settings_t* settings) {
  unsigned int i;

  for (i = 0; i < target->source.count; i++) {
    pyg_source_t* src;
    const char* rule;

    src = &target->source.list[i];
    if (src->type == kPygSourceSkip || src->type == kPygSourceLink)
      continue;

    switch (src->type) {
      case kPygSourceC: rule = "cc"; break;
      case kPygSourceCC: rule = "cxx"; break;
      default:
        /* TODO(indutny): objective C support */
        UNREACHABLE();
        break;
    }
    CHECKED_PRINT("build %s/%s: %s__%d %s\n",
                  settings->builddir,
                  src->out,
                  rule,
                  target->pyg->id,
                  src->path);
  }

  return pyg_ok();
}


pyg_error_t pyg_gen_ninja_print_link(pyg_target_t* target,
                                     pyg_settings_t* settings) {
  unsigned int i;
  const char* link;
  const char* out_ext;

  /* TODO(indutny): windows, osx support */
  switch (target->type) {
    case kPygTargetExecutable: link = "ld"; out_ext = ""; break;
    case kPygTargetStatic: link = "ar"; out_ext = ".a"; break;
    case kPygTargetShared: link = "sold"; out_ext = ".so"; break;
    default: UNREACHABLE(); break;
  }

  CHECKED_PRINT("build %s/%d/%s%s: %s__%d",
                settings->builddir,
                target->pyg->id,
                target->name,
                out_ext,
                link,
                target->pyg->id);

  for (i = 0; i < target->source.count; i++) {
    pyg_source_t* src;

    src = &target->source.list[i];
    CHECKED_PRINT(" %s/%s", settings->builddir, src->out);
  }
  CHECKED_PRINT("\n");

  /* Root targets should be reachable by plain name */
  if (target->pyg->id == 0) {
    CHECKED_PRINT("build %s: phony %s/0/%s%s\n",
                  target->name,
                  settings->builddir,
                  target->name,
                  out_ext);
  }

  return pyg_ok();
}
