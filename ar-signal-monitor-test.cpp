#include "ar-signal-monitor.h"
#include <chrono>
#include <csignal>
#include <iostream>
#include "pin-conversions.h"

using namespace std;

void callback(ArTemperatureHumiditySignalMonitor::SensorData sd, void *msg) {
  printf("<> %c: %d\n", sd.channel, sd.signalQuality);
  // printf("%s%c %c, %d, %.1f, %.1f\n",
  //   (char *) msg, sd.validChecksum ? ':' : '~', sd.channel,
  //   sd.humidity, sd.tempCelsius, sd.tempFahrenheit);
}

static ArTemperatureHumiditySignalMonitor *SM;

void signalHandler(int signum) {
  delete SM;
  exit(signum);
}

int main(int argc, char **argv) {
  cout << "Acu-Rite temperature/humidity monitor starting\n\n";
  SM = new ArTemperatureHumiditySignalMonitor();
  SM->init(13, PinSystem::PHYS);
  SM->enableDebugOutput(true);
  SM->addListener(&callback, (void *) "Got data");

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
