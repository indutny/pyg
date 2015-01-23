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
  pyg_t* pyg;
  pyg_buf_t buf;
  pyg_error_t err;
  int r;

  r = -1;
  if (argc < 2) {
    fprintf(stderr, "Usage:\n  %s file.gyp\n", argv[0]);
    goto fail;
  }

  err = pyg_buf_init(&buf, kPygBufferSize);
  if (!pyg_is_ok(err)) {
    pyg_error_print(err, stderr);
    goto fail;
  }

  err = pyg_new(argv[1], &pyg);
  if (!pyg_is_ok(err)) {
    pyg_error_print(err, stderr);
    goto failed_pyg_new;
  }

  err = pyg_load(pyg);
  if (!pyg_is_ok(err))
    goto failed_pyg_load;

  err = pyg_translate(pyg, &pyg_gen_ninja, &buf);
  if (!pyg_is_ok(err)) {
    pyg_error_print(err, stderr);
    r = -1;
    goto failed_pyg_load;
  }

  /* Print output */
  pyg_buf_print(&buf, stdout);

  r = 0;

failed_pyg_load:
  pyg_free(pyg);

failed_pyg_new:
  pyg_buf_destroy(&buf);

fail:
  return r;
}
