#include "src/generator/ninja.h"
#include "src/common.h"
#include "src/pyg.h"

#include <limits.h>  /* PATH_MAX */

#define CHECKED_PRINT(...)                                                    \
    do {                                                                      \
      pyg_error_t err;                                                        \
      err = pyg_buf_put(settings->out, __VA_ARGS__);                          \
      if (!pyg_is_ok(err))                                                    \
        return err;                                                           \
    } while (0)

static pyg_error_t pyg_gen_ninja_prologue_cb(pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_epilogue_cb(pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_target_cb(pyg_target_t* target,
                                           pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_print_rules(pyg_target_t* target,
                                             pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_print_build(pyg_target_t* target,
                                             pyg_settings_t* settings);
static pyg_error_t pyg_gen_ninja_print_link(pyg_target_t* target,
                                            pyg_settings_t* settings);
static const char* pyg_gen_ninja_link(pyg_target_type_t type);
static const char* pyg_gen_ninja_out_ext(pyg_target_type_t type);
static const char* pyg_gen_ninja_cmd(pyg_target_t* target, const char* name);
static const char* pyg_gen_ninja_path(pyg_target_t* target,
                                      const char* name,
                                      const char* ext,
                                      pyg_settings_t* settings);

pyg_gen_t pyg_gen_ninja = {
  .prologue_cb = pyg_gen_ninja_prologue_cb,
  .target_cb = pyg_gen_ninja_target_cb,
  .epilogue_cb = pyg_gen_ninja_epilogue_cb,
};


pyg_error_t pyg_gen_ninja_prologue_cb(pyg_settings_t* settings) {
  /* Shameless plagiarism from GYP */
  CHECKED_PRINT("rule copy\n"
                "  command = ln -f $in $out 2>/dev/null || "
                "(rm -rf $out && cp -af $in $out)\n\n");

  return pyg_ok();
}


pyg_error_t pyg_gen_ninja_epilogue_cb(pyg_settings_t* settings) {
  return pyg_ok();
}


pyg_error_t pyg_gen_ninja_target_cb(pyg_target_t* target,
                                    pyg_settings_t* settings) {
  pyg_error_t err;

  err = pyg_gen_ninja_print_rules(target, settings);
  if (!pyg_is_ok(err))
    return err;

  err = pyg_gen_ninja_print_build(target, settings);
  if (!pyg_is_ok(err))
    return err;

  return pyg_gen_ninja_print_link(target, settings);
}


pyg_error_t pyg_gen_ninja_print_rules(pyg_target_t* target,
                                      pyg_settings_t* settings) {
  return pyg_ok();
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
      case kPygSourceCXX: rule = "cxx"; break;
      default:
        /* TODO(indutny): objective C support */
        UNREACHABLE();
        break;
    }

    CHECKED_PRINT("build %s: %s %s\n",
                  pyg_gen_ninja_path(target, src->out, "", settings),
                  pyg_gen_ninja_cmd(target, rule),
                  src->path);
  }

  return pyg_ok();
}


const char* pyg_gen_ninja_link(pyg_target_type_t type) {
  switch (type) {
    case kPygTargetExecutable: return "ld";
    case kPygTargetStatic: return "ar";
    case kPygTargetShared: return "sold";
    default: UNREACHABLE(); return NULL;
  }
}


const char* pyg_gen_ninja_out_ext(pyg_target_type_t type) {
  /* TODO(indutny): windows, osx support */
  switch (type) {
    case kPygTargetExecutable: return "";
    case kPygTargetStatic: return ".a";
    case kPygTargetShared: return ".so";
    default: UNREACHABLE(); return NULL;
  }
}


pyg_error_t pyg_gen_ninja_print_link(pyg_target_t* target,
                                     pyg_settings_t* settings) {
  unsigned int i;
  const char* link;
  const char* out_ext;

  link = pyg_gen_ninja_link(target->type);
  out_ext = pyg_gen_ninja_out_ext(target->type);

  CHECKED_PRINT("build %s: %s",
                pyg_gen_ninja_path(target, target->name, out_ext, settings),
                pyg_gen_ninja_cmd(target, link));

  for (i = 0; i < target->source.count; i++) {
    pyg_source_t* src;

    src = &target->source.list[i];
    CHECKED_PRINT(" %s",
                  pyg_gen_ninja_path(target, src->out, "", settings));
  }

  for (i = 0; i < target->deps.count; i++) {
    pyg_target_t* dep;
    const char* dep_ext;

    dep = target->deps.list[i];
    dep_ext = pyg_gen_ninja_out_ext(dep->type);
    CHECKED_PRINT(" %s", pyg_gen_ninja_path(dep, dep->name, dep_ext, settings));
  }

  CHECKED_PRINT("\n");

  /* Root targets should be reachable by plain name */
  if (target->pyg->id == 0) {
    CHECKED_PRINT("build %s/%s%s: copy %s\n",
                  settings->builddir,
                  target->name,
                  out_ext,
                  pyg_gen_ninja_path(target, target->name, out_ext, settings));
    CHECKED_PRINT("build %s: phony %s/%s%s\n",
                  target->name,
                  settings->builddir,
                  target->name,
                  out_ext);
  }

  return pyg_ok();
}


const char* pyg_gen_ninja_cmd(pyg_target_t* target, const char* name) {
  static char out[PATH_MAX];
  snprintf(out,
           sizeof(out),
           "%s__%s__%d",
           target->name,
           name,
           target->pyg->id);
  return out;
}


const char* pyg_gen_ninja_path(pyg_target_t* target,
                               const char* name,
                               const char* ext,
                               pyg_settings_t* settings) {
  static char out[PATH_MAX];
  snprintf(out,
           sizeof(out),
           "%s/%d/%s/%s%s",
           settings->builddir,
           target->pyg->id,
           target->name,
           name,
           ext);
  return out;
}
