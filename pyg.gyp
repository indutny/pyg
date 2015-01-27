{
  "variables": {
    "hello": "world",
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

    "cflags": "-g3 -O0 -std=c99 -Wall -Wextra -Wno-unused-parameter -pedantic",

    "sources": [
      "src/common.c",
      "src/error.c",
      "src/pyg.c",
      "src/generator/ninja.c",
      "src/cli.c",
    ]
  }, {
    "target_name": "parson",
    "type": "static_library",

    "include_dirs": [
      "deps/parson",
    ],

    "cflags": "-g3 -O0 -std=c99 -Wall -Wextra -Wno-unused-parameter -pedantic",

    "sources": [
      "deps/parson/parson.c",
    ],
  }],
}
