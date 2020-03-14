import { addSensorDataListener, removeSensorDataListener,
         convertPin, PinSystem } from './index';

let pin = '';
let pinouts = false;

for (let i = 0; i < process.argv.length && !pin; ++i) {
  pin = process.argv[i] === '-p' ? process.argv[i + 1]?.toLowerCase().trim() : '';
  pinouts = pinouts || process.argv[i] === '--pinouts';
}

if (pinouts) {
  for (let i = 1; i <= 56; ++i) {
    if (41 <= i && i <= 49)
      continue;

    console.log('phys %s -> gpio %s -> wpi %s', i,
      convertPin(i, PinSystem.PHYS, PinSystem.GPIO),
      convertPin(i, PinSystem.PHYS, PinSystem.WIRING_PI));
  }

  console.log();

  for (let i = 0; i <= 31; ++i) {
    console.log('gpio %s -> phys %s -> wpi %s', i,
      convertPin(i, PinSystem.GPIO, PinSystem.PHYS),
      convertPin(i, PinSystem.GPIO, PinSystem.WIRING_PI));
  }

  console.log();

  for (let i = 0; i <= 31; ++i) {
    console.log('wpi %s -> gpio %s -> phys %s', i,
      convertPin(i, PinSystem.WIRING_PI, PinSystem.GPIO),
      convertPin(i, PinSystem.WIRING_PI, PinSystem.PHYS));
  }

  process.exit(0);
}

pin = pin || '27';
console.log(`Awaiting humidity/temperature data on pin ${pin}...`);

const id = addSensorDataListener(pin, data => {
  let formatted = JSON.stringify(data, null, 1).replace(/[{}"\n\r]/g, '').trim();

  formatted = formatted.replace(/(miscData1:) \d+/, '$1 0x' +
    data.miscData1.toString(16).toUpperCase().padStart(4, '0'));

  formatted = formatted.replace(/(miscData2:) \d+/, '$1 0x' +
    data.miscData2.toString(16).toUpperCase().padStart(2, '0'));

  formatted = formatted.replace(/(miscData3:) \d+/, '$1 0x' +
    data.miscData3.toString(16).toUpperCase());

  const date = new Date();
  const timeStamp = new Date(date.getTime() -
    date.getTimezoneOffset() * 60000).toISOString().substr(11, 19).replace('T', ' ') + ':';

  console.log(timeStamp, formatted);
});

function cleanUp() {
  removeSensorDataListener(id);
  console.log();
  process.exit(0);
}

process.on('SIGINT', cleanUp);
process.on('SIGTERM', cleanUp);
