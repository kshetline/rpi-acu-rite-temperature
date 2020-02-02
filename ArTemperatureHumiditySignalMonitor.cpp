/*
 * ArTemperatureHumiditySignalMonitor.cpp
 *
 * Copyright 2020 Kerry Shetline <kerry@shetline.com>
 *
 * This code is derived from code originally written by Ray Wang
 * (Rayshobby LLC), http://rayshobby.net/?p=8998
 *
 * MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "ArTemperatureHumiditySignalMonitor.h"

#include <functional>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <tgmath.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>

using namespace std;

#define ARTHSM ArTemperatureHumiditySignalMonitor

// Signal timings in microseconds
const int MESSAGE_LENGTH =   39052; // 56 data bits plus short sync high, long sync low
// 0 bit is short high followed by long low, 1 bit is long high, short low.
const int SHORT_PULSE =        210;
const int LONG_PULSE =         401;
const int BIT_LENGTH =         SHORT_PULSE + LONG_PULSE;
const int PRE_LONG_SYNC =      207;
const int LONG_SYNC_PULSE =   2205;
const int SHORT_SYNC_PULSE =   606;
const int TOLERANCE =          100;
const int LONG_SYNC_TOL =      250;

const int TOTAL_BITS =            56;
const int MIN_TRANSITIONS =       TOTAL_BITS * 2;
const int IDEAL_TRANSITIONS =     MIN_TRANSITIONS + 2; // short sync high, long sync low
const int MAX_TRANSITONS =        IDEAL_TRANSITIONS + 4; // small allowance for spurious noises

const int MIN_MESSAGE_LENGTH       = TOTAL_BITS * (SHORT_PULSE + LONG_PULSE) - TOLERANCE;
const int MAX_MESSAGE_LENGTH       = MESSAGE_LENGTH + TOLERANCE;

const int CHANNEL_FIRST_BIT =      0;
const int CHANNEL_LAST_BIT =       1;

const int MISC_DATA_1_FIRST_BIT =  2;
const int MISC_DATA_1_LAST_BIT =  15;

const int BATTERY_LOW_BIT =       16;

const int MISC_DATA_2_FIRST_BIT = 17;
const int MISC_DATA_2_LAST_BIT =  23;

const int HUMIDITY_FIRST_BIT =    25;
const int HUMIDITY_LAST_BIT =     31;

const int MISC_DATA_3_FIRST_BIT = 33;
const int MISC_DATA_3_LAST_BIT =  35;

const int TEMPERATURE_FIRST_BIT = 36;
const int TEMPERATURE_LAST_BIT =  47;

const int MAX_MONITORS = 10;

const int REPEAT_SUPRESSION    =  60; // 1 minute
const int REUSE_OLD_DATA_LIMIT = 600; // 10 minutes
 
bool ARTHSM::initialSetupDone = false;
ARTHSM::PinSystem ARTHSM::pinSystem = (ARTHSM::PinSystem) -1;

ARTHSM* monitors[MAX_MONITORS] = { NULL };
extern void (*smCallbacks[MAX_MONITORS])();

int mod(int x, int y) {
  int m = x % y;

  if ((m < 0 && y > 0) || (m > 0 && y < 0)) {
    return y + m;
  }

  return m;
}

ARTHSM::ArTemperatureHumiditySignalMonitor() {
  dispatchLock = new mutex();
  signalLock = new mutex();
}

ARTHSM::~ArTemperatureHumiditySignalMonitor() {
  if (callbackIndex >= 0)
    monitors[callbackIndex] = NULL;

  delete dispatchLock;
  delete signalLock;
}

void ARTHSM::init(int dataPin) {
  init(dataPin, PinSystem::VIRTUAL);
}

void ARTHSM::init(int dataPin, PinSystem pinSys) {
  if (pinSystem >= PinSystem::SYS && pinSys != pinSystem)
    throw "WiringPi pin numbering system cannot be changed";

  if (!initialSetupDone) {
    pinSystem = pinSys;

    int setUpResult;

    switch (pinSystem) {
      case PinSystem::VIRTUAL: setUpResult = wiringPiSetup(); break;
      case PinSystem::GPIO:    setUpResult = wiringPiSetupGpio(); break;
      case PinSystem::PHYS:    setUpResult = wiringPiSetupPhys(); break;
      case PinSystem::SYS:     setUpResult = wiringPiSetupSys(); break;
    }

    if (setUpResult < 0)
      throw "WiringPi could not be set up";

    initialSetupDone = true;
  }

  for (int i = 0; i < MAX_MONITORS; ++i) {
    if (monitors[i] == NULL) {
      callbackIndex = i;
      monitors[i] = this;
      break;
    }
  }

  if (callbackIndex < 0)
    throw "Maximum number of ArTemperatureHumiditySignalMonitor instances reached";

  this->dataPin = dataPin;
  wiringPiISR(dataPin, INT_EDGE_BOTH, smCallbacks[callbackIndex]);
}

void ARTHSM::signalHasChanged(ARTHSM *sm) {
  sm->signalHasChangedAux();
}

void ARTHSM::signalHasChangedAux() {
  unsigned long now = micros();

  signalLock->lock();

  unsigned short duration = min(now - lastSignalChange, 10000ul);

  lastSignalChange = now;
  timingIndex = (timingIndex + 1) % RING_BUFFER_SIZE;
  timings[timingIndex] = duration;

  if (syncCount == 0 && isStartSyncAcquired()) {
    ++syncCount;
    syncIndex1 = (timingIndex + 1) % RING_BUFFER_SIZE;
    frameStartTime = now;
  }
  else if (syncCount == 1 && isEndSyncAcquired()) {
    syncCount = 0;
    syncIndex2 = (timingIndex + 1) % RING_BUFFER_SIZE;

    int messageTime = now - frameStartTime;
    int changeCount = mod(syncIndex2 - syncIndex1, RING_BUFFER_SIZE);

    if (MIN_TRANSITIONS <= changeCount && changeCount <= MAX_TRANSITONS &&
        MIN_MESSAGE_LENGTH <= messageTime && messageTime <= MAX_MESSAGE_LENGTH)
    {
      processMessage(now);
    }
  }

  signalLock->unlock();
}

void ARTHSM::enableDebugOutput(bool state) {
  debugOutput = state;
}

string ARTHSM::getBitsAsString() {
  string s;

  for (int i = 0; i < 56; ++i) {
    if (i > 0 && i % 8 == 0)
      s += ' ';

    int bit = getBit(i);

    if (bit == 0)
      s += '0';
    else if (bit == 1)
      s += '1';
    else
      s += '~';
  }

  return s;
}

string getTimestamp() {
  char buf[32];
  struct timeval time;

  gettimeofday(&time, NULL);
  time_t sec = time.tv_sec;
  strftime(buf, 32, "%R:%S.", localtime(&sec));
  sprintf(&buf[9], "%03ld", time.tv_usec / 1000);

  return string(buf);
}

void ARTHSM::processMessage(unsigned long frameEndTime) {
  processMessage(frameEndTime, 0);
}

void ARTHSM::processMessage(unsigned long frameEndTime, int attempt) {
  auto integrity = checkDataIntegrity();
  string allBits = getBitsAsString() + " (" + to_string(frameEndTime - frameStartTime) + "µs)";
#if defined(COLLECT_STATS) || defined(SHOW_RAW_DATA) || defined(SHOW_MARGINAL_DATA)
#define TIMES_ARRAY_ARG , times, changeCount
  int changeCount = mod(syncIndex2 - syncIndex1, RING_BUFFER_SIZE);
  int times[changeCount];
  for (int i = 0; i < changeCount; ++i)
    times[i] = timings[(syncIndex1 + i) % RING_BUFFER_SIZE];
#else
#define TIMES_ARRAY_ARG
#endif

  if (integrity > BAD_PARITY) {
#ifdef COLLECT_STATS
    if (integrity == GOOD) {
      totalMessageTime += (frameEndTime - frameStartTime);
      ++totalMessages;
      for (int i = 0; i < changeCount; ++i) {
        int t = times[i];
        if (i < 112) {
          if (t < LONG_PULSE - TOLERANCE)
            totalShortBitTime += t;
            ++totalShortBits;
          }
          else {
            totalLongBitTime += t;
            ++totalLongBits;
          }
        }
        else if (i == 112) {
          totalPreLongSyncTime += t;
          ++totalPreLongSyncs;
        }
        else if (i == 113) {
          totalLongSyncTime += t;
          ++totalLongSyncs;
        }
        else if (i > 113) {
          totalShortSyncTime += t;
          ++totalShortSyncs;
        }
      }
    }
#endif

    SensorData sd;

    sd.channel = "C?BA"[getInt(CHANNEL_FIRST_BIT, CHANNEL_LAST_BIT)];
    sd.validChecksum = (integrity == GOOD);
    sd.batteryLow = getBit(BATTERY_LOW_BIT);
    sd.miscData1 = getInt(MISC_DATA_1_FIRST_BIT, MISC_DATA_1_LAST_BIT);
    sd.miscData2 = getInt(MISC_DATA_2_FIRST_BIT, MISC_DATA_2_LAST_BIT);
    sd.miscData3 = getInt(MISC_DATA_3_FIRST_BIT, MISC_DATA_3_LAST_BIT);
    sd.collectionTime = frameEndTime / 1000000; // convert to seconds

    int rawHumidity = getInt(HUMIDITY_FIRST_BIT, HUMIDITY_LAST_BIT);
    sd.humidity = rawHumidity > 100 ? -999 : rawHumidity;

    sd.rawTemp = getInt(TEMPERATURE_FIRST_BIT, TEMPERATURE_LAST_BIT, true);
    sd.tempCelsius = (sd.rawTemp - 1000) / 10.0;

    if (abs(sd.tempCelsius) > 100)
      sd.tempCelsius = -999;

    sd.tempFahrenheit = sd.tempCelsius == -999 ? -999 :
      round((sd.tempCelsius * 1.8 + 32.0) * 10.0) / 10.0;

    thread dispatch([this, allBits, sd, attempt TIMES_ARRAY_ARG] {
      dispatchLock->lock();

      if (debugOutput) {
#ifdef SHOW_RAW_DATA
        for (int i = 0; i < changeCount; i += 2) {
          printf("%*d:%*d,%*d", 2, i / 2, 4, times[i], 4, times[i + 1]);
          printf(i == 120 || (i + 2) % 8 == 0 ? "\n" : "   ");
        }
#endif
        cout << allBits << endl << getTimestamp();
        printf("%c channel %c, %d%%, %.1fC (%d raw), %.1fF, battery %s%s\n",
          sd.validChecksum ? ':' : '~', sd.channel,
          sd.humidity, sd.tempCelsius, sd.rawTemp, sd.tempFahrenheit,
          sd.batteryLow ? "LOW" : "good",
          attempt > 0 ? "!" : "");
#ifdef COLLECT_STATS
        printf("Average message time: %.0fµs\n", totalMessageTime / max((double) totalMessages, 1.0));
        printf("sb: %.1fµs, lb: %.1fµs, ps: %.1fµs, ls: %.1fµs, ss: %.1fµs\n",
          totalShortBitTime / max((double) totalShortBits, 1.0),
          totalLongBitTime / max((double) totalLongBits, 1.0),
          totalPreLongSyncTime / max((double) totalPreLongSyncs, 1.0),
          totalLongSyncTime / max((double) totalLongSyncs, 1.0),
          totalShortSyncTime / max((double) totalShortSyncs, 1.0));
#endif
      }

      if (sd.channel != '?') {
        bool doCallback = true;
        bool cacheNewData = true;

        if (lastSensorData.count(sd.channel) > 0) {
          const SensorData lastData = lastSensorData[sd.channel];

          if (sd.collectionTime < lastData.collectionTime + REPEAT_SUPRESSION &&
             sd.hasSameValues(lastData))
            doCallback = cacheNewData = false;

          if (!sd.validChecksum && lastData.validChecksum) {
            cacheNewData = false;

            if (sd.collectionTime < lastData.collectionTime + REUSE_OLD_DATA_LIMIT &&
                sd.hasCloseValues(lastData))
              doCallback = false;
          }
        }

        if (doCallback) {
          auto iterator = clientCallbacks.begin();

          while (iterator != clientCallbacks.end()) {
            iterator->second.first(sd, iterator->second.second);
            ++iterator;
          }
        }

        if (cacheNewData)
          lastSensorData[sd.channel] = sd;
      }

      dispatchLock->unlock();
    });
    dispatch.detach();
  }
  else if (attempt == 0) {
    tryToCleanUpSignal();
    processMessage(frameEndTime, 1);
  }
  else if (debugOutput) {
    thread report([this, allBits TIMES_ARRAY_ARG] {
#ifdef SHOW_MARGINAL_DATA
      int b = 0;
      int tt = 0;
      for (int i = 0; i < changeCount; ++i) {
        int t = times[i];
        if (tt == 0)
          printf("%*d:", 2, b);
        printf(" %d", t);
        tt += t;
        if (tt > 550) {
          ++b;
          tt = 0;
          printf("\n");
        }
        else if (i == changeCount - 1)
          printf("\n");
      }
#endif
      dispatchLock->lock();
      cout << allBits << endl << getTimestamp() << ": Corrupted data\n";
      dispatchLock->unlock();
    });
    report.detach();
  }
}

void ARTHSM::tryToCleanUpSignal() {
  const int totalSubBits = TOTAL_BITS * 3;
  const double subBitDuration = BIT_LENGTH / 3.0;
  double subBits[totalSubBits];
  int highLow = -1;
  int timeOffset = 0;
  int subBitCount = 0;
  double accumulatedTime = 0;
  double accumulatedWeight = 0;
  double availableTime = 0;

  while (subBitCount < totalSubBits) {
    if (availableTime < 0.01) {
      availableTime = timings[mod(syncIndex1 + timeOffset++, RING_BUFFER_SIZE)];
      highLow *= -1;
    }

    double nextTimeChunk = min(availableTime, subBitDuration - accumulatedTime);

    accumulatedTime += nextTimeChunk;
    accumulatedWeight += nextTimeChunk * highLow;
    availableTime -= nextTimeChunk;

    if (abs(accumulatedTime - subBitDuration) < 0.01 ||
       (syncIndex1 + timeOffset) % RING_BUFFER_SIZE == syncIndex2)
    {
      subBits[subBitCount++] = accumulatedWeight;
      accumulatedTime = accumulatedWeight = 0;
    }
  }

  timeOffset = 0;

  int badBit = -1;
  int checksum1 = 0;
  int checksum2 = 0;

  for (int i = 0; i < totalSubBits; i += 3) {
    int bitIndex = i / 3;
    double s0 = subBits[i];
    double s1 = subBits[i + 1];
    double s2 = subBits[i + 2];

    if (s0 > 0 && s1 > 0 && s2 < 0) { // 1 bit
      setTiming(timeOffset++, LONG_PULSE);
      setTiming(timeOffset++, SHORT_PULSE);

      int bitPlaceValue = 1 << (7 - bitIndex % 8);

      if (bitIndex < 48)
        checksum1 += bitPlaceValue;
      else
        checksum2 += bitPlaceValue;
    }
    else if (s0 > 0 && s1 < 0 && s2 < 0) { // 0 bit
      setTiming(timeOffset++, SHORT_PULSE);
      setTiming(timeOffset++, LONG_PULSE);
    }
    else {
      setTiming(timeOffset++, 0); // deliberate bad data
      setTiming(timeOffset++, 0);

      // Attempt to correct only single-bit failures
      if (badBit < 0)
        badBit = bitIndex;
      else {
        badBit = -2;
        break;
      }
    }
  }

  if (badBit >= 0) {
    if (checksum1 == checksum2) { // bad bit is 0
      setTiming(badBit * 2, SHORT_PULSE);
      setTiming(badBit * 2 + 1, LONG_PULSE);
    }
    else {
      int bitPlaceValue = 1 << (7 - badBit % 8);

      if (badBit < 48)
        checksum1 += bitPlaceValue;
      else
        checksum2 += bitPlaceValue;

      if (checksum1 == checksum2) { // bad bit is 1
        setTiming(badBit * 2, LONG_PULSE);
        setTiming(badBit * 2 + 1, SHORT_PULSE);
      }
    }
  }
}

int ARTHSM::addListener(VoidFunctionPtr callback) {
  return addListener(callback, NULL);
}

int ARTHSM::addListener(VoidFunctionPtr callback, void *data) {
  clientCallbacks[++nextClientCallbackIndex] = make_pair(callback, data);

  return nextClientCallbackIndex;
}

void ARTHSM::removeListener(int listenerId) {
  clientCallbacks.erase(listenerId);
}

int ARTHSM::getTiming(int offset) {
  return timings[mod(timingIndex + offset, RING_BUFFER_SIZE)];
}

// NOTE: getTiming() is relative to timingIndex, but setTiming() is relative to syncIndex1.
void ARTHSM::setTiming(int offset, unsigned short value) {
  timings[mod(syncIndex1 + offset, RING_BUFFER_SIZE)] = value;
}

bool ARTHSM::isStartSyncAcquired() {
  if (digitalRead(dataPin) != HIGH)
    return false;

  for (int i = 0; i < 8; i += 2) {
    int t1 = getTiming(-i);
    int t0 = getTiming(-i - 1);

    if (t0 < SHORT_SYNC_PULSE - TOLERANCE || t0 > SHORT_SYNC_PULSE + TOLERANCE ||
        t1 < SHORT_SYNC_PULSE - TOLERANCE || t1 > SHORT_SYNC_PULSE + TOLERANCE) {
      return false;
    }
  }

  return true;
}

bool ARTHSM::isEndSyncAcquired() {
  if (digitalRead(dataPin) != HIGH)
    return false;

  int t = getTiming(0);

  if (t < LONG_SYNC_PULSE - LONG_SYNC_TOL)
    return false;

  t = getTiming(-1);

  if (t < PRE_LONG_SYNC - TOLERANCE || t > PRE_LONG_SYNC + TOLERANCE)
    return false;

  return true;
}

int ARTHSM::getBit(int offset) {
  int t0 = timings[mod(syncIndex1 + offset * 2, RING_BUFFER_SIZE)];
  int t1 = timings[mod(syncIndex1 + offset * 2 + 1, RING_BUFFER_SIZE)];

	if (t0 + t1 < SHORT_PULSE + LONG_PULSE + TOLERANCE * 2 &&
      SHORT_PULSE - TOLERANCE < t0 && t0 < LONG_PULSE + TOLERANCE &&
      SHORT_PULSE - TOLERANCE < t1 && t1 < LONG_PULSE + TOLERANCE)
  {
		return t0 > t1 ? 1 : 0;
	}

  return -1;
}

int ARTHSM::getInt(int firstBit, int lastBit) {
  return getInt(firstBit, lastBit, false);
}

int ARTHSM::getInt(int firstBit, int lastBit, bool skipParity) {
  int result = 0;

  for (int i = firstBit; i <= lastBit; ++i) {
    int bit = getBit(i);

    if (bit < 0)
      return -1;
    else if (skipParity && i % 8 == 0)
      continue;

    result = (result << 1) + bit;
  }

  return result;
}

ARTHSM::DataIntegrity ARTHSM::checkDataIntegrity() {
  for (int bit = 0; bit < 56; ++bit) {
    if (getBit(bit) < 0)
      return BAD_BITS;
  }

  // Check parity on the middle three bytes
  for (int byte = 3; byte <= 5; ++byte) {
    int parity = getBit(byte * 8);
    int sum = 0;

    for (int bitIndex = 1; bitIndex <= 7; ++bitIndex)
      sum += getBit(byte * 8 + bitIndex);

    if (sum % 2 != parity)
      return BAD_PARITY;
  }

  // Validate checksum
  int checksum = 0;

  for (int byte = 0; byte <= 5; ++ byte)
    checksum += getInt(byte * 8, byte * 8 + 7);

  return (checksum & 0xFF) == getInt(48, 55) ? GOOD : BAD_CHECKSUM;
}

bool ARTHSM::SensorData::hasSameValues(const SensorData &sd) const {
  // Assumption in made that derived values tempCelsius and tempFahrenheit are
  // consistent with rawTemp.
  return channel == sd.channel &&
         batteryLow == sd.batteryLow &&
         humidity == sd.humidity &&
         rawTemp == sd.rawTemp;
}

bool ARTHSM::SensorData::hasCloseValues(const SensorData &sd) const {
  return channel == sd.channel &&
         batteryLow == sd.batteryLow &&
         abs(humidity - sd.humidity) < 3 &&
         abs(rawTemp - sd.rawTemp) < 30;
}

void smCallback0() { ARTHSM::signalHasChanged(monitors[0]); }
void smCallback1() { ARTHSM::signalHasChanged(monitors[1]); }
void smCallback2() { ARTHSM::signalHasChanged(monitors[2]); }
void smCallback3() { ARTHSM::signalHasChanged(monitors[3]); }
void smCallback4() { ARTHSM::signalHasChanged(monitors[4]); }
void smCallback5() { ARTHSM::signalHasChanged(monitors[5]); }
void smCallback6() { ARTHSM::signalHasChanged(monitors[6]); }
void smCallback7() { ARTHSM::signalHasChanged(monitors[7]); }
void smCallback8() { ARTHSM::signalHasChanged(monitors[8]); }
void smCallback9() { ARTHSM::signalHasChanged(monitors[9]); }

void (*smCallbacks[MAX_MONITORS])() = {
  smCallback0, smCallback1, smCallback2, smCallback3, smCallback4,
  smCallback5, smCallback6, smCallback7, smCallback8, smCallback9
};
