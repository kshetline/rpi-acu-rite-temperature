# rpi-acu-rite-temperature

## Library for reading the Acu-Rite 06002M Wireless Temperature and Humidity Sensor using a Raspberry Pi with a 433Mhz receiver

_Note: This software is neither produced by nor endorsed by [Acu-Rite](https://acu-rite.com/site/). The author of this library is not associated with Acu-Rite or its products in any way, apart from finding their [433 MHz remote humidity/temperature sensors](https://www.amazon.com/gp/product/B00T0K8NXC/) useful for this kind of project._

_This library was inspired by work originally done by [Ray Wang](http://rayshobby.net/?p=8998)._

`rpi-acu-rite-temperature` can be used for JavaScript/TypeScript programming in a Node.js environment, or the included C++ code can be used directly. It is the JavaScript/TypeScript interface that is documented here.

Full functionality requires a Raspberry Pi, but this code can be installed and compiled under MacOS (and perhaps Windows as well — not yet verified) in such a way that it returns simulated data for testing and development use.

---
> **Breaking changes:**
>
> As of version 2.0.0, `rpi-acu-rite-temperature` uses the `pigpio` library instead of `wiringPi` for GPIO functionality. This greatly improves the reliability of signal processing, but requires running code with elevated privileges (via `sudo`, or otherwise as root). Pin numbering now defaults to Broadcom GPIO numbering, `wiringPi` numbering is available using `PinSystem.WIRING_PI`, and `PinSystem.SYS` is not available.
---

### Installation

`npm install rpi-acu-rite-temperature`

### Usage

`const { addSensorDataListener, removeSensorDataListener } = require('rpi-acu-rite-temperature');`

_...or..._

`import { addSensorDataListener, PinSystem, removeSensorDataListener } from 'rpi-acu-rite-temperature';`

### HtSensorData

The format of the data returned by this library:

```typescript
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
```

`signalQuality` is measured over a five minute window, and will register low even for a strong signal until a full five minutes have passed.

While it's best for `validChecksum` to be `true`, any data provided has at least been validated by three parity bits even if the checksum didn't come out right. In some cases, if you aren't willing to accept data from a weak signal despite an invalid checksum, you might have to wait a long time for any data at all.

### addSensorDataListener

This function is used to register a callback that receives the above temperature/humidity data. You must specify the input `pin` to which your [433 MHz RF receiver](https://www.amazon.com/gp/product/B00HEDRHG6/) is connected, and optionally specify a pin numbering system. The default is `PinSystem.GPIO`, for Broadcom GPIO numbers. Optionally you may use:

* `PinSystem.PHYS`: physical pin numbers on the P1 connector (1-40) or Rev. 2 P5 connector (53-56 for P5 3-6) (string suffix `p`)
* `PinSystem.WIRING_PI`: WiringPi pin numbers (string suffix `w`)

_For more information see: http://wiringpi.com/reference/setup/_

You can also specify the pin and pin system together as a string value, such as `'13p'`, which is physical pin 13 on the P1 connector.
```
addSensorDataListener(pin: number | string, callback: HtSensorDataCallback): number;
addSensorDataListener(pin: number, pinSystem: PinSystem, callback: HtSensorDataCallback): number;
```

The function returns a numeric ID which can be used by the function below to unregister your callback.

### removeSensorDataListener

```
removeSensorDataListener(callbackId: number): void;
```
