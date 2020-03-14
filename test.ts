import { addSensorDataListener, convertPin, PinSystem, removeSensorDataListener } from './index';

let pin = '';
let pinouts = false;

for (let i = 0; i < process.argv.length && !pin; ++i) {
  pin = process.argv[i] === '-p' ? process.argv[i + 1]?.toLowerCase().trim() : '';
  pinouts = pinouts || process.argv[i] === '--pinouts';
}

if (pinouts) {
  const format = (n: number): string => (n < 0 ? '··' : String(n).padStart(2, '0'));

  for (let i = 1; i <= 56; ++i) {
    if (41 <= i && i <= 49)
      continue;

    console.log('phys %s -> gpio %s -> wpi %s', format(i),
      format(convertPin(i, PinSystem.PHYS, PinSystem.GPIO)),
      format(convertPin(i, PinSystem.PHYS, PinSystem.WIRING_PI)));
  }

  console.log();

  for (let i = 0; i <= 31; ++i) {
    console.log('gpio %s -> phys %s -> wpi %s', format(i),
      format(convertPin(i, PinSystem.PHYS)),
      format(convertPin(i, PinSystem.WIRING_PI)));
  }

  console.log();

  for (let i = 0; i <= 31; ++i) {
    console.log('wpi %s -> gpio %s -> phys %s', format(i),
      format(convertPin(i + 'w', PinSystem.GPIO)),
      format(convertPin(i + 'w', PinSystem.PHYS)));
  }

  process.exit(0);
}

pin = pin || '27';
console.log(`Awaiting humidity/temperature data on pin ${pin}...`);

const id = addSensorDataListener(pin, data => {
  const date = new Date();
  const timeStamp = new Date(date.getTime() -
    date.getTimezoneOffset() * 60000).toISOString().substr(11, 19).replace('T', ' ') + ':';

  if (data.channel === '-') {
    console.log(timeStamp, 'Dead air detected');
    return;
  }

  let formatted = JSON.stringify(data, null, 1).replace(/[{}"\n\r]/g, '').trim();

  formatted = formatted.replace(/(miscData1:) \d+/, '$1 0x' +
    data.miscData1.toString(16).toUpperCase().padStart(4, '0'));

  formatted = formatted.replace(/(miscData2:) \d+/, '$1 0x' +
    data.miscData2.toString(16).toUpperCase().padStart(2, '0'));

  formatted = formatted.replace(/(miscData3:) \d+/, '$1 0x' +
    data.miscData3.toString(16).toUpperCase());

  console.log(timeStamp, formatted);
});

function cleanUp() {
  removeSensorDataListener(id);
  console.log();
  process.exit(0);
}

process.on('SIGINT', cleanUp);
process.on('SIGTERM', cleanUp);
