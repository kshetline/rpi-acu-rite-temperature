#include "ar-signal-monitor.h"
#include <chrono>
#if defined(WIN32) || defined(WINDOWS)
#include <windows.h>
#else
#include <csignal>
#endif
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

#if defined(WIN32) || defined(WINDOWS)
BOOL consoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT) {
    cout << "\n*** Exiting temperature/humidity monitor ***\n";
    exit(0);
  }

  return true;
}
#else
void signalHandler(int signum) {
  cout << "\n*** Exiting temperature/humidity monitor ***\n";
  delete SM;
  exit(signum);
}
#endif

int main(int argc, char **argv) {
  cout << "*** Acu-Rite temperature/humidity monitor starting *** \n\n";
  SM = new ArTemperatureHumiditySignalMonitor();
  SM->init(13, PinSystem::PHYS);
  SM->enableDebugOutput(true);
  SM->addListener(&callback, (void *) "Got data");

#if defined(WIN32) || defined(WINDOWS)
  SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
#endif

  while (true)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}
