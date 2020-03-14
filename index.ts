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

export enum PinSystem { GPIO, PHYS, WIRING_PI, VIRTUAL = 2 /* Alias for WIRING_PI */ }

export type HtSensorDataCallback = (data: HtSensorData) => void;

export function addSensorDataListener(pin: number | string, callback: HtSensorDataCallback): number;
export function addSensorDataListener(pin: number, pinSystem: PinSystem, callback: HtSensorDataCallback): number;
export function addSensorDataListener(pin: number | string, pinSysOrCallback: PinSystem | HtSensorDataCallback,
                                      callback?: HtSensorDataCallback): number {
  let pinNumber: number;
  let pinSystem = PinSystem.GPIO;

  if (typeof pin === 'string') {
    pinNumber = parseFloat(pin);
    pinNumber = (isNaN(pinNumber) ? 27 : pinNumber);
    const pinSystemIndex = 'pwv'.indexOf(pin.substr(-1).toLowerCase()) + 1;
    pinSystem = [PinSystem.GPIO, PinSystem.PHYS, PinSystem.WIRING_PI, PinSystem.VIRTUAL][pinSystemIndex];
  }
  else
    pinNumber = pin;

  if (typeof pinSysOrCallback === 'function')
    callback = pinSysOrCallback;
  else
    pinSystem = pinSysOrCallback;

  if (typeof callback !== 'function')
    throw new Error('callback function must be specified');

  return ArSignalMonitor.addSensorDataListener(pinNumber, pinSystem, callback);
}

export function removeSensorDataListener(callbackId: number): void {
  ArSignalMonitor.removeSensorDataListener(callbackId);
}

export function convertPin(pin: number, pinSystemFrom: PinSystem, pinSystemTo: PinSystem): number;
export function convertPin(gpioPin: number, pinSystemTo: PinSystem): number;
export function convertPin(pin: string, pinSystemTo: PinSystem): number;
export function convertPin(pin: number | string, pinSystem0: PinSystem, pinSystem1?: PinSystem): number {
  let pinNumber: number;
  let pinSystemFrom: number;
  let pinSystemTo: number;

  if (typeof pin === 'string') {
    pinNumber = parseFloat(pin);
    pinNumber = (isNaN(pinNumber) ? 27 : pinNumber);
    const pinSystemIndex = 'pwv'.indexOf(pin.substr(-1).toLowerCase()) + 1;
    pinSystemFrom = [PinSystem.GPIO, PinSystem.PHYS, PinSystem.WIRING_PI, PinSystem.VIRTUAL][pinSystemIndex];
    pinSystemTo = pinSystem0;
  }
  else {
    pinNumber = pin;

    if (pinSystem1 == null) {
      pinSystemFrom = PinSystem.GPIO;
      pinSystemTo = pinSystem0;
    }
    else {
      pinSystemFrom = pinSystem0;
      pinSystemTo = pinSystem1;
    }
  }

  return ArSignalMonitor.convertPin(pinNumber, pinSystemFrom, pinSystemTo);
}
