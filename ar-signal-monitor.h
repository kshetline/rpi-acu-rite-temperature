#ifndef AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR
#define AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR

#include <future>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifdef USE_FAKE_PIGPIO
#include "pigpio-fake.h"
#else
#include <pigpio.h>
#endif
#include "pin-conversions.h"

static const int RING_BUFFER_SIZE = 512;

#undef SHOW_RAW_DATA
#undef SHOW_MARGINAL_DATA
#undef SHOW_CORRUPT_DATA

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
    static int64_t baseMicroTime;
    static int64_t extendedMicroTime;
    static bool initialSetupDone;
    static uint32_t lastMicroTimeU32;
    static int nextClientCallbackIndex;
    static bool pinInUse[];
    static int pinsInUse;
    static std::mutex timeLock;

    enum DataIntegrity { BAD_BITS, BAD_PARITY, BAD_CHECKSUM, GOOD };

    typedef void (*VoidFunctionPtr)(SensorData sensorData, void *miscData);
    typedef void *VoidPtr;
    typedef std::pair<VoidFunctionPtr, VoidPtr> ClientCallback;
    typedef std::pair<int64_t, int> TimeAndQuality;

    int badBits = 0;
    int baseIndex = 0;
    int64_t baseTime = -1;
    std::map<int, ClientCallback> clientCallbacks;
    int dataEndIndex = 0;
    int dataIndex = -1;
    int dataPin = -1;
    bool debugOutput = false;
    int64_t frameStartTime = 0;
    SensorData heldData;
    std::string heldBits;
    std::future<void> heldDataControl;
    std::promise<void> heldDataExitSignal;
    bool holdingRecentData = false;
    std::thread *holdThread = nullptr;
    int64_t lastConnectionCheck = 0;
    std::map<char, SensorData> lastSensorData;
    int lastPinState = -1;
    int64_t lastSignalChange = 0;
    int potentialDataIndex = 0;
    std::promise<void> qualityCheckExitSignal;
    std::future<void> qualityCheckLoopControl;
    std::map<char, std::vector<TimeAndQuality>> qualityTracking;
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
    void static signalHasChanged(int dataPin, int level, uint32_t tick, void *userData);

  private:
    DataIntegrity checkDataIntegrity();
    bool combineMessages();
    bool combineMessages(int count, int *msgIndices);
    void dispatchData(SensorData sd, std::string allBits);
    void enqueueSensorData(SensorData sd, std::string bitString);
    void establishQualityCheck();
    bool findStartOfTriplet();
    int getBit(int offset);
    std::string getBitsAsString();
    int getInt(int firstBit, int lastBit);
    int getInt(int firstBit, int lastBit, bool skipParity);
    int getTiming(int offset);
    bool isSyncAcquired();
    void processMessage(int64_t frameEndTime);
    void processMessage(int64_t frameEndTime, int attempt);
    void sendData(const SensorData &sd);
    void setTiming(int offset, int value);
    void signalHasChangedAux(int64_t now, int pinState);
    bool tryToCleanUpSignal();
    int updateSignalQuality(char channel, int64_t time, int rank);

    static int64_t micros();
    static int64_t micros(uint32_t microTimeU32);
    static bool isZeroBit(int t0, int t1);
    static bool isOneBit(int t0, int t1);
    static bool isShortSync(int t0, int t1);
    static bool isLongSync(int t0, int t1);
};

#endif
