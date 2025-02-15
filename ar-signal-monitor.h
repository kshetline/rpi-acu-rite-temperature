#ifndef AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR
#define AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR

#include <future>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(USE_FAKE_GPIOD) || defined(__APPLE__) || defined(WIN32) || defined(WINDOWS)
#include "gpiod-fake.h"
#else
#include <gpiod.h>
#endif
#include "pin-conversions.h"

namespace std { // No, I don't want to indent everything inside this.

static const int RING_BUFFER_SIZE = 512;

#undef SHOW_RAW_DATA
#undef SHOW_MARGINAL_DATA
#undef SHOW_CORRUPT_DATA

#define PI_LOW  GPIOD_CTXLESS_EVENT_CB_FALLING_EDGE
#define PI_HIGH GPIOD_CTXLESS_EVENT_CB_RISING_EDGE

class ArTemperatureHumiditySignalMonitor {
  public:
    class SensorData {
      public:
        bool batteryLow = false;
        char channel = '?';
        int64_t collectionTime = 0;
        int humidity = -999;
        int miscData1 = 0;
        int miscData2 = 0;
        int miscData3 = 0;
        int rawTemp = -999;
        int rank = 0;
        int repeatsCaptured = 0;
        int signalQuality = 0;
        double tempCelsius = -999;
        double tempFahrenheit = -999;
        bool validChecksum = false;

        bool hasSameValues(const SensorData &sd) const;
        bool hasCloseValues(const SensorData &sd) const;
    };

  private:
    static bool initialSetupDone;
    static int nextClientCallbackIndex;
    static bool pinInUse[];
    static int pinsInUse;

    enum DataIntegrity { BAD_BITS, BAD_PARITY, BAD_CHECKSUM, GOOD };

    typedef void (*VoidFunctionPtr)(SensorData sensorData, void *miscData);
    typedef void *VoidPtr;
    typedef pair<VoidFunctionPtr, VoidPtr> ClientCallback;
    typedef pair<int64_t, int> TimeAndQuality;

    int badBits = 0;
    int baseIndex = 0;
    int64_t baseTime = -1;
    map<int, ClientCallback> clientCallbacks;
    int dataEndIndex = 0;
    int dataIndex = -1;
    int dataPin = -1;
    bool debugOutput = false;
    int64_t frameStartTime = 0;
    SensorData heldData;
    string heldBits;
    future<void> heldDataControl;
    promise<void> heldDataExitSignal;
    bool holdingRecentData = false;
    thread *holdThread = nullptr;
    int64_t lastConnectionCheck = 0;
    map<char, SensorData> lastSensorData;
    int lastPinState = -1;

    int64_t lastSignalChange = 0;
    int potentialDataIndex = 0;
    promise<void> qualityCheckExitSignal;
    future<void> qualityCheckLoopControl;
    map<char, vector<TimeAndQuality>> qualityTracking;
    int sequentialBits = 0;
    int syncIndex1 = 0;
    int syncIndex2 = 0;
    int64_t syncTime1 = -1;
    int64_t syncTime2 = -1;
    int timingIndex = -1;
    int timings[RING_BUFFER_SIZE] = {0};

  public:
    ArTemperatureHumiditySignalMonitor();
    ~ArTemperatureHumiditySignalMonitor();
    void init(int dataPin);
    void init(int dataPin, PinSystem pinSys);

    int addListener(VoidFunctionPtr callback);
    int addListener(VoidFunctionPtr callback, void *data);
    int getDataPin();
    void enableDebugOutput(bool state);
    void removeListener(int listenerId);
    int static signalHasChanged(int eventType, unsigned int dataPin, const timespec* tick, void *userData);

  private:
    DataIntegrity checkDataIntegrity();
    bool combineMessages();
    bool combineMessages(int count, int *msgIndices);
    void dispatchData(SensorData sd, std::string allBits);
    void enqueueSensorData(SensorData sd, std::string bitString);
    void establishQualityCheck();
    bool findStartOfTriplet();
    int getBit(int offset);
    string getBitsAsString();
    int getInt(int firstBit, int lastBit);
    int getInt(int firstBit, int lastBit, bool skipParity);
    int getTiming(int offset);
    bool isSyncAcquired();
    void processMessage(int64_t frameEndTime, int64_t clockTime);
    void processMessage(int64_t frameEndTime, int64_t clockTime, int attempt);
    void sendData(const SensorData &sd);
    void setTiming(int offset, int value);
    void signalHasChangedAux(int64_t now, int pinState);
    bool tryToCleanUpSignal();
    int updateSignalQuality(char channel, int64_t time, int rank);

    static int64_t micros();
    static int64_t micros(const timespec* ts);
    static bool isZeroBit(int t0, int t1);
    static bool isOneBit(int t0, int t1);
    static bool isShortSync(int t0, int t1);
    static bool isLongSync(int t0, int t1);
};

#endif
}
