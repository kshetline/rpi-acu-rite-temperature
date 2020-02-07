const ArSignalMonitor = require('bindings')('ar_signal_monitor');

export interface HtSensorData {
  batteryLow: boolean;
  channel: string;       // A, B, or C
  humidity: number;      // Integer 0-100
  miscData1: number;     // Bits  2-15 of the transmission.
  miscData2: number;     // Bits 17-23 of the transmission.
  miscData3: number;     // Bits 33-35 of the transmission.
  rawTemp: number;       // Integer tenths of a degree Celsius plus 1000 (original transmission data format)
  signalQuality: number; // Integer 0-100
  tempCelsius: number;
  tempFahrenheit: number;
  validChecksum: boolean; // Is the data fully trustworthy?
}

export enum PinSystem { VIRTUAL, SYS, GPIO, PHYS }

export type HtSensorDataCallback = (data: HtSensorData) => void;

export function addSensorDataListener(pin: number, callback: HtSensorDataCallback): number;
export function addSensorDataListener(pin: number, pinSystem: PinSystem, callback: HtSensorDataCallback): number;
export function addSensorDataListener(pin: number, pinSysOrCallback: PinSystem | HtSensorDataCallback,
                                      callback?: HtSensorDataCallback): number {
  let pinSystem = PinSystem.VIRTUAL;

  if (typeof pinSysOrCallback === 'function')
    callback = pinSysOrCallback;
  else
    pinSystem = pinSysOrCallback;

  return ArSignalMonitor.addSensorDataListener(pin, pinSystem, callback);
}

export function removeSensorDataListener(callbackId: number): void {
  ArSignalMonitor.removeSensorDataListener(callbackId);
}
