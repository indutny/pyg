{
  "variables": {
    "cflags": "-g3 -O0 -std=c99 -Wall -Wextra -Wno-unused-parameter -pedantic"
  },
  "targets": [{
    "target_name": "pyg",
    "type": "executable",

    "dependencies": [
      "parson",
    ],

    "include_dirs": [
      ".",
      "deps/parson",
    ],

    "cflags": "<(cflags)",

    "sources": [
      "src/common.c",
      "src/error.c",
      "src/pyg.c",
      "src/generator/ninja.c",
      "src/cli.c",
      "src/json.c",
      "src/eval.c",
      "src/unroll.c",
    ]
  }, {
    "target_name": "parson",
    "type": "static_library",

    "include_dirs": [
      "deps/parson",
    ],

    "cflags": "<(cflags)",

    "sources": [
      "deps/parson/parson.c",
    ],
  }],
}
