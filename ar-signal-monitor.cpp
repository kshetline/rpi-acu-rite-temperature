/*
 * ar-signal-monitor.cpp
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
 */

#include "ar-signal-monitor.h"

#include <functional>
#include <iomanip>
#include <iostream>
#include "pin-conversions.h"
#include <stdlib.h>
#include <sys/time.h>
#include <tgmath.h>
#include <thread>
#include <time.h>
#include <unistd.h>

using namespace std;

#define ARTHSM ArTemperatureHumiditySignalMonitor

// Signal timings in microseconds
// 0 bit is short high followed by long low, 1 bit is long high, short low.
static const int SHORT_PULSE =         210;
static const int LONG_PULSE =          401;
static const int BIT_LENGTH =          SHORT_PULSE + LONG_PULSE;
static const int THIRD_OF_A_BIT =      BIT_LENGTH / 3;
static const int PRE_LONG_SYNC =       207;
static const int LONG_SYNC_PULSE =    2205;
static const int SHORT_SYNC_PULSE =    606;
static const int TOLERANCE =           100;
static const int LONG_SYNC_TOL =       450;

static const int MESSAGE_BITS =       56;
static const int MIN_TRANSITIONS =    MESSAGE_BITS * 2;
static const int IDEAL_TRANSITIONS =  MIN_TRANSITIONS + 2; // short sync high, long sync low
static const int MAX_TRANSITIONS =    IDEAL_TRANSITIONS + 4; // small allowance for spurious noises
static const int MAX_BAD_BITS =       5;

static const int MESSAGE_LENGTH =    MESSAGE_BITS * (SHORT_PULSE + LONG_PULSE);
static const int SYNC_TO_SYNC_TIME = MESSAGE_LENGTH + PRE_LONG_SYNC + LONG_SYNC_PULSE + SHORT_SYNC_PULSE * 8;
static const int MIN_MESSAGE_LENGTH = MESSAGE_LENGTH - TOLERANCE;
static const int MAX_MESSAGE_LENGTH = SYNC_TO_SYNC_TIME + TOLERANCE;

static const int CHANNEL_FIRST_BIT =      0;
static const int CHANNEL_LAST_BIT =       1;

static const int MISC_DATA_1_FIRST_BIT =  2;
static const int MISC_DATA_1_LAST_BIT =  15;

static const int BATTERY_LOW_BIT =       16;

static const int MISC_DATA_2_FIRST_BIT = 17;
static const int MISC_DATA_2_LAST_BIT =  23;

static const int HUMIDITY_FIRST_BIT =    25;
static const int HUMIDITY_LAST_BIT =     31;

static const int MISC_DATA_3_FIRST_BIT = 33;
static const int MISC_DATA_3_LAST_BIT =  35;

static const int TEMPERATURE_FIRST_BIT = 36;
static const int TEMPERATURE_LAST_BIT =  47;

static const int REPEAT_SUPPRESSION =         60; // 1 minute
static const int CACHE_REPEAT_SUPPRESSION =   60; // 15 seconds
static const int REUSE_OLD_DATA_LIMIT =      600; // 10 minutes

static const int SIGNAL_QUALITY_CHECK_RATE =    90; // 90 seconds
static const int SIGNAL_QUALITY_WINDOW = 300000000; // 5 minutes
static const int DESIRED_SIGNAL_RATE =    30000000; // At least one channel update every 30 seconds
static const int RANK_HIGH  = 4;
static const int RANK_MID   = 2;
static const int RANK_LOW   = 1;
static const int RANK_CHECK = 0;

long ARTHSM::extendedMicroTime = 0;
bool ARTHSM::initialSetupDone = false;
uint32_t ARTHSM::lastMicroTimeU32 = 0;
int ARTHSM::nextClientCallbackIndex = 0;
bool ARTHSM::pinInUse[32] = {false};
int ARTHSM::pinsInUse = 0;

