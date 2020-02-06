#ifndef WIRING_PI_FAKE
#define WIRING_PI_FAKE

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#define INPUT 0
#define INT_EDGE_BOTH 0
#define LOW 0
#define HIGH 1

static const int WPI_SHORT_PULSE =        210;
static const int WPI_LONG_PULSE =         401;
static const int WPI_PRE_LONG_SYNC =      207;
static const int WPI_LONG_SYNC_PULSE =   2205;
static const int WPI_SHORT_SYNC_PULSE =   606;

static const int WPI_MESSAGE_RATE = 15; // seconds

static unsigned long wpiCurrMicros = 0;
static bool wpiRunning = false;
static bool wpiPinHigh = false;
static int wpiChannels[] = { 0x3, 0x2, 0x0 };
static int wpiLastHumidity[] = { 50, 40, 30 };
static int wpiLastTemp[] = { 1020, 1120, 1220 };

typedef struct {
  int pin;
  void (*callback)();
} WPI_PinCallback;

static std::vector<WPI_PinCallback> wpiCallbacks;

static int wpiApplyParity(int b) {
  int b0 = b;
  int sum = 0;

  for (int i = 0; i < 7; ++i) {
    sum += b & 1;
    b >>= 1;
  }

  return b0 + (sum % 2 == 1 ? 0x80 : 0);
}

static void wpiSendPulse(int duration) {
  std::this_thread::sleep_for(std::chrono::microseconds(duration));
  wpiPinHigh ^= true;
  wpiCurrMicros += duration;

  for (auto pcb : wpiCallbacks)
    pcb.callback();
}

static void wpiSendByte(int b) {
  for (int i = 0; i < 8; ++i) {
    if ((b & 0x80) != 0) {
      wpiSendPulse(WPI_LONG_PULSE);
      wpiSendPulse(WPI_SHORT_PULSE);
    }
    else {
      wpiSendPulse(WPI_SHORT_PULSE);
      wpiSendPulse(WPI_LONG_PULSE);
    }

    b <<= 1;
  }
}

static void wpiSendForChannel(int channel, int index) {
  if (index == 0) {
    wpiSendPulse(WPI_SHORT_PULSE);
    wpiSendPulse(WPI_LONG_PULSE);
  }

  wpiSendPulse(WPI_PRE_LONG_SYNC);
  wpiSendPulse(WPI_LONG_SYNC_PULSE);

  for (int i = 0; i < 8; ++i)
    wpiSendPulse(WPI_SHORT_SYNC_PULSE);

  int bytes[7] = { 0 };
  int humidity =  std::min(std::max(wpiLastHumidity[index] + std::rand() % 3 - 1, 45 - index * 10), 55 - index * 10);
  int temp = std::min(std::max(wpiLastTemp[index] + std::rand() % 3 - 1, 980 + index * 100), 1060 + index * 100);

  wpiLastHumidity[index] = humidity;
  wpiLastTemp[index] = temp;

  bytes[0] = channel << 6;
  bytes[3] = wpiApplyParity(humidity);
  bytes[4] = wpiApplyParity(temp >> 7);
  bytes[5] = wpiApplyParity(temp & 0x7F);

  for (int i = 0; i < 6; ++i)
    bytes[6] += bytes[i]; // compute checksum

  // Occasionally toss in a random bad bit
  if (std::rand() % 10 == 0) {
    int badBit = 2 + std::min(std::rand() % 78, 53); // Increase odds for bad checksum over bad parity, don't mess up channel.
    bytes[badBit / 8] ^= 1 << (badBit % 8);
  }

  for (int i = 0; i < 7; ++i)
    wpiSendByte(bytes[i]);

  wpiSendPulse(WPI_PRE_LONG_SYNC);
  wpiSendPulse(WPI_LONG_SYNC_PULSE);

  for (int i = 0; i < 8; ++i)
    wpiSendPulse(WPI_SHORT_SYNC_PULSE);
}

static void wpiSendSignals() {
  std::thread([]() {
    while (wpiRunning) {
      for (int i = 0; i < 3; ++i)
        wpiSendForChannel(wpiChannels[i], i);

      std::this_thread::sleep_for(std::chrono::seconds(WPI_MESSAGE_RATE));
      wpiCurrMicros += WPI_MESSAGE_RATE * 1000000;
    }
  }).detach();
}

static int wiringPiSetup() {
  return 0;
}

static int wiringPiSetupGpio() { return wiringPiSetup(); }
static int wiringPiSetupPhys() { return wiringPiSetup(); }
static int wiringPiSetupSys() { return wiringPiSetup(); }

static int digitalRead(int dataPin) {
  return wpiPinHigh ? LOW : HIGH;
}

static void pinMode(int dataPin, int mode) {
  if (mode == INPUT) {
    auto it = wpiCallbacks.begin();

    while (it != wpiCallbacks.end()) {
      if (it->pin == dataPin)
        it = wpiCallbacks.erase(it);
      else
        ++it;
    }

    if (wpiRunning && wpiCallbacks.size() == 0)
      wpiRunning = false;
  }
}

static void wiringPiISR(int dataPin, int mode, void (*callback)()) {
  wpiCallbacks.push_back(WPI_PinCallback { dataPin, callback });

  if (!wpiRunning && wpiCallbacks.size() > 0) {
    wpiRunning = true;
    wpiSendSignals();
  }
}

static unsigned long micros() {
  return wpiCurrMicros;
}

#endif
