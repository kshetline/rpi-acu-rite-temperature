#ifndef AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR
#define AR_TEMPERATURE_HUMIDITY_SIGNAL_MONITOR

#include <map>
#include <mutex>
#include <string>
#include <utility>

const int RING_BUFFER_SIZE = 512;

#undef COLLECT_STATS
#undef SHOW_RAW_DATA
#undef SHOW_MARGINAL_DATA

class ArTemperatureHumiditySignalMonitor {
  public:
    enum PinSystem { VIRTUAL, SYS, GPIO, PHYS };

    static PinSystem pinSystem;

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
        double tempCelsius;
        double tempFahrenheit;
        bool validChecksum;

        bool hasSameValues(const SensorData &sd) const;
        bool hasCloseValues(const SensorData &sd) const;
    };

   private:
    static bool initialSetupDone;

    enum DataIntegrity { BAD_BITS, BAD_PARITY, BAD_CHECKSUM, GOOD };

    typedef void (*VoidFunctionPtr)(SensorData sensorData, void *miscData);
    typedef void *VoidPtr;
    typedef std::pair<VoidFunctionPtr, VoidPtr> ClientCallback;

    int callbackIndex = -1;
    std::map<int, ClientCallback> clientCallbacks;
    int dataPin;
    bool debugOutput = false;
    std::mutex *dispatchLock;
    std::map<char, SensorData> lastSensorData;
    unsigned long lastSignalChange = 0;
    unsigned long frameStartTime = 0;
    int nextClientCallbackIndex = 0;
    std::mutex *signalLock;
    int syncCount = 0;
    int syncIndex1 = 0;
    int syncIndex2 = 0;
    int timingIndex = -1;
    unsigned short timings[RING_BUFFER_SIZE] = {0};

  public:
    ArTemperatureHumiditySignalMonitor();
    ~ArTemperatureHumiditySignalMonitor();
    void init(int dataPin);
    void init(int dataPin, PinSystem pinSys);

    int addListener(VoidFunctionPtr callback);
    int addListener(VoidFunctionPtr callback, void *data);
    void enableDebugOutput(bool state);
    void removeListener(int listenerId);
    void static signalHasChanged(ArTemperatureHumiditySignalMonitor *sm);

  private:
    DataIntegrity checkDataIntegrity();
    int getBit(int offset);
    std::string getBitsAsString();
    int getInt(int firstBit, int lastBit);
    int getInt(int firstBit, int lastBit, bool skipParity);
    int getTiming(int offset);
    bool isEndSyncAcquired();
    bool isStartSyncAcquired();
    void processMessage(unsigned long frameEndTime);
    void processMessage(unsigned long frameEndTime, int attempt);
    void setTiming(int offset, unsigned short value);
    void signalHasChangedAux();
    void tryToCleanUpSignal();

#ifdef COLLECT_STATS
    unsigned long totalMessageTime = 0;
    unsigned long totalMessages = 0;
    unsigned long totalShortBitTime = 0;
    unsigned long totalShortBits = 0;
    unsigned long totalLongBitTime = 0;
    unsigned long totalLongBits = 0;
    unsigned long totalShortSyncTime = 0;
    unsigned long totalShortSyncs = 0;
    unsigned long totalPreLongSyncTime = 0;
    unsigned long totalPreLongSyncs = 0;
    unsigned long totalLongSyncTime = 0;
    unsigned long totalLongSyncs = 0;
#endif
};

#endif
