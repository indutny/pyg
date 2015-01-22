#include "src/pyg.h"

#include "parson.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  JSON_Value* json;
  pyg_t* pyg;

  if (argc < 2) {
    fprintf(stderr, "Usage:\n  %s file.gyp\n", argv[0]);
    return -1;
  }

  json = json_parse_file_with_comments(argv[1]);
  if (json == NULL) {
    fprintf(stderr, "Failed to parse JSON in %s\n", argv[1]);
    return -1;
  }

  pyg = pyg_new(json);
  if (pyg == NULL) {
    fprintf(stderr, "Failed to allocate pyg_t\n");
    return -1;
  }

  pyg_free(pyg);

  return 0;
}
