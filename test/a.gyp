{
  "targets": [{
    'target_name': "a",

    "deps": [
      "b",
      "test/sub/c.gyp:c"
    ],
  }, {
    "target_name": "b"
  }],
}
