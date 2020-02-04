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
        "ArTemperatureHumiditySignalMonitor.cpp",
        "ArTemperatureHumiditySignalMonitor.h"
      ],
      "include_dirs": [
        "node_modules/node-addon-api",
        "/usr/include/node",
        "<!(node -e \"require('node-addon-api').include\")"
      ],
      "libraries": [
        "-lwiringPi"
      ],
      "defines": ["NAPI_ENABLE_CPP_EXCEPTIONS"]
    }
  ]
}
