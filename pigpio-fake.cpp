#include "pigpio-fake.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#if defined(WIN32) || defined(WINDOWS)
#define NOMINMAX
#include <Windows.h>
#include <synchapi.h>
static int pgfPendingMicros = 0;
#endif

#define PGF_SUPPRESS_UNUSED_WARN(fn) void *_pgf_suw_##fn = ((void *) fn);

static const int PGF_SHORT_PULSE =        210;
static const int PGF_LONG_PULSE =         401;
static const int PGF_PRE_LONG_SYNC =      207;
static const int PGF_LONG_SYNC_PULSE =   2205;
static const int PGF_SHORT_SYNC_PULSE =   606;

static const int PGF_MESSAGE_RATE = 15; // seconds

static uint32_t pgfCurrMicros = 0;
static bool pgfRunning = false;
static bool pgfPinHigh = false;
static int pgfChannels[] = { 0x3, 0x2, 0x0 };
static int pgfLastHumidity[] = { 50, 40, 30 };
static int pgfLastTemp[] = { 1020, 1120, 1220 };

typedef struct {
  int pin;
  void (*callback)(int dataPin, int level, unsigned int tick, void *userData);
  void *miscData;
} PGF_PinAlert;

static std::vector<PGF_PinAlert> pgfCallbacks;

static void pgfMicroSleep(int micros) {
#if defined(WIN32) || defined(WINDOWS)
  pgfPendingMicros += micros;
  int hundredths = pgfPendingMicros / 10000;

  if (hundredths > 0) {
    pgfPendingMicros -= hundredths * 10000;
    SleepEx(hundredths * 10, false);
  }
#else
  std::this_thread::sleep_for(std::chrono::microseconds(micros));
#endif
}

static int pgfApplyParity(int b) {
  int b0 = b;
  int sum = 0;

  for (int i = 0; i < 7; ++i) {
    sum += b & 1;
    b >>= 1;
  }

  return b0 + (sum % 2 == 1 ? 0x80 : 0);
}

static void pgfSendPulse(int duration) {
  pgfMicroSleep(duration);
  pgfPinHigh ^= true;
  pgfCurrMicros += duration;

  for (auto pcb : pgfCallbacks)
    pcb.callback(pcb.pin, pgfPinHigh ? PI_LOW : PI_HIGH, pgfCurrMicros, pcb.miscData);
}

static void pgfSendByte(int b) {
  for (int i = 0; i < 8; ++i) {
    if ((b & 0x80) != 0) {
      pgfSendPulse(PGF_LONG_PULSE);
      pgfSendPulse(PGF_SHORT_PULSE);
    }
    else {
      pgfSendPulse(PGF_SHORT_PULSE);
      pgfSendPulse(PGF_LONG_PULSE);
    }

    b <<= 1;
  }
}

static void pgfSendForChannel(int channel, int index) {
  if (index == 0) {
    pgfSendPulse(PGF_SHORT_PULSE);
    pgfSendPulse(PGF_LONG_PULSE);
  }

  pgfSendPulse(PGF_PRE_LONG_SYNC);
  pgfSendPulse(PGF_LONG_SYNC_PULSE);

  for (int i = 0; i < 8; ++i)
    pgfSendPulse(PGF_SHORT_SYNC_PULSE);

  int bytes[7] = { 0 };
  int humidity =  std::min(std::max(pgfLastHumidity[index] + std::rand() % 3 - 1, 45 - index * 10), 55 - index * 10);
  int temp = std::min(std::max(pgfLastTemp[index] + std::rand() % 3 - 1, 980 + index * 100), 1060 + index * 100);

  pgfLastHumidity[index] = humidity;
  pgfLastTemp[index] = temp;

  bytes[0] = channel << 6;
  bytes[3] = pgfApplyParity(humidity);
  bytes[4] = pgfApplyParity(temp >> 7);
  bytes[5] = pgfApplyParity(temp & 0x7F);

  for (int i = 0; i < 6; ++i)
    bytes[6] += bytes[i]; // compute checksum

  bytes[6] &= 0xFF;

  // Occasionally toss in a random bad bit, with channel A being the noisiest.
  if (std::rand() % (index == 0 ? 3 : 10) == 0) {
    // Increase odds for bad checksum over bad parity, and don't mess up channel bits.
    int badBit = 2 + std::rand() % 78;
    badBit = (badBit < 55 ? badBit : 48 + badBit % 8);
    bytes[badBit / 8] ^= 0x80 >> (badBit % 8);
  }

  for (int i = 0; i < 7; ++i)
    pgfSendByte(bytes[i]);

  pgfSendPulse(PGF_PRE_LONG_SYNC);
  pgfSendPulse(PGF_LONG_SYNC_PULSE);

  for (int i = 0; i < 8; ++i)
    pgfSendPulse(PGF_SHORT_SYNC_PULSE);
}

static void pgfSendSignals() {
  std::thread([]() {
    while (pgfRunning) {
      for (int i = 0; i < 3; ++i)
        pgfSendForChannel(pgfChannels[i], i);

      std::this_thread::sleep_for(std::chrono::seconds(PGF_MESSAGE_RATE));
      pgfCurrMicros += PGF_MESSAGE_RATE * 1000000;
    }
  }).detach();
}

int gpioInitialise() {
  std::srand((unsigned int) std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count());

  return 0;
}
PGF_SUPPRESS_UNUSED_WARN(gpioInitialise)

void gpioSetMode(int dataPin, int mode) {
  // do nothing
}
PGF_SUPPRESS_UNUSED_WARN(gpioSetMode)

void gpioGlitchFilter(int pin, int time) {
  // do nothing
}
PGF_SUPPRESS_UNUSED_WARN(gpioGlitchFilter)

void gpioTerminate() {
  // do nothing
}
PGF_SUPPRESS_UNUSED_WARN(gpioTerminate)

void gpioSetAlertFuncEx(int dataPin,
    void (*callback)(int dataPin, int level, unsigned int tick, void *userData), void *miscData) {
  auto match = std::find_if(pgfCallbacks.begin(), pgfCallbacks.end(),
    [dataPin](PGF_PinAlert pcb) { return pcb.pin == dataPin; });

  if (callback == nullptr) {
    if (match != pgfCallbacks.end())
      pgfCallbacks.erase(match);
  }
  else if (match != pgfCallbacks.end())
    throw "Pin callback already in use";

  if (callback != nullptr)
    pgfCallbacks.push_back(PGF_PinAlert { dataPin, callback, miscData });

  if (!pgfRunning && pgfCallbacks.size() > 0) {
    pgfRunning = true;
    pgfSendSignals();
  }
  else if (pgfRunning && pgfCallbacks.size() == 0)
    pgfRunning = false;
}

void gpioSetAlertFunc(int dataPin,
    void (*callback)(int dataPin, int level, unsigned int tick, void *userData)) {
  gpioSetAlertFuncEx(dataPin, callback, nullptr);
}
PGF_SUPPRESS_UNUSED_WARN(gpioSetAlertFunc)

uint32_t gpioTick() {
  return pgfCurrMicros;
}
PGF_SUPPRESS_UNUSED_WARN(gpioTick)
