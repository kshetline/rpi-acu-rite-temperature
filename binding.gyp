{
  "targets": [
    {
      "target_name": "ar_signal_monitor",
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "cflags": ["-Wall", "-std=c++11", "-pthread"],
      "cflags_cc": ["-Wall", "-pthread"],
      "sources": [
        "ar-signal-monitor-node.cpp",
        "ar-signal-monitor-node.h",
        "ar-signal-monitor.cpp",
        "ar-signal-monitor.h"
      ],
      "include_dirs": [
        "node_modules/node-addon-api",
        "/usr/include/node",
        "/usr/local/include/node",
        "<!(node -e \"require('node-addon-api').include\")"
      ],
      "libraries": [
        "-lwiringPi"
      ],
      "defines": ["NAPI_CPP_EXCEPTIONS"],
      'conditions': [
        ["OS==\"mac\"", {
          "defines": ["USE_FAKE_WIRING_PI"],
          "libraries!": ["-lwiringPi"],
          "xcode_settings": {"GCC_ENABLE_CPP_EXCEPTIONS": "YES"}
        }],
        ["OS==\"win\"", {
          "defines": ["USE_FAKE_WIRING_PI"],
          "libraries!": ["-lwiringPi"]
        }],
      ],
    }
  ]
}
