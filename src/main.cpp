
#include <memory>

#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/config_item.h"
#include "sensesp_app_builder.h"
#include "sensesp_onewire/onewire_temperature.h"
#include "OneWireNg_CurrentPlatform.h"

using namespace reactesp;
using namespace sensesp;
using namespace sensesp::onewire;

void registerDS1820(DallasTemperatureSensors *dts, uint read_delay, int i){
  debugI("*** Initializing OneWire sensor %d ***", i);
  auto tempSensor = new OneWireTemperature(dts, read_delay, String("/tempSensor") + String(i) + String("/oneWire"));
  ConfigItem(tempSensor)
    ->set_title(String("DS1820 temperature sensor ") + String(i) + String(" address"))
    ->set_sort_order(100+10*i);

  auto tempSensorCalibration =
      new Linear(1.0, 0.0, String("/tempSensor") + String(i) + String("/linear"));
  ConfigItem(tempSensorCalibration)
      ->set_title(String("DS1820 temperature sensor ") + String(i) + String(" calibration"))
      ->set_sort_order(100+10*i+1);

  auto tempSensorSKOutput = new SKOutputFloat(
        String("environment.temperature") + String(i), String("/tempSensor") + String(i) + String("/skPath"));
  ConfigItem(tempSensorSKOutput)
        ->set_title(String("DS1820 temperature sensor ") + String(i) + String(" Signal K Path"))
        ->set_sort_order(100+10*i+2);

  tempSensor->connect_to(tempSensorCalibration)
                ->connect_to(tempSensorSKOutput);
  debugI("*** Initialized OneWire sensor %d ***", i);
}

void setup() {
  SetupLogging();

  // Create the global SensESPApp() object.
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                      // Set a custom hostname for the app.
                    ->set_hostname("1Wire")
                    // Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    ->get_app();

  /*
     Find all the sensors and their unique addresses. Then, each new instance
     of OneWireTemperature will use one of those addresses. You can't specify
     which address will initially be assigned to a particular sensor, so if you
     have more than one sensor, you may have to swap the addresses around on
     the configuration page for the device. (You get to the configuration page
     by entering the IP address of the device into a browser.)
  */

  /*
     Tell SensESP where the sensor is connected to the board
     ESP32 pins are specified as just the X in GPIOX
  */
  uint8_t pin = 19;
  // Define how often SensESP should read the sensor(s) in milliseconds
  uint read_delay = 10000;

  // Counting available sensors
  int numSensors = 0;
  OneWireNg* onewire_ = new OneWireNg_CurrentPlatform(pin, false);
  for (const auto& addr : *onewire_) {
    numSensors++;
  }
  debugI("*** Found %d OneWire sensor(s) ***", numSensors);
  DallasTemperatureSensors* dts = new DallasTemperatureSensors(pin);

  for (int i = 0; i < numSensors; i++) {
    registerDS1820(dts, read_delay, i);
  }
}

// main program loop
void loop() {
  static auto event_loop = sensesp_app->get_event_loop();
  event_loop->tick();
}
