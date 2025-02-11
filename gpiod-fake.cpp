#include "gpiod-fake.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <iostream>
#if defined(WIN32) || defined(WINDOWS)
#define NOMINMAX
#include <Windows.h>
#include <sync hapi.h>
static int pgfPendingMicros = 0;
#endif

#ifndef PI_LOW
#define PI_LOW  GPIOD_CTXLESS_EVENT_CB_FALLING_EDGE
#define PI_HIGH GPIOD_CTXLESS_EVENT_CB_RISING_EDGE
#endif

using namespace std;

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
  unsigned int pin;
  gpiod_ctxless_event_handle_cb callback;
  void *miscData;
} PGF_PinAlert;

static vector<PGF_PinAlert> pgfCallbacks;

static void pgfMicroSleep(int micros) {
#if defined(WIN32) || defined(WINDOWS)
  pgfPendingMicros += micros;
  int hundredths = pgfPendingMicros / 10000;

  if (hundredths > 0) {
    pgfPendingMicros -= hundredths * 10000;
    SleepEx(hundredths * 10, false);
  }
#else
  this_thread::sleep_for(chrono::microseconds(micros));
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
  struct timespec ts;

  pgfMicroSleep(duration);
  pgfPinHigh ^= true;
  pgfCurrMicros += duration;
  ts.tv_sec = pgfCurrMicros / 1000000;
  ts.tv_nsec = pgfCurrMicros * 1000 % 1000000000;

  for (auto pcb : pgfCallbacks) {
    if (pcb.pin != 0)
      pcb.callback(pgfPinHigh ? PI_LOW : PI_HIGH, pcb.pin, &ts, pcb.miscData);
  }
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
  int humidity =  min(max(pgfLastHumidity[index] + rand() % 3 - 1, 45 - index * 10), 55 - index * 10);
  int temp = min(max(pgfLastTemp[index] + rand() % 3 - 1, 980 + index * 100), 1060 + index * 100);

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
  if (rand() % (index == 0 ? 3 : 10) == 0) {
    // Increase odds for bad checksum over bad parity, and don't mess up channel bits.
    int badBit = 2 + rand() % 78;
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
  thread([]() {
    while (pgfRunning) {
      for (int i = 0; i < 3; ++i) {
        if (i != 2 || pgfCurrMicros < 150'000'000) // Channel C cuts out after 2.5 minutes
          pgfSendForChannel(pgfChannels[i], i);
      }

      this_thread::sleep_for(chrono::seconds(PGF_MESSAGE_RATE));
      pgfCurrMicros += PGF_MESSAGE_RATE * 1000000;
    }
  }).detach();
}


int gpiod_ctxless_event_monitor(const char* device, int event_type, unsigned int dataPin, bool active_low,
    const char* consumer, const timespec* timeout, gpiod_ctxless_event_poll_cb poll_cb,
    gpiod_ctxless_event_handle_cb event_cb, void* miscData) {
  auto match = find_if(pgfCallbacks.begin(), pgfCallbacks.end(),
    [dataPin](PGF_PinAlert pcb) { return pcb.pin == dataPin; });

  if (event_cb == nullptr) {
    if (match != pgfCallbacks.end())
      pgfCallbacks.erase(match);
  }
  else if (match != pgfCallbacks.end())
    throw "Pin callback already in use";

  if (event_cb != nullptr)
    pgfCallbacks.push_back(PGF_PinAlert { dataPin, event_cb, miscData });

  if (!pgfRunning && pgfCallbacks.size() > 0) {
    pgfRunning = true;
    pgfSendSignals();
  }
  else if (pgfRunning && pgfCallbacks.size() == 0)
    pgfRunning = false;

  return 0;
}

void fakeGpiodInit() {
  srand((unsigned int) chrono::duration_cast<chrono::milliseconds>
    (chrono::system_clock::now().time_since_epoch()).count());
}
