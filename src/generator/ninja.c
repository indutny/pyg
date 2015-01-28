#include "src/generator/ninja.h"
#include "src/common.h"
#include "src/pyg.h"

#include <limits.h>  /* PATH_MAX */
#include <string.h>

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
static const char* pyg_gen_ninja_link(pyg_target_t* target);
static const char* pyg_gen_ninja_out_ext(pyg_target_type_t type);
static const char* pyg_gen_ninja_cmd(pyg_target_t* target, const char* name);
static const char* pyg_gen_ninja_path(pyg_target_t* target,
                                      const char* name,
                                      const char* ext,
                                      pyg_settings_t* settings);
static const char* pyg_gen_ninja_src_path(const char* path,
                                          pyg_settings_t* settings);

pyg_gen_t pyg_gen_ninja = {
  .prologue_cb = pyg_gen_ninja_prologue_cb,
  .target_cb = pyg_gen_ninja_target_cb,
  .epilogue_cb = pyg_gen_ninja_epilogue_cb,
};


pyg_error_t pyg_gen_ninja_prologue_cb(pyg_settings_t* settings) {
  /* Shameless plagiarism from GYP */
  CHECKED_PRINT("cc = cc\n"
                "cxx = c++\n"
                "ld = $cc\n"
                "ldxx = $cxx\n"
                "ar = ar\n\n");

  CHECKED_PRINT("rule copy\n"
                "  command = ln -f $in $out 2>/dev/null || "
                "(rm -rf $out && cp -af $in $out)\n"
                "  description = COPY $out\n");

  return pyg_ok();
}


pyg_error_t pyg_gen_ninja_epilogue_cb(pyg_settings_t* settings) {
  return pyg_ok();
}


pyg_error_t pyg_gen_ninja_target_cb(pyg_target_t* target,
                                    pyg_settings_t* settings) {
  pyg_error_t err;

  /* TODO(indutny): actions? */
  if (target->type == kPygTargetNone)
    return pyg_ok();

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
  JSON_Array* arr;
  size_t i;
  size_t count;
  const char* cflags;
  const char* ldflags;
  static const int types[] = { kPygSourceC, kPygSourceCXX };

  /* Include dirs */
  CHECKED_PRINT("\n%s =", pyg_gen_ninja_cmd(target, "include_dirs"));

  arr = json_object_get_array(target->json, "include_dirs");
  count = json_array_get_count(arr);

  /* TODO(indutny): MSVC support */
  for (i = 0; i < count; i++)
    CHECKED_PRINT(" -I%s", json_array_get_string(arr, i));

  /* Defines */
  CHECKED_PRINT("\n%s =", pyg_gen_ninja_cmd(target, "defines"));

  arr = json_object_get_array(target->json, "defines");
  count = json_array_get_count(arr);

  /* TODO(indutny): MSVC support */
  for (i = 0; i < count; i++)
    CHECKED_PRINT(" -D%s", json_array_get_string(arr, i));

  /* Libraries */
  CHECKED_PRINT("\n%s =", pyg_gen_ninja_cmd(target, "libs"));

  arr = json_object_get_array(target->json, "libraries");
  count = json_array_get_count(arr);

  /* TODO(indutny): MSVC support */
  for (i = 0; i < count; i++)
    CHECKED_PRINT(" %s", json_array_get_string(arr, i));

  CHECKED_PRINT("\n");

  /* cflags, ldflags */
  /* TODO(indutny): cflags/ldflags could be arrays */
  cflags = json_object_get_string(target->json, "cflags");
  ldflags = json_object_get_string(target->json, "ldflags");
  if (cflags == NULL)
    cflags = "";
  if (ldflags == NULL)
    ldflags = "";
  CHECKED_PRINT("%s = %s\n", pyg_gen_ninja_cmd(target, "cflags"), cflags);
  CHECKED_PRINT("%s = %s\n\n", pyg_gen_ninja_cmd(target, "ldflags"), ldflags);

  for (i = 0; i < ARRAY_SIZE(types); i++) {
    int type = types[i];
    const char* cc;
    const char* ld;

    if ((target->source.types & type) == 0)
      continue;

    if (type == kPygSourceC) {
      cc = "cc";
      ld = "ld";
    } else {
      cc = "cxx";
      ld = "ldxx";
    }

    CHECKED_PRINT("rule %s\n", pyg_gen_ninja_cmd(target, cc));
    CHECKED_PRINT("  command = $%s -MMD -MF $out.d ", cc);
    CHECKED_PRINT("$%s ", pyg_gen_ninja_cmd(target, "defines"));
    CHECKED_PRINT("$%s ", pyg_gen_ninja_cmd(target, "include_dirs"));
    CHECKED_PRINT("$%s -c $in -o $out\n", pyg_gen_ninja_cmd(target, "cflags"));
    CHECKED_PRINT("  description = COMPILE $out\n"
                  "  depfile = $out.d\n"
                  "  deps = gcc\n\n");

    CHECKED_PRINT("rule %s\n", pyg_gen_ninja_cmd(target, ld));
    CHECKED_PRINT("  command = $ld $%s ", pyg_gen_ninja_cmd(target, "ldflags"));
    CHECKED_PRINT("-o $out $in $%s\n", pyg_gen_ninja_cmd(target, "libs"));
    CHECKED_PRINT("  description = LINK $out\n\n");
  }

  CHECKED_PRINT("rule %s\n", pyg_gen_ninja_cmd(target, "ar"));
  CHECKED_PRINT("  command = libtool -static -o $out $in\n");
  CHECKED_PRINT("  description = LIBTOOL-STATIC $out\n\n");

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
                  pyg_gen_ninja_src_path(src->path, settings));
  }

  return pyg_ok();
}


const char* pyg_gen_ninja_link(pyg_target_t* target) {
  switch (target->type) {
    case kPygTargetNone:
      return "phony";
    case kPygTargetExecutable:
      if (target->source.types & kPygSourceCXX)
        return "ldxx";
      else
        return "ld";
    case kPygTargetStatic: return "ar";
    case kPygTargetShared:
      if (target->source.types & kPygSourceCXX)
        return "soldxx";
      else
        return "sold";
    default: UNREACHABLE(); return NULL;
  }
}


const char* pyg_gen_ninja_out_ext(pyg_target_type_t type) {
  /* TODO(indutny): windows, osx support */
  switch (type) {
    case kPygTargetNone: return "";
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

  link = pyg_gen_ninja_link(target);
  out_ext = pyg_gen_ninja_out_ext(target->type);

  if (target->source.count == 0) {
    CHECKED_PRINT("build %s: phony\n",
                  pyg_gen_ninja_path(target, target->name, out_ext, settings));
  } else {
    CHECKED_PRINT("build %s: %s",
                  pyg_gen_ninja_path(target, target->name, out_ext, settings),
                  pyg_gen_ninja_cmd(target, link));

    for (i = 0; i < target->source.count; i++) {
      pyg_source_t* src;

      src = &target->source.list[i];
      CHECKED_PRINT(" %s",
                    pyg_gen_ninja_path(target, src->out, "", settings));
    }
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
           "%s_%s_%d",
           name,
           target->name,
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


const char* pyg_gen_ninja_src_path(const char* path, pyg_settings_t* settings) {
  int dlen;
  int plen;

  if (settings->deprefix == NULL)
    return path;

  dlen = strlen(settings->deprefix);
  plen = strlen(path);

  if (plen < dlen)
    return path;

  if (memcmp(settings->deprefix, path, dlen) != 0)
    return path;

  /* Skip prefix and `/` */
  return path + dlen + 1;
}
