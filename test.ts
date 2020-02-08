import { addSensorDataListener, PinSystem, removeSensorDataListener } from './index';

let pinArg = '';

for (let i = 0; i < process.argv.length - 1 && !pinArg; ++i)
  pinArg = process.argv[i] === '-p' ? process.argv[i + 1].toLowerCase() : '';

const pin = parseFloat(pinArg) || 2;
const pinSystemIndex = 'sgp'.indexOf(pinArg.substr(-1)) + 1;
const pinSystem = [PinSystem.VIRTUAL, PinSystem.SYS, PinSystem.GPIO, PinSystem.PHYS][pinSystemIndex];

console.log(`Awaiting humidity/temperature data on pin ${pin}${' sgp'.substr(pinSystemIndex, 1).trim()}...`);

const id = addSensorDataListener(pin, pinSystem, data => {
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
