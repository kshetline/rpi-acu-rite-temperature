let defineStr = 'USE_FAKE_PIGPIO';

if (process.platform === 'linux') {
  const fs = require('fs');

  try {
    if (fs.existsSync('/proc/cpuinfo')) {
      const lines = fs.readFileSync('/proc/cpuinfo').toString().split('\n');

      for (const line of lines) {
        if (/\bModel\s*:\s*Raspberry Pi\b/i.test(line)) {
          defineStr = 'USE_REAL_PIGPIO';
          break;
        }
      }
    }
  }
  catch (err) {}
}

process.stdout.write(defineStr);
