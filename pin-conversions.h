#ifndef PIN_CONVERSIONS
#define PIN_CONVERSIONS

enum PinSystem { GPIO, PHYS, WIRING_PI };

int convertPinToGpio(int pinNumber, PinSystem pinSys);
int convertPin(int pinNumber, PinSystem pinSysFrom, PinSystem pinSysTo);

#endif
