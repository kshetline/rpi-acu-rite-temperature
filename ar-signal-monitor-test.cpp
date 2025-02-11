#include "ar-signal-monitor.h"
#include <chrono>
#if defined(WIN32) || defined(WINDOWS)
#include <windows.h>
#else
#include <csignal>
#endif
#include <cstring>
#include <iostream>
#include "pin-conversions.h"

using namespace std;

void callback(ArTemperatureHumiditySignalMonitor::SensorData sd, void *msg) {
  if (sd.channel == '-')
    cout << "Dead air\n";
  else
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
  if (argc == 2 && strcmp(argv[1], "-p") == 0) {
    for (int i = 1; i <= 56; ++i) {
      if (41 <= i && i <= 49)
        continue;

      cout << "phys " << i << " -> gpio " <<
        convertPin(i, PHYS, GPIO) << " -> wpi " <<
        convertPin(i, PHYS, WIRING_PI) << endl;
    }

    cout << endl;

    for (int i = 0; i <= 31; ++i) {
      cout << "gpio " << i << " -> phys " <<
        convertPin(i, GPIO, PHYS) << " -> wpi " <<
        convertPin(i, GPIO, WIRING_PI) << endl;
    }

    cout << endl;

    for (int i = 0; i <= 31; ++i) {
      cout << "wpi " << i << " -> gpio " <<
        convertPin(i, WIRING_PI, GPIO) << " -> phys " <<
        convertPin(i, WIRING_PI, PHYS) << endl;
    }

    return 0;
  }

  int pin = (argc == 2 && strcmp(argv[1], "-d") == 0) ? 0 : 27;

  cout << "*** Acu-Rite temperature/humidity monitor starting *** \n\n";
  SM = new ArTemperatureHumiditySignalMonitor();
  SM->init(pin, PinSystem::GPIO);
  SM->enableDebugOutput(true);
  SM->addListener(&callback, (void *) "Got data");

#if defined(WIN32) || defined(WINDOWS)
  SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
#endif

  while (true)
    this_thread::sleep_for(chrono::seconds(1));

  return 0;
}
