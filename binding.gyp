{
  "targets": [
    {
      "target_name": "ar_signal_monitor",
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "cflags": ["-Wall", "-Wno-psabi", "-std=c++11", "-pthread"],
      "cflags_cc": ["-Wall", "-Wno-psabi", "-pthread"],
      "sources": [
        "ar-signal-monitor-node.cpp",
        "ar-signal-monitor-node.h",
        "ar-signal-monitor.cpp",
        "ar-signal-monitor.h"
      ],
      "include_dirs": [
        "<!(node -e \"require('node-addon-api').include\")",
        "../node_modules/node-addon-api",
        "node_modules/node-addon-api",
        "/usr/include/node",
        "/usr/local/include/node"
      ],
      "libraries": [
        "-lpigpio"
      ],
      "defines": ["NAPI_CPP_EXCEPTIONS"],
      "conditions": [
        ["OS==\"mac\"", {
          "defines": ["USE_FAKE_PIGPIO"],
          "libraries!": ["-lpigpio"],
          "xcode_settings": {"GCC_ENABLE_CPP_EXCEPTIONS": "YES"}
        }],
        ["OS==\"win\"", {
          "defines": ["USE_FAKE_PIGPIO"],
          "libraries!": ["-lpigpio"]
        }],
      ],
    }
  ]
}
