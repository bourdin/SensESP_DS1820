
#include <memory>

#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/transforms/frequency.h"
#include "sensesp_onewire/onewire_temperature.h"
#include "sensesp_app_builder.h"
#include "OneWireNg_CurrentPlatform.h"
#include <Adafruit_BMP280.h>

using namespace reactesp;
using namespace sensesp;
using namespace sensesp::onewire;

void registerBMP280(int sda, int scl, uint read_delay) {
  debugI("*** Initializing BMP280 sensor ***");
  Adafruit_BMP280 bmp280;

  Wire.setPins(sda, scl);
  Wire.begin();
  bool status = bmp280.begin();
  if (!status) {
    debugI("*** Could not find BMP280 sensor ***");
    return;
  }
  debugI("*** Configuring BMP280 sensor ***");

  auto *BMP280Sensor = new RepeatSensor<float>(read_delay, [&bmp280]() {
    return (bmp280.readTemperature() + 273.15); });

  auto BMP280SKOutput = new SKOutput<float>(
      "environment.enginecompartment.temperature",
      "/sensors/enginecompartmenttemperature/sk",
      new SKMetadata("K", "temperature")
  );
  ConfigItem(BMP280SKOutput)
      ->set_title("Engine compartment temperature Signal K Path")
      ->set_sort_order(302);

  BMP280Sensor
      ->connect_to(BMP280SKOutput);
}

void registerRPM(int pin, uint read_delay) {
  debugI("*** Initializing RPM sensor ***");
  auto *RPMSensor = new DigitalInputCounter(pin, INPUT_PULLUP, RISING, read_delay);

  auto RPMFrequency = new Frequency(1., "/RPMSensor/calibrate");
  ConfigItem(RPMFrequency)
      ->set_title("RPM sensor frequency multiplier")
      ->set_sort_order(201);

  auto RPMSensorSKOutput = new SKOutputFloat(
      "propulsion.main.revolutions",
      "/sensors/engine_rpm/sk",
      new SKMetadata("Hz", "engine revolution")
  );
  ConfigItem(RPMSensorSKOutput)
      ->set_title("RPM sensor frequency multiplier Signal K Path")
      ->set_sort_order(202);

  RPMSensor
     ->connect_to(RPMFrequency)
      ->connect_to(RPMSensorSKOutput);
  debugI("*** Initialized RPM sensor ***");
}

void registerDS1820(DallasTemperatureSensors *dts, uint read_delay, int i){
  debugI("*** Initializing OneWire sensor %d ***", i);
  auto tempSensor = new OneWireTemperature(dts, read_delay, String("/sensors/temperature") + String(i) + String("/oneWire"));
  ConfigItem(tempSensor)
      ->set_title(String("DS1820 temperature sensor ") + String(i) + String(" address"))
      ->set_sort_order(100+10*i);

  auto tempSensorCalibration =
      new Linear(1.0, 0.0, String("/sensors/temperature") + String(i) + String("/linear"));
  ConfigItem(tempSensorCalibration)
      ->set_title(String("DS1820 temperature sensor ") + String(i) + String(" calibration"))
      ->set_sort_order(100+10*i+1);

  auto tempSensorSKOutput = new SKOutputFloat(
      String("environment.temperature") + String(i),
      String("/sensors/temperature") + String(i) + String("/sk"),
      new SKMetadata("K", "temperature")
  );
  ConfigItem(tempSensorSKOutput)
      ->set_title(String("DS1820 temperature sensor ") + String(i) + String(" Signal K Path"))
      ->set_sort_order(100+10*i+2);

  tempSensor
      ->connect_to(tempSensorCalibration)
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
     Tell SensESP where the sensor is connected to the board
     ESP32 pins are specified as just the X in GPIOX
  */
  uint8_t OWpin = 19;
  pinMode(OWpin, INPUT_PULLUP);

  // Define how often SensESP should read the sensor(s) in milliseconds
  uint read_delay = 10000;

  // Count available sensors
  int numSensors = 0;
  OneWireNg* onewire_ = new OneWireNg_CurrentPlatform(OWpin, false);
  for (const auto& addr : *onewire_) {
    numSensors++;
  }
  debugI("*** Found %d OneWire sensor(s) ***", numSensors);

  // Create the main Dallas temperature sensor
  DallasTemperatureSensors* dts = new DallasTemperatureSensors(OWpin);

  // Register initial onewire senors
  for (int i = 0; i < numSensors; i++) {
    registerDS1820(dts, read_delay, i);
  }

  // Create the RPM counter
  uint8_t RPMpin = 21;
  read_delay = 1000;
  registerRPM(RPMpin, read_delay);

  // read_delay = 5000;
  // int scl = 1;
  // int sda = 2;
  // pinMode(OWpin, INPUT_PULLUP);
  // registerBMP280(sda, scl, read_delay);
}

// main program loop
void loop() {
  static auto event_loop = sensesp_app->get_event_loop();
  event_loop->tick();
}
