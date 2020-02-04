const ArSignalMonitor = require('bindings')('ar_signal_monitor');

ArSignalMonitor.addSensorDataListener(2, (obj: any) => {
  const formatted = JSON.stringify(obj, null, 1).replace(/[{}"\n\r]/g, '').trim();

  console.log(formatted);
});

setTimeout(() => {}, 0x7FFF);
