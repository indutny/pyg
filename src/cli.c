#include "src/common.h"
#include "src/error.h"
#include "src/pyg.h"
#include "src/generator/ninja.h"

#include "parson.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static const int kPygBufferSize = 1024 * 1024;

int main(int argc, char** argv) {
  JSON_Value* json;
  JSON_Object* json_obj;
  pyg_t* pyg;
  pyg_buf_t buf;
  pyg_error_t err;
  const char* file;
  int r;

  if (argc < 2) {
    fprintf(stderr, "Usage:\n  %s file.gyp\n", argv[0]);
    return -1;
  }

  file = argv[1];
  json = json_parse_file_with_comments(file);
  if (json == NULL) {
    fprintf(stderr, "Failed to parse JSON in %s\n", argv[1]);
    return -1;
  }

  r = -1;
  json_obj = json_object(json);
  if (json == NULL) {
    fprintf(stderr, "Failed to parse JSON in %s: not object\n", argv[1]);
    goto failed_to_object;
  }

  err = pyg_buf_init(&buf, kPygBufferSize);
  if (!pyg_is_ok(err)) {
    pyg_error_print(err, stderr);
    goto failed_to_object;
  }

  pyg = pyg_new(json_obj, file);
  if (pyg == NULL) {
    fprintf(stderr, "Failed to allocate pyg_t\n");
    goto failed_pyg_new;
  }

  err = pyg_translate(pyg, &pyg_gen_ninja, &buf);

  if (!pyg_is_ok(err)) {
    pyg_error_print(err, stderr);
    return -1;
  }

  pyg_free(pyg);

  /* Print output */
  pyg_buf_print(&buf, stdout);

  r = 0;

failed_pyg_new:
  pyg_buf_destroy(&buf);

failed_to_object:
  json_value_free(json);

  return r;
}