static mutex dispatchLocks[32];
static mutex signalLocks[32];

static int mod(int x, int y) {
  int m = x % y;

  if ((m < 0 && y > 0) || (m > 0 && y < 0)) {
    return y + m;
  }

  return m;
}

ARTHSM::ArTemperatureHumiditySignalMonitor() {
}

ARTHSM::~ArTemperatureHumiditySignalMonitor() {
  if (dataPin >= 0) {
    gpioSetAlertFunc(dataPin, nullptr); // stop pin signal callbacks
    dispatchLocks[dataPin].lock();
    qualityCheckExitSignal.set_value();
    dispatchLocks[dataPin].unlock();
    pinInUse[dataPin] = false;

    if (--pinsInUse == 0 && initialSetupDone) {
      gpioTerminate();
      initialSetupDone = false;
    }
  }
}

void ARTHSM::init(int dataPin) {
  init(dataPin, PinSystem::GPIO);
}

void ARTHSM::init(int dataPin, PinSystem pinSys) {
  dataPin = convertPinToGpio(dataPin, pinSys);

  if (dataPin < 0)
    throw "Invalid pin number";

  if (pinInUse[dataPin])
    throw "Pin already in use";

  if (!initialSetupDone) {
    if (gpioInitialise() == PI_INIT_FAILED)
      throw "WiringPi could not be set up";

    initialSetupDone = true;
  }

  establishQualityCheck();
  this->dataPin = dataPin;
  pinInUse[dataPin] = true;
  ++pinsInUse;
  gpioSetMode(dataPin, PI_INPUT);
  gpioGlitchFilter(dataPin, 100);
  gpioSetAlertFuncEx(dataPin, signalHasChanged, this);
}

int ARTHSM::getDataPin() {
  return dataPin;
}

int ARTHSM::addListener(VoidFunctionPtr callback) {
  return addListener(callback, nullptr);
}

int ARTHSM::addListener(VoidFunctionPtr callback, void *data) {
  clientCallbacks[++nextClientCallbackIndex] = make_pair(callback, data);

  return nextClientCallbackIndex;
}

void ARTHSM::removeListener(int listenerId) {
  clientCallbacks.erase(listenerId);
}

void ARTHSM::enableDebugOutput(bool state) {
  debugOutput = state;
}

long ARTHSM::micros() {
  return micros(gpioTick());
}

long ARTHSM::micros(uint32_t microTimeU32) {
  if (microTimeU32 < lastMicroTimeU32)
    extendedMicroTime += 0x100000000;

  lastMicroTimeU32 = microTimeU32;

  return extendedMicroTime + microTimeU32;
}

int ARTHSM::getTiming(int offset) {
  return timings[mod(timingIndex + offset, RING_BUFFER_SIZE)];
}

// NOTE: getTiming() is relative to timingIndex, but setTiming() is relative to dataIndex.
void ARTHSM::setTiming(int offset, unsigned short value) {
  timings[mod(dataIndex + offset, RING_BUFFER_SIZE)] = value;
}

bool ARTHSM::isZeroBit(int t0, int t1) {
    return (SHORT_PULSE - TOLERANCE < t0 && t0 < SHORT_PULSE + TOLERANCE &&
            LONG_PULSE - TOLERANCE < t1 && t1 < LONG_PULSE + TOLERANCE);
}

bool ARTHSM::isOneBit(int t0, int t1) {
    return (LONG_PULSE - TOLERANCE < t0 && t0 < LONG_PULSE + TOLERANCE &&
            SHORT_PULSE - TOLERANCE < t1 && t1 < SHORT_PULSE + TOLERANCE);
}

bool ARTHSM::isShortSync(int t0, int t1) {
    return (SHORT_SYNC_PULSE - TOLERANCE < t0 && t0 < SHORT_SYNC_PULSE + TOLERANCE &&
            SHORT_SYNC_PULSE - TOLERANCE < t1 && t1 < SHORT_SYNC_PULSE + TOLERANCE);
}

