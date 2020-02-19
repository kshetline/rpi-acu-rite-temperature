#ifndef AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR
#define AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR

#include <future>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

static const int RING_BUFFER_SIZE = 512;

#undef SHOW_RAW_DATA
#undef SHOW_MARGINAL_DATA

class ArTemperatureHumiditySignalMonitor {
  public:
    enum PinSystem { VIRTUAL, SYS, GPIO, PHYS };
    static PinSystem getPinSystem();

    class SensorData {
      public:
        bool batteryLow;
        char channel;
        unsigned int collectionTime;
        int humidity;
        int miscData1;
        int miscData2;
        int miscData3;
        int rawTemp;
        int signalQuality;
        double tempCelsius;
        double tempFahrenheit;
        bool validChecksum;

        bool hasSameValues(const SensorData &sd) const;
        bool hasCloseValues(const SensorData &sd) const;
    };

   private:
    static bool initialSetupDone;
    static PinSystem pinSystem;
    static int callbackIndex;

    enum DataIntegrity { BAD_BITS, BAD_PARITY, BAD_CHECKSUM, GOOD };

    typedef void (*VoidFunctionPtr)(SensorData sensorData, void *miscData);
    typedef void *VoidPtr;
    typedef std::pair<VoidFunctionPtr, VoidPtr> ClientCallback;
    typedef std::pair<unsigned long, int> TimeAndQuality;

    std::map<int, ClientCallback> clientCallbacks;
    int dataPin = -1;
    bool debugOutput = false;
    std::mutex *dispatchLock;
    std::map<char, SensorData> lastSensorData;
    unsigned long lastSignalChange = 0;
    unsigned long frameStartTime = 0;
    int nextClientCallbackIndex = 0;
    std::promise<void> qualityCheckExitSignal;
    std::future<void> qualityCheckLoopControl;
    std::map<char, std::vector<TimeAndQuality>> qualityTracking;
    int sequentialBits = 0;
    int badBits = 0;
    int potentialDataIndex = 0;
    int dataIndex = -1;
    int dataEndIndex = 0;
    int timingIndex = -1;
    unsigned short timings[RING_BUFFER_SIZE] = {0};

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
    void static signalHasChanged(int index);

  private:
    DataIntegrity checkDataIntegrity();
    void establishQualityCheck();
    int getBit(int offset);
    std::string getBitsAsString();
    int getInt(int firstBit, int lastBit);
    int getInt(int firstBit, int lastBit, bool skipParity);
    int getTiming(int offset);
    bool isSyncAcquired();
    void processMessage(unsigned long frameEndTime);
    void processMessage(unsigned long frameEndTime, int attempt);
    void sendData(const SensorData &sd);
    void setTiming(int offset, unsigned short value);
    void signalHasChangedAux(unsigned long now);
    void tryToCleanUpSignal();
    int updateSignalQuality(char channel, unsigned long time, int rank);

    static bool isZeroBit(int t0, int t1);
    static bool isOneBit(int t0, int t1);
    static bool isShortSync(int t0, int t1);
    static bool isLongSync(int t0, int t1);
};

#endif
