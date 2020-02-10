import { addSensorDataListener, removeSensorDataListener } from './index';

let pin = '';

for (let i = 0; i < process.argv.length - 1 && !pin; ++i)
  pin = process.argv[i] === '-p' ? process.argv[i + 1]?.toLowerCase().trim() : '';

pin = pin || '2';
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
    date.getTimezoneOffset() * 60000).toISOString().substr(0, 19).replace('T', ' ') + ':';

  console.log(timeStamp, formatted);
});

process.on('SIGINT', () => {
  removeSensorDataListener(id);
  console.log();
  process.exit(0);
});
