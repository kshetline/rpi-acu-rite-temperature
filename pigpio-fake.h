#ifndef PIGPIO_FAKE
#define PIGPIO_FAKE

#include <stdint.h>

#define PI_INIT_FAILED -1
#define PI_INPUT 0
#define PI_LOW 0
#define PI_HIGH 1

int gpioInitialise();
void gpioSetMode(int dataPin, int mode);
void gpioGlitchFilter(int pin, int time);
void gpioTerminate();
void gpioSetAlertFuncEx(int dataPin,
    void (*callback)(int dataPin, int level, unsigned int tick, void *userData), void *miscData);
void gpioSetAlertFunc(int dataPin,
    void (*callback)(int dataPin, int level, unsigned int tick, void *userData));
uint32_t gpioTick();

#endif
