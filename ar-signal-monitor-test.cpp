#include <iostream>
#include "ArTemperatureHumiditySignalMonitor.h"

using namespace std;

void callback(ArTemperatureHumiditySignalMonitor::SensorData sd, void *msg) {
	printf("<>\n");
  // printf("%s%c %c, %d, %.1f, %.1f\n",
  //   (char *) msg, sd.validChecksum ? ':' : '~', sd.channel,
  //   sd.humidity, sd.tempCelsius, sd.tempFahrenheit);
}

int main(int argc, char **argv) {
  cout << "Acu-Rite temperature/humidity monitor starting\n\n";
  auto SM = ArTemperatureHumiditySignalMonitor();
  SM.init(13, ArTemperatureHumiditySignalMonitor::PinSystem::PHYS);
  SM.enableDebugOutput(true);
  SM.addListener(&callback, (void *) "Got data");

  getchar();
  getchar();

  return 0;
}
