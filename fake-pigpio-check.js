let defineStr = 'USE_FAKE_PIGPIO';
let libRemove = '-lpigpio';

if (process.platform === 'linux') {
  const fs = require('fs');

  try {
    if (fs.existsSync('/proc/cpuinfo')) {
      const lines = fs.readFileSync('/proc/cpuinfo').toString().split('\n');

      for (const line of lines) {
        if (/\bModel\s*:\s*Raspberry Pi\b/i.test(line)) {
          defineStr = 'USE_REAL_PIGPIO';
          libRemove = '';
          break;
        }
      }
    }
  }
  catch (err) {}
}

if (!!process.argv.find(arg => arg === '-l'))
  process.stdout.write(libRemove);
else
  process.stdout.write(defineStr);
