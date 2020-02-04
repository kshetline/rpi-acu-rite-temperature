#include <napi.h>
#include <iostream>
#include "ArTemperatureHumiditySignalMonitor.h"

using namespace std;

#define ARTHSM ArTemperatureHumiditySignalMonitor

map<int, ARTHSM*> signalMonitorsByPin;
map<int, ARTHSM*> signalMonitorsById;

struct CallbackInfo {
  Napi::Env env;
  Napi::Function callback;
  Napi::FunctionReference &callbackRef;
  napi_threadsafe_function *tsfn;
  int callbackId;
};

static void jsCallback(napi_env env, napi_value js_cb, void* context, void* miscData) {
  CallbackInfo *cbi = (CallbackInfo *) context;
  auto origEnv = cbi->env;
  auto sensorData = (ARTHSM::SensorData*) miscData;
  Napi::Object obj = Napi::Object::New(env);
  napi_value result;
  char channel[2] = { sensorData->channel, 0 };

  obj.Set(Napi::String::New(env, "batteryLow"),
    Napi::Boolean::New(env, sensorData->batteryLow));
  obj.Set(Napi::String::New(env, "channel"),
    Napi::String::New(env, channel));
  obj.Set(Napi::String::New(env, "humidity"),
    sensorData->humidity == -999 ? origEnv.Null() : Napi::Number::New(env, sensorData->humidity));
  obj.Set(Napi::String::New(env, "miscData1"),
    Napi::Number::New(env, sensorData->miscData1));
  obj.Set(Napi::String::New(env, "miscData2"),
    Napi::Number::New(env, sensorData->miscData2));
  obj.Set(Napi::String::New(env, "miscData3"),
    Napi::Number::New(env, sensorData->miscData3));
  obj.Set(Napi::String::New(env, "rawTemp"),
    Napi::Number::New(env, sensorData->rawTemp));
  obj.Set(Napi::String::New(env, "tempCelsius"),
    sensorData->tempCelsius == -999 ? origEnv.Null() : Napi::Number::New(env, sensorData->tempCelsius));
  obj.Set(Napi::String::New(env, "tempFahrenheit"),
    sensorData->tempFahrenheit == -999 ? origEnv.Null() : Napi::Number::New(env, sensorData->tempFahrenheit));
  obj.Set(Napi::String::New(env, "validChecksum"),
    Napi::Boolean::New(env, sensorData->validChecksum));

  napi_value args[] = { obj };

  napi_call_function(env, origEnv.Global(), js_cb, 1, args, &result);
  napi_release_threadsafe_function(*cbi->tsfn, napi_tsfn_release);
  delete sensorData;
}

static void finalizeTSFn(napi_env env, void* data, void* context) {
}

static void callBackHandler(ARTHSM::SensorData sensorData, void *miscData) {
  CallbackInfo *cbi = (CallbackInfo *) miscData;

  auto sensorDataCopy = (ARTHSM::SensorData *) malloc(sizeof(ARTHSM::SensorData));
  memcpy(sensorDataCopy, &sensorData, sizeof(ARTHSM::SensorData));

  if (napi_acquire_threadsafe_function(*cbi->tsfn) == napi_ok)
    napi_call_threadsafe_function(*cbi->tsfn, sensorDataCopy, napi_tsfn_blocking);
  else
    delete sensorDataCopy;
}

void AddSensorDataListener(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2) {
    Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return;
  }

  int pin = info[0].As<Napi::Number>().Int32Value();
  int pinSys = 0;
  int callBackArg = 1;
  ARTHSM *monitor;

  if (info.Length() > 2) {
    pinSys = info[1].As<Napi::Number>().Int32Value();
    ++callBackArg;
  }

  if (signalMonitorsByPin.count(pin) == 0) {
    try {
      monitor = new ARTHSM();
      monitor->init(pin, (ARTHSM::PinSystem) pinSys);
    }
    catch (char *err) {
      cerr << err << endl;
      Napi::TypeError::New(env, err).ThrowAsJavaScriptException();
      return;
    }

    signalMonitorsByPin[pin] = monitor;
  }
  else
    monitor = signalMonitorsByPin[pin];

  auto callback = info[callBackArg].As<Napi::Function>();
  auto callbackRef = Napi::Persistent(callback);
  napi_threadsafe_function *threadSafeFunction = new napi_threadsafe_function;
  CallbackInfo *cbi = new CallbackInfo { env, callback, callbackRef, threadSafeFunction, 0 };

  NAPI_THROW_IF_FAILED_VOID(env,
    napi_create_threadsafe_function(env, callback, nullptr,
    Napi::String::New(env, "ARTHSM callback"), 0, 1, nullptr,
    finalizeTSFn, cbi, jsCallback, threadSafeFunction));

  cbi->callbackId = monitor->addListener(callBackHandler, cbi);
  signalMonitorsById[cbi->callbackId] = monitor;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "addSensorDataListener"),
              Napi::Function::New(env, AddSensorDataListener));

  return exports;
}

NODE_API_MODULE(ar_signal_monitor, Init)
