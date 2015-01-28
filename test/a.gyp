{
  "variables": {
    "hello": "ohai",
    "ok": "abc <(hello) def",
    "xkcd": "great",
    "spam": 1,
    "sink": 23,
    "src": "../src"
  },
  "targets": [{
    'target_name': "a",

    "dependencies": [
      "b",
      "../test/sub/c.gyp:c"
    ],

    "sources": [
      "<(src)/pyg.c",
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
