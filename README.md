# rpi-acu-rite-temperature

## Library for reading the Acu-Rite 06002M Wireless Temperature and Humidity Sensor using a Raspberry Pi with a 433Mhz receiver

_Note: This software is neither produced by nor endorsed by [Acu-Rite](https://acu-rite.com/site/). The author of this library is not associated with Acu-Rite or its products in any way, apart from finding their [433 MHz remote humidity/temperature sensors](https://www.amazon.com/gp/product/B00T0K8NXC/) useful for this kind of project._

_This library was inspired by work originally done by [Ray Wang](http://rayshobby.net/?p=8998)._

`rpi-acu-rite-temperature` can be used for JavaScript/TypeScript programming in a Node.js environment, or the included C++ code can be used directly. It is the JavaScript/TypeScript interface that is documented here.

Full functionality requires a Raspberry Pi, but this code can be installed and compiled under MacOS or Windows in such a way that it returns simulated data for testing and development use.

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
  channel: string;       // A, B, C, or - (dash) when dead air detected
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
`channel` is either the single-letter channel identifier A, B, or C, or it's a `-` (dash), indicating that no signal at all (not even radio noise) has been detected for a minute or more. Since an RF receiver module typically outputs a lot of random noise even when it isn't receiving a valid transmission, “dead air” generally means that you've selected the wrong pin number, or the RF module is disconnected,  or it is otherwise not functioning.

Even after a dead air indication has been received, and before signal has been regained, you might receive a few more updates for channels A, B, or C. These are delivered while the signal quality rating of each channel degrades toward zero during the absence of reception.

`humidity` will be `undefined` if the signal was decoded as having a value greater than 100.

`rawTemp` will be `undefined`, and `tempCelsius` and `tempFahrenheit` as well, if the signal was decoded with a value outside of the range ±60°C.

`signalQuality` is measured over a five minute window, and may register low even for a strong signal until a full five minutes have passed.

It's best for `validChecksum` to be `true`, but the data provided has at least been validated by three parity bits even if the checksum doesn't come out right. When a weak signal makes updates infrequent, it may be possible, with care, to use somewhat questionable data.

### addSensorDataListener

```
addSensorDataListener(pin: number | string, callback: HtSensorDataCallback): number;
addSensorDataListener(pin: number, pinSystem: PinSystem, callback: HtSensorDataCallback): number;
```

This function is used to register a callback that receives the above temperature/humidity data. You must specify the input `pin` to which your [433 MHz RF receiver](https://www.amazon.com/gp/product/B00HEDRHG6/) is connected, and optionally specify a pin numbering system. The default is `PinSystem.GPIO`, for Broadcom GPIO numbers. Optionally you may use:

* `PinSystem.PHYS`: physical pin numbers on the P1 connector (1-40) or the Rev. 2 P5 connector (53-56 for P5 3-6) (string suffix `p`)
* `PinSystem.WIRING_PI`: WiringPi pin numbers (string suffix `w`)
* `PinSystem.VIRTUAL`: _Same as WiringPi_ (string suffix `v`)

_For more information see: http://wiringpi.com/reference/setup/_

You can also specify the pin and pin system together as a string value, such as `'13p'`, which is physical pin 13 on the P1 connector.

The function returns a numeric ID which can be used by the function below to unregister your callback.

### removeSensorDataListener

```
removeSensorDataListener(callbackId: number): void;
```

### convertPin

This is a utility function for converting between Raspberry Pi pin numbering systems. You can:

1. pass a numeric pin number, the pin system of that pin number, and the pin system you wish to convert to.
1. pass a numeric GPIO number and the pin system you wish to convert to.
1. pass a pin number and pin system as one combined string, followed by the pin system you wish to convert to. The pin system for the original pin number is specified by an appended letter (`g`, `p`, `w`, or `v`, as described above), defaulting to GPIO if no letter is provided.

```
export function convertPin(pin: number, pinSystemFrom: PinSystem, pinSystemTo: PinSystem): number;
export function convertPin(gpioPin: number, pinSystemTo: PinSystem): number;
export function convertPin(pin: string, pinSystemTo: PinSystem): number;
```