bool ARTHSM::isLongSync(int t0, int t1) {
    return (PRE_LONG_SYNC - TOLERANCE < t0 && t0 < PRE_LONG_SYNC + TOLERANCE &&
            LONG_SYNC_PULSE - LONG_SYNC_TOL < t1 && t1 < LONG_SYNC_PULSE + LONG_SYNC_TOL);
}

bool ARTHSM::isSyncAcquired() {
  int t0, t1;

  for (int i = 0; i < 8; i += 2) {
    t1 = getTiming(-i);
    t0 = getTiming(-i - 1);

    if (!isShortSync(t0, t1)) {
      return false;
    }
  }

  t0 = getTiming(-9);
  t1 = getTiming(-8);

  return isLongSync(t0, t1);
}

int ARTHSM::getBit(int offset) {
  int t0 = timings[mod(dataIndex + offset * 2, RING_BUFFER_SIZE)];
  int t1 = timings[mod(dataIndex + offset * 2 + 1, RING_BUFFER_SIZE)];

  if (isZeroBit(t0, t1))
    return 0;
  else if (isOneBit(t0, t1))
    return 1;

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

void ARTHSM::signalHasChanged(int dataPin, int level, unsigned int tick, void *userData) {
  if ((level != PI_LOW && level != PI_HIGH) || !pinInUse[dataPin])
    return;

  signalLocks[dataPin].lock();

  if (userData != nullptr)
    ((ARTHSM*) userData)->signalHasChangedAux(micros(tick), level);
  else
    signalLocks[dataPin].unlock();
}

void ARTHSM::signalHasChangedAux(long now, int pinState) {
  if (pinState == lastPinState) {
    signalLocks[dataPin].unlock();
    return;
  }

  lastPinState = pinState;

  unsigned short duration = min(now - lastSignalChange, 10000l);

  lastSignalChange = now;
  timingIndex = (timingIndex + 1) % RING_BUFFER_SIZE;
  timings[timingIndex] = duration;

  if (pinState == PI_HIGH) {
    int currentIndex = (timingIndex + 1) % RING_BUFFER_SIZE;

    if (syncTime2 >= 0 && now > syncTime2 + SYNC_TO_SYNC_TIME + LONG_SYNC_TOL &&
        findStartOfTriplet() && combineMessages())
    {
      cout << "Successful triple message blend\n";
      dataIndex = syncIndex2;
      dataEndIndex = currentIndex;
      processMessage(now);
      syncTime1 = syncTime2 = -1;
    }

    bool gotBit = false;
    int t1 = duration;
    int t0 = getTiming(-1);

    if (isZeroBit(t0, t1) || isOneBit(t0, t1)) {
      ++sequentialBits;

      if (sequentialBits == 1) {
        potentialDataIndex = mod(timingIndex - 1, RING_BUFFER_SIZE);
        frameStartTime = now - t0 - t1;
      }
      else if (sequentialBits == MESSAGE_BITS) {
        dataIndex = potentialDataIndex;
        dataEndIndex = mod(timingIndex + 1, RING_BUFFER_SIZE);
        processMessage(now);

        if (sequentialBits != 0) { // Failed as good data?
          --sequentialBits;
          frameStartTime += getTiming(potentialDataIndex) + getTiming(potentialDataIndex + 1);
          potentialDataIndex = (potentialDataIndex + 2) % RING_BUFFER_SIZE;
        }
      }

      gotBit = true;
    }
    else {
      sequentialBits = 0;

      if (!isShortSync(t0, t1) && !isLongSync(t0, t1))
        ++badBits;
    }

    int messageTime = now - frameStartTime;

    if (!gotBit && isSyncAcquired()) {
      if (syncTime1 < 0 || abs(now - syncTime1 - SYNC_TO_SYNC_TIME) < LONG_SYNC_TOL) {
        syncTime1 = now;
        syncIndex1 = currentIndex;
      }
      else {
        syncTime2 = now;
        syncIndex2 = currentIndex;
      }

      int changeCount = mod(currentIndex - dataIndex, RING_BUFFER_SIZE);

      if (dataIndex >= 0 &&
          MIN_TRANSITIONS <= changeCount && changeCount <= MAX_TRANSITIONS &&
          MIN_MESSAGE_LENGTH <= messageTime && messageTime <= MAX_MESSAGE_LENGTH) {
        dataEndIndex = currentIndex;
        processMessage(now);
      }

      dataIndex = currentIndex;
      badBits = 0;
      frameStartTime = now;
    }
    else if (dataIndex >= 0 && badBits < MAX_BAD_BITS &&
             messageTime > MIN_MESSAGE_LENGTH + SHORT_SYNC_PULSE &&
             messageTime < MAX_MESSAGE_LENGTH) {
      dataEndIndex = timingIndex;
      processMessage(now);
      dataIndex = -1;
    }
  }

  signalLocks[dataPin].unlock();
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

  gettimeofday(&time, nullptr);
  time_t sec = time.tv_sec;
  strftime(buf, 32, "%R:%S.", localtime(&sec));
  sprintf(&buf[9], "%03ld", (long) (time.tv_usec / 1000L));

  return string(buf);
}

void ARTHSM::processMessage(long frameEndTime) {
  processMessage(frameEndTime, 0);
}

void ARTHSM::processMessage(long frameEndTime, int attempt) {
  auto integrity = checkDataIntegrity();
  char channel = "?C?BA"[getInt(CHANNEL_FIRST_BIT, CHANNEL_LAST_BIT) + 1];
  string allBits = (debugOutput ? getBitsAsString() + " (" + to_string(frameEndTime - frameStartTime) + "µs)" : "");
  string timestamp = (debugOutput ? getTimestamp() : "");
#if defined(SHOW_RAW_DATA) || defined(SHOW_MARGINAL_DATA)
#define TIMES_ARRAY_ARG , times, changeCount
  int changeCount = mod(dataEndIndex - dataIndex, RING_BUFFER_SIZE);
  int times[changeCount];

  for (int i = 0; i < changeCount; ++i)
    times[i] = timings[(dataIndex + i) % RING_BUFFER_SIZE];
#else
#define TIMES_ARRAY_ARG /* empty */
#endif

  if (integrity > BAD_PARITY) {
    sequentialBits = 0;

    SensorData sd;

    sd.channel = channel;
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

    if (abs(sd.tempCelsius) > 60)
      sd.tempCelsius = -999;

    sd.tempFahrenheit = sd.tempCelsius == -999 ? -999 :
      round((sd.tempCelsius * 1.8 + 32.0) * 10.0) / 10.0;

    sd.signalQuality = updateSignalQuality(sd.channel, frameEndTime,
      sd.validChecksum && sd.humidity != -999 && sd.rawTemp != -999 ? RANK_HIGH : RANK_MID);

    thread([this, allBits, sd, attempt, timestamp TIMES_ARRAY_ARG] {
      dispatchLocks[dataPin].lock();

      if (debugOutput) {
#ifdef SHOW_RAW_DATA
        for (int i = 0; i < changeCount; i += 2) {
          printf("%*d:%*d,%*d", 2, i / 2, 4, times[i], 4, times[i + 1]);
          printf(i == 120 || (i + 2) % 8 == 0 ? "\n" : "   ");
        }
#endif
        cout << allBits << endl << timestamp;
        printf("%c channel %c, %d%%, %.1fC (%d raw), %.1fF, battery %s%s\n",
          sd.validChecksum ? ':' : '~', sd.channel,
          sd.humidity, sd.tempCelsius, sd.rawTemp, sd.tempFahrenheit,
          sd.batteryLow ? "LOW" : "good",
          attempt > 0 ? "^" : "");
      }

      if (sd.channel != '?') {
        int channelActive = lastSensorData.count(sd.channel) > 0;
        bool doCallback = channelActive || sd.validChecksum;
        bool cacheNewData = doCallback;

        if (channelActive) {
          const SensorData lastData = lastSensorData[sd.channel];

          if (sd.collectionTime < lastData.collectionTime + REPEAT_SUPPRESSION &&
              sd.hasSameValues(lastData))
            doCallback = false;

          if (sd.collectionTime < lastData.collectionTime + CACHE_REPEAT_SUPPRESSION &&
              sd.hasSameValues(lastData))
            cacheNewData = false;

          if (!sd.validChecksum && lastData.validChecksum) {
            cacheNewData = false;

            if (sd.collectionTime < lastData.collectionTime + REUSE_OLD_DATA_LIMIT &&
                sd.hasCloseValues(lastData))
              doCallback = false;
          }
          else if (sd.validChecksum && !lastData.validChecksum)
            doCallback = cacheNewData = true;
        }

        if (doCallback)
          sendData(sd);

        if (cacheNewData)
          lastSensorData[sd.channel] = sd;
      }

      dispatchLocks[dataPin].unlock();
    }).detach();

    dataIndex = -1;
    badBits = 0;
  }
  else if (attempt == 0) {
    tryToCleanUpSignal();
    processMessage(frameEndTime, 1);
  }
  else if (debugOutput) {
    thread([allBits, timestamp TIMES_ARRAY_ARG] {
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
#ifdef SHOW_CORRUPT_DATA
      cout << allBits << endl << timestamp << ": Corrupted data\n";
#endif
    }).detach();
  }
  else if (integrity == BAD_PARITY)
    updateSignalQuality(channel, frameEndTime, RANK_LOW);
}

void ARTHSM::sendData(const SensorData &sd) {
  auto iterator = clientCallbacks.begin();

  while (iterator != clientCallbacks.end()) {
    iterator->second.first(sd, iterator->second.second);
    ++iterator;
  }
}

bool ARTHSM::tryToCleanUpSignal() {
  int msgIndices[] = { dataIndex };
  return combineMessages(1, msgIndices);
}

bool ARTHSM::combineMessages() {
  int msgIndices[] = { baseIndex, syncIndex1, syncIndex2 };
  return combineMessages(3, msgIndices);
}

bool ARTHSM::combineMessages(int count, int *msgIndices) {
  const int totalSubBits = MESSAGE_BITS * 3;
  double subBits[totalSubBits];
  int badBit = -1;
  int checksum1 = 0;
  int checksum2 = 0;

  for (int m = 0; m < count; ++m) {
    int msgIndex = msgIndices[m];
    int highLow = -1;
    int timeOffset = 0;
    int subBitCount = 0;
    double accumulatedTime = 0;
    double accumulatedWeight = 0;
    double availableTime = 0;

    while (subBitCount < totalSubBits) {
      if (availableTime < 0.01) {
        availableTime = timings[mod(msgIndex + timeOffset++, RING_BUFFER_SIZE)];
        highLow *= -1;
      }

      double nextTimeChunk = min(availableTime, THIRD_OF_A_BIT - accumulatedTime);

      accumulatedTime += nextTimeChunk;
      accumulatedWeight += nextTimeChunk * highLow;
      availableTime -= nextTimeChunk;

      if (abs(accumulatedTime - THIRD_OF_A_BIT) < 0.01 ||
         (msgIndex + timeOffset) % RING_BUFFER_SIZE == dataEndIndex)
      {
        subBits[subBitCount++] = accumulatedWeight;
        accumulatedTime = accumulatedWeight = 0;
      }
    }

    if (m < count - 1)
      continue;

    timeOffset = 0;
    badBit = -1;
    checksum1 = 0;
    checksum2 = 0;

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

  return (checksum1 == checksum2 && badBit >= -1);
}

bool ARTHSM::findStartOfTriplet() {
  baseIndex = syncIndex1;
  baseTime = syncTime1;
  int tries = 0;

  while (baseIndex > syncTime1 - SYNC_TO_SYNC_TIME) {
    baseTime -= timings[baseIndex = mod(baseIndex - 1, RING_BUFFER_SIZE)];
    baseTime -= timings[baseIndex = mod(baseIndex - 1, RING_BUFFER_SIZE)];

    if (++tries >= RING_BUFFER_SIZE / 3) {
      baseIndex = -1;
      return false;
    }
  }

  if (baseTime < syncTime1 - SYNC_TO_SYNC_TIME - BIT_LENGTH / 2) {
    baseTime += timings[baseIndex = (baseIndex + 1) % RING_BUFFER_SIZE];
    baseTime += timings[baseIndex = (baseIndex + 1) % RING_BUFFER_SIZE];
  }

  return true;
}

int ARTHSM::updateSignalQuality(char channel, long time, int rank) {
  if (channel == '?')
    return 0;

  vector<TimeAndQuality> recents;
  bool channelActive = false;
  int siblingRank = -1;

  if (qualityTracking.count(channel) > 0) {
    channelActive = true;
    recents = qualityTracking[channel];

    // Purge old data, or very recent data (from the same 3-message repeat) of lesser or equal quality)
    auto it = recents.begin();

    while (it != recents.end()) {
      long dataTime = it->first;

      if (dataTime + MESSAGE_LENGTH * 4 > time)
        siblingRank = it->second;

      if (dataTime + SIGNAL_QUALITY_WINDOW < time || (siblingRank >= 0 && siblingRank <= rank))
        it = recents.erase(it);
      else
        ++it;
    }
  }

  if (rank != RANK_CHECK && (siblingRank < 0 || siblingRank < rank))
    recents.push_back({ time, rank });

  if (channelActive || rank == RANK_HIGH) // Only track low-quality data for an active channel
    qualityTracking[channel] = recents;

  auto it = recents.begin();
  int total = 0;

  while (it != recents.end()) {
    total += it->second;
    ++it;
  }

  int desiredTotal = max((int) (SIGNAL_QUALITY_WINDOW / DESIRED_SIGNAL_RATE), (int) recents.size()) * RANK_HIGH;

  return min((int) round(total * 100.0 / desiredTotal), 100);
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

void ARTHSM::establishQualityCheck() {

  qualityCheckLoopControl = qualityCheckExitSignal.get_future();

  thread([this]() {
    while (qualityCheckLoopControl.wait_for(std::chrono::seconds(SIGNAL_QUALITY_CHECK_RATE)) == std::future_status::timeout) {
      dispatchLocks[dataPin].lock();

      long now = micros();
      auto it = lastSensorData.begin();

      while (it != lastSensorData.end()) {
        auto sd = it->second;

        if (sd.collectionTime + SIGNAL_QUALITY_CHECK_RATE < now / 1000000) {
          int prevQuality = sd.signalQuality;
          sd.signalQuality = updateSignalQuality(sd.channel, now, RANK_CHECK);

          if (sd.signalQuality != prevQuality) {
            ARTHSM *sm = this;
            SensorData sdCopy = sd;

            thread([sm, sdCopy]() {
              dispatchLocks[sm->dataPin].lock();
              sm->sendData(sdCopy);
              dispatchLocks[sm->dataPin].unlock();
            }).detach();
          }
        }

        // Only send quality 0 once, then act as if the channel doesn't exist until signal is received again.
        if (sd.signalQuality == 0) {
          it = lastSensorData.erase(it);
          lastSensorData.erase(sd.channel);
          qualityTracking.erase(sd.channel);
        }
        else
          ++it;
      }

      dispatchLocks[dataPin].unlock();
    }
  }).detach();
}

bool ARTHSM::SensorData::hasSameValues(const SensorData &sd) const {
  // Assumption is made that derived values tempCelsius and tempFahrenheit are
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
