const ArSignalMonitor = require('bindings')('ar_signal_monitor');

console.log('Awaiting temperature/humidity data...');

let updates = 0;

const id = ArSignalMonitor.addSensorDataListener(2, (data: any) => {
  let formatted = JSON.stringify(data, null, 1).replace(/[{}"\n\r]/g, '').trim();

  formatted = formatted.replace(/(miscData1:) \d+/, '$1 0x' +
    data.miscData1.toString(16).toUpperCase().padStart(4, '0'));

  formatted = formatted.replace(/(miscData2:) \d+/, '$1 0x' +
    data.miscData2.toString(16).toUpperCase().padStart(2, '0'));

  formatted = formatted.replace(/(miscData3:) \d+/, '$1 0x' +
    data.miscData3.toString(16).toUpperCase());

  console.log(formatted);

  if (++updates == 4)
    ArSignalMonitor.removeSensorDataListener(id);
});
