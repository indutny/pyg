{
  "targets": [{
    'target_name': "c",
    "type": "static_library",

    "deps": [
      "../a.gyp:b",
    ],

    "sources": [
      "c.c",
    ],
  }, {
    "target_name": "d",
    "type": "shared_library",

    "sources": [
      "d.cc",
    ],
  }],
}
