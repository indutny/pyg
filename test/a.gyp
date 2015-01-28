{
  "variables": {
    "hello": "ohai",
    "ok": "abc <(hello) def"
  },
  "targets": [{
    'target_name': "a",

    "dependencies": [
      "b",
      "../test/sub/c.gyp:c"
    ],

    "sources": [
      "../src/pyg.c",
    ],

    "conditions": [
      ["hello == 'ohai' and xkcd != \"great\" || spam > 0 and sink != 42", {
        "sources": [
          "./ohai.c",
        ],
      }],
    ],
  }, {
    "target_name": "b",
    "type": "static_library",

    "sources": [
      "ohai.cc",
    ],
  }],
}
