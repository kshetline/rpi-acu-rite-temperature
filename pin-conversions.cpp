#include "pin-conversions.h"

#include <fstream>
#include <iostream>
#include <regex>

#define PCONV_SUPPRESS_UNUSED_WARN(fn) void *_pconv_suw_##fn = ((void *) fn);

using namespace std;

// Pin conversion tables below taken from Gordon Henderson's WiringPi

// Revision 1, 1.1:

static int wpiToGpioR1[64] =
{
  17, 18, 21, 22, 23, 24, 25, 4,  // From the Original Wiki - GPIO 0 through 7: wpi 0 - 7
   0,  1,       // I2C  - SDA1, SCL1        wpi  8 -  9
   8,  7,       // SPI  - CE1, CE0          wpi 10 - 11
  10,  9, 11,   // SPI  - MOSI, MISO, SCLK  wpi 12 - 14
  14, 15,       // UART - Tx, Rx            wpi 15 - 16

// Padding:

      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
};

// Revision 2:

static int wpiToGpioR2[64] =
{
  17, 18, 27, 22, 23, 24, 25, 4,  // From the Original Wiki - GPIO 0 through 7: wpi  0 - 7
   2,  3,       // I2C  - SDA0, SCL0                    wpi  8 - 9
   8,  7,       // SPI  - CE1, CE0                      wpi 10 - 11
  10,  9, 11,   // SPI  - MOSI, MISO, SCLK              wpi 12 - 14
  14, 15,       // UART - Tx, Rx                        wpi 15 - 16
  28, 29, 30, 31,     // Rev 2: New GPIOs 8 though 11   wpi 17 - 20
   5,  6, 13, 19, 26, // B+                             wpi 21, 22, 23, 24, 25
  12, 16, 20, 21,     // B+                             wpi 26, 27, 28, 29
   0,  1,             // B+                             wpi 30, 31

// Padding:

  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
};

static int* wpiToGpio = wpiToGpioR2;

// physToGpio:
//  Take a physical pin (1 through 26/40) and re-map it to the BCM_GPIO pin
//  Cope for 2 different board revisions here.
//  For P5 connector, P5 pin numbers are offset by 50, i.e. 3, 4, 5, 6 => 53, 54, 55, 56

static int physToGpioR1[64] =
{
  -1,     // 0
  -1, -1, // 1, 2
   0, -1,
   1, -1,
   4, 14,
  -1, 15,
  17, 18,
  21, -1,
  22, 23,
  -1, 24,
  10, -1,
   9, 25,
  11,  8,
  -1,  7, // 25, 26

                                              -1, -1, -1, -1, -1, // ... 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
};

static int physToGpioR2[64] =
{
  -1,     // 0
  -1, -1, // 1, 2
   2, -1,
   3, -1,
   4, 14,
  -1, 15,
  17, 18,
  27, -1,
  22, 23,
  -1, 24,
  10, -1,
   9, 25,
  11,  8,
  -1,  7, // 25, 26

// B+:

   0,  1, // 27, 18
   5, -1,
   6, 12,
  13, -1,
  19, 16,
  26, 20,
  -1, 21, // 39, 40

// Filler:

  -1, -1,
  -1, -1,
  -1, -1,
  -1, -1,
  -1, -1,

  // P5 connector on Rev 2 boards:

   // Note: The original code had GPIO 28 and 29 here, 30 and 31 on the next line,
   // mapping positions 51-54 to P5 3-6. I believe this was an error, and moved
   // the GPIO numbers forward by two positions accordingly.
    -1, -1,
    28, 29, // 53, 54 (P5-3, P5-4)
    30, 31, // 55, 56 (P5-5, P5-6)
    -1, -1,

    // Filler:

      -1, -1,
      -1, -1,
      -1
};

static int* physToGpio = physToGpioR2;

enum GpioLayout {
    UNCHECKED,
    LAYOUT_1, // A, B, Rev 1, 1.1
    LAYOUT_2, // A2, B2, A+, B+, CM, Pi2, Pi3, Pi4, Zero
    UNKNOWN
};

#ifdef USE_FAKE_GPIOD
static GpioLayout gpioLayout = GpioLayout::LAYOUT_2;
static bool supportPhysPins = true;
#else
static GpioLayout gpioLayout = GpioLayout::UNCHECKED;
static bool supportPhysPins = false;
#endif

static void getLayout() {
  if (gpioLayout == UNCHECKED) {
    gpioLayout = UNKNOWN;

    ifstream revFile("/proc/cpuinfo");

    if (revFile && revFile.is_open()) {
      string line;
      smatch match;
      regex revPattern("^Revision\\s*:\\s*\\w*(\\w{4})$");

      while (getline(revFile, line)) {
        if (regex_match(line, match, revPattern)) {
          string revLast4 = match.str(1);

          if (revLast4 == "0002" || revLast4 == "0003") {
            gpioLayout = LAYOUT_1;
            wpiToGpio = wpiToGpioR1;
            physToGpio = physToGpioR1;
          }
          else
            gpioLayout = LAYOUT_2;

          supportPhysPins = true;
          break;
        }
      }

      revFile.close();
    }
  }
}

static bool convertInit = false;
static int gpioToPhys[32] = {-1};
static int gpioToWPi[32] = {-1};

static void getConversions() {
  getLayout();

  if (convertInit || gpioLayout == UNKNOWN)
    return;

  for (int i = 0; i < 64; ++i) {
    int gpio = wpiToGpio[i];

    if (gpio >= 0)
      gpioToWPi[gpio] = i;

    gpio = physToGpio[i];

    if (gpio >= 0)
      gpioToPhys[gpio] = i;
  }

  convertInit = true;
}

int convertPin(int pinNumber, PinSystem pinSysFrom, PinSystem pinSysTo) {
  getConversions();

  if (!supportPhysPins && (pinSysFrom == PHYS || pinSysTo == PHYS))
    throw "Unknown hardware - physical pin numbering not supported";
  else if (pinNumber < 0 || pinNumber > 63 || (pinSysFrom != PHYS && pinNumber > 31))
    return -1;

  int gpio;

  switch (pinSysFrom) {
    case GPIO:
      switch (pinSysTo) {
        case GPIO: return pinNumber;
        case PHYS: return gpioToPhys[pinNumber];
        case WIRING_PI: return gpioToWPi[pinNumber];
        break;
      }
      break;

    case PHYS:
      switch (pinSysTo) {
        case GPIO: return physToGpio[pinNumber];
        case PHYS: return pinNumber;
        case WIRING_PI: return (gpio = physToGpio[pinNumber]) >= 0 ? gpioToWPi[gpio] : -1;
        break;
      }
      break;

    case WIRING_PI:
      switch (pinSysTo) {
        case GPIO: return wpiToGpio[pinNumber];
        case PHYS: return (gpio = wpiToGpio[pinNumber]) >= 0 ? gpioToPhys[gpio] : -1;
        case WIRING_PI: return pinNumber;
        break;
      }
  }

  return -1;
}

int convertPinToGpio(int pinNumber, PinSystem pinSys) {
  return convertPin(pinNumber, pinSys, GPIO);
}
PCONV_SUPPRESS_UNUSED_WARN(convertPinToGpio)
