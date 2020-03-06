{
  'targets': [
    {
      'target_name': 'ar_signal_monitor',
      'cflags!': ['-fno-exceptions'],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags': ['-Wall', '-Wno-psabi', '-std=c++14', '-pthread'],
      'cflags_cc': ['-Wall', '-Wno-psabi', '-pthread'],
      'sources': [
        'ar-signal-monitor-node.cpp',
        'ar-signal-monitor.cpp',
        'ar-signal-monitor.h',
        'pigpio-fake.cpp',
        'pigpio-fake.h',
        'pin-conversions.cpp',
        'pin-conversions.h'
      ],
      'include_dirs': [
        '<!(node -e \'require("node-addon-api").include\')',
        '../node_modules/node-addon-api',
        'node_modules/node-addon-api',
        '/usr/include/node',
        '/usr/local/include/node'
      ],
      'dependencies': [
        '<!(node -p \'require("node-addon-api").gyp\')'
      ],
      'libraries': [
        '-lpigpio'
      ],
      'defines': ['NAPI_CPP_EXCEPTIONS'],
      'conditions': [
        ['OS=="mac"', {
          'defines': ['USE_FAKE_PIGPIO'],
          'libraries!': ['-lpigpio'],
          'xcode_settings': {'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'}
        }],
        ['OS=="win"', {
          'defines': ['USE_FAKE_PIGPIO', 'WINDOWS'],
          'libraries!': ['-lpigpio'],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1,
              'AdditionalOptions': [ '/std:c++14', '/utf-8' ]
            }
          }
        }],
        ['OS=="linux"', {
          'defines': ['<!(node fake-pigpio-check.js)'],
          '!libraries': ['<!(node fake-pigpio-check.js -l)']
        }]
      ],
    }
  ]
}
