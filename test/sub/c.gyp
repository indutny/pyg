{
  "targets": [{
    'target_name': "c",
    "type": "static_library",

    "deps": [
      "../a.gyp:b",
    ],
  }, {
    "target_name": "d",
    "type": "shared_library",
  }],
}
