#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/config_item.h"
#include "sensesp_app_builder.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/transforms/lambda_transform.h"

#include <sensesp/system/observablevalue.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <Adafruit_INA3221.h>

#define SEALEVELPRESSURE_HPA (1013.25)

using namespace sensesp;

Adafruit_BMP3XX BMP388;
Adafruit_INA3221 INA;

float voltageToCharge(float voltage){
  float charge;
  if (voltage <= 12) {
    charge = 0.0f;
  } else if (voltage < 12.4) {
    charge = (voltage - 12) * 50.0f / 0.4f / 100.0f;
  } else if (voltage < 12.8) {
    charge = 0.5f + (voltage - 12.4) * 50.0f / 0.4f / 100.0f;
  } else {
    charge = 1.0f;
  }
  return charge;
}

void BMPDebug() {
  if (! BMP388.performReading()) {
    debugI("Failed to perform reading :(");
    return;
  }
  debugI("Temperature = %f C",BMP388.temperature);

  debugI("Pressure = %f hPa", BMP388.pressure / 100.0);
  debugI("Pressure = %f hPa", BMP388.readPressure() / 100.0);

  debugI();
}

void INADebug() {
  // INA.setShuntResistance(0,0.05f);
  INA.setShuntResistance(0, 0.025f);
  for (int i = 0 ; i < 1; i++){
    debugI("Shunt %d:  bus voltage %f  V / shunt voltage %e  mV / current %e",
      i, INA.getBusVoltage(i), INA.getShuntVoltage(i)*1000.0, INA.getCurrentAmps(i)*1000.0);
  }
}

void registerBMP388(uint read_delay) {
  debugI("*** Configuring BMP388 sensor ***");

  BMP388.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  BMP388.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  BMP388.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  BMP388.setOutputDataRate(BMP3_ODR_50_HZ);

  // Temperature sensor
  auto *BMP388SensorT = new RepeatSensor<float>(read_delay, [&BMP388]() {
    return (BMP388.readTemperature() + 273.15); });

  auto BMP388SKOutputT = new SKOutput<float>(
      "sensors.bmp388.temperature",
      "/sensors/bmp388/temperature/sk",
      new SKMetadata("K", "BMP388 temperature (K)")
  );
  ConfigItem(BMP388SKOutputT)
      ->set_title("BMP388 Temperature Signal K Path")
      ->set_sort_order(100);

  BMP388SensorT
      ->connect_to(BMP388SKOutputT);

  // Pressure sensor
  auto *BMP388SensorP = new RepeatSensor<float>(read_delay, [&BMP388]() {
    return (BMP388.readPressure()); });

  auto BMP388SKOutputP = new SKOutput<float>(
      "sensors.bmp388.pressure",
      "/sensors/bmp388/pressure/sk",
      new SKMetadata("Pa", "BMP388 pressure (Pa)")
  );
  ConfigItem(BMP388SKOutputP)
      ->set_title("BMP388 Pressure Signal K Path")
      ->set_sort_order(101);

  BMP388SensorP
      ->connect_to(BMP388SKOutputP);
}

void registerINA3221(uint read_delay) {
  debugI("*** registering INA 3221");

  //// SHUNT 0 ////
  // Voltage measurement
  auto *shunt0Voltage = new RepeatSensor<float>(read_delay, [&INA]() {
    return (INA.getBusVoltage(0)); });
  auto shunt0VoltageSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt0.voltage",
    "/electrical/batteries/shunt0/voltage/sk",
    new SKMetadata("V", "shunt 0 voltage (V)")
  );
  ConfigItem(shunt0VoltageSKOutput)
      ->set_title("Shunt 0 voltage SK path")
      ->set_sort_order(200);
  shunt0Voltage->connect_to(shunt0VoltageSKOutput);

  // Current measurement
  auto *shunt0Current = new RepeatSensor<float>(read_delay, [&INA]() {
    return (INA.getShuntVoltage(0)); });

  // The mouser shunts I have have a resistance of 0.0005 Ohm
  auto* shunt0Resistance = new PersistingObservableValue<float>(
    0.05f*0.0005f/(0.05f+0.0005f),
    "/electrical/batteries/shunt0/resistance");
  ConfigItem(shunt0Resistance)
    ->set_title("Shunt 0 resistance (Ohm)")
    ->set_description("Shunt resistor value in Ohms (e.g. 0.05)")
    ->set_requires_restart(true)
    ->set_sort_order(201);

  auto* divide_transform0 = new LambdaTransform<float, float>(
    [shunt0Resistance](float input) -> float {
      float r = shunt0Resistance->get();
      if (r == 0.0f) return 0.0f;
      // debugI("input=%e  divisor=%e output=%e", input, r, input / r);
      return input / r;
    }
  );

  auto shunt0CurrentSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt0.current",
    "/electrical/batteries/shunt0/current/sk",
    new SKMetadata("A", "shunt0  current (A)")
  );
  ConfigItem(shunt0CurrentSKOutput)
      ->set_title("Shunt 0 current SK path")
      ->set_sort_order(202);

  shunt0Current
      ->connect_to(divide_transform0)
      ->connect_to(shunt0CurrentSKOutput);

  // Charge estimate:
  auto *shunt0Charge = new RepeatSensor<float>(read_delay, [&INA]() {
    float voltage = INA.getBusVoltage(0);

    return voltageToCharge(voltage);
  });
  auto shunt0ChargeSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt0.capacity.stateOfCharge",
    "/electrical/batteries/shunt0/capacity/stateOfCharge/sk",
    new SKMetadata("ratio", "shunt 0 state of charge (ratio)")
  );
  ConfigItem(shunt0ChargeSKOutput)
      ->set_title("Shunt 0 state of charge SK path")
      ->set_sort_order(203);
  shunt0Charge
      ->connect_to(shunt0ChargeSKOutput);

  //// SHUNT 1 ////
  // Voltage measurement
  auto *shunt1Voltage = new RepeatSensor<float>(read_delay, [&INA]() {
    return (INA.getBusVoltage(1)); });
  auto shunt1VoltageSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt1.voltage",
    "/electrical/batteries/shunt1/voltage/sk",
    new SKMetadata("V", "shunt 1 voltage (V)")
  );
  ConfigItem(shunt1VoltageSKOutput)
      ->set_title("Shunt 1 voltage SK path")
      ->set_sort_order(300);
  shunt1Voltage->connect_to(shunt1VoltageSKOutput);

  // Current measurement
  auto *shunt1Current = new RepeatSensor<float>(read_delay, [&INA]() {
    return (INA.getShuntVoltage(1)); });

  // The mouser shunts I have have a resistance of 0.0005 Ohm
  auto* shunt1Resistance = new PersistingObservableValue<float>(
    0.05f*0.0005f/(0.05f+0.0005f),
    "/electrical/batteries/shunt1/resistance");
  ConfigItem(shunt1Resistance)
    ->set_title("Shunt 1 resistance (Ohm)")
    ->set_description("Shunt resistor value in Ohms (e.g. 0.05)")
    ->set_requires_restart(true)
    ->set_sort_order(301);

  auto* divide_transform1 = new LambdaTransform<float, float>(
    [shunt1Resistance](float input) -> float {
      float r = shunt1Resistance->get();
      if (r == 0.0f) return 0.0f;
      // debugI("input=%e  divisor=%e output=%e", input, r, input / r);
      return input / r;
    }
  );

  auto shunt1CurrentSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt1.current",
    "/electrical/batteries/shunt1/current/sk",
    new SKMetadata("A", "shunt1  current (A)")
  );
  ConfigItem(shunt1CurrentSKOutput)
      ->set_title("Shunt 1 current SK path")
      ->set_sort_order(302);

  shunt1Current
      ->connect_to(divide_transform1)
      ->connect_to(shunt1CurrentSKOutput);

  // Charge estimate:
  auto *shunt1Charge = new RepeatSensor<float>(read_delay, [&INA]() {
    float voltage = INA.getBusVoltage(1);

    return voltageToCharge(voltage);
  });
  auto shunt1ChargeSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt1.capacity.stateOfCharge",
    "/electrical/batteries/shunt1/capacity/stateOfCharge/sk",
    new SKMetadata("ratio", "shunt 1 state of charge (ratio)")
  );
  ConfigItem(shunt1ChargeSKOutput)
      ->set_title("Shunt 1 state of charge SK path")
      ->set_sort_order(303);
  shunt1Charge
      ->connect_to(shunt1ChargeSKOutput);

  //// SHUNT 2 ////
  // Voltage measurement
  auto *shunt2Voltage = new RepeatSensor<float>(read_delay, [&INA]() {
    return (INA.getBusVoltage(2)); });
  auto shunt2VoltageSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt2.voltage",
    "/electrical/batteries/shunt2/voltage/sk",
    new SKMetadata("V", "shunt 2 voltage (V)")
  );
  ConfigItem(shunt2VoltageSKOutput)
      ->set_title("Shunt 2 voltage SK path")
      ->set_sort_order(400);
  shunt2Voltage->connect_to(shunt2VoltageSKOutput);

  // Current measurement
  auto *shunt2Current = new RepeatSensor<float>(read_delay, [&INA]() {
    return (INA.getShuntVoltage(2)); });

  // The mouser shunts I have have a resistance of 0.0005 Ohm
  auto* shunt2Resistance = new PersistingObservableValue<float>(
    0.05f*0.0005f/(0.05f+0.0005f),
    "/electrical/batteries/shunt2/resistance");
  ConfigItem(shunt2Resistance)
    ->set_title("Shunt 2 resistance (Ohm)")
    ->set_description("Shunt resistor value in Ohms (e.g. 0.05)")
    ->set_requires_restart(true)
    ->set_sort_order(401);

  auto* divide_transform2 = new LambdaTransform<float, float>(
    [shunt2Resistance](float input) -> float {
      float r = shunt2Resistance->get();
      if (r == 0.0f) return 0.0f;
      // debugI("input=%e  divisor=%e output=%e", input, r, input / r);
      return input / r;
    }
  );

  auto shunt2CurrentSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt2.current",
    "/electrical/batteries/shunt2/current/sk",
    new SKMetadata("A", "shunt2  current (A)")
  );
  ConfigItem(shunt2CurrentSKOutput)
      ->set_title("Shunt 2 current SK path")
      ->set_sort_order(402);

  shunt2Current
      ->connect_to(divide_transform2)
      ->connect_to(shunt2CurrentSKOutput);

  // Charge estimate:
  auto *shunt2Charge = new RepeatSensor<float>(read_delay, [&INA]() {
    float voltage = INA.getBusVoltage(2);

    return voltageToCharge(voltage);
  });
  auto shunt2ChargeSKOutput = new SKOutputFloat(
    "electrical.batteries.shunt2.capacity.stateOfCharge",
    "/electrical/batteries/shunt2/capacity/stateOfCharge/sk",
    new SKMetadata("ratio", "shunt 2 state of charge (ratio)")
  );
  ConfigItem(shunt2ChargeSKOutput)
      ->set_title("Shunt 2 state of charge SK path")
      ->set_sort_order(403);
  shunt2Charge
      ->connect_to(shunt2ChargeSKOutput);
}


void setup() {
#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(9600);
#endif

  SetupLogging();

  // Create the global SensESPApp() object.
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    ->set_hostname("BatteryMonitor")
                    ->enable_ota("")
                    ->enable_system_info_sensors("sensors.batterymonitor")
                    ->get_app();

  // Define how often SensESP should read the sensor(s) in milliseconds
  uint read_delay = 10000;
  int scl = 18;
  int sda = 17;
  bool status;
  Wire.begin(scl, sda);

  status = BMP388.begin_I2C();
  if (!status) {
    debugI("*** Could not find BMP388 sensor ***");
  } else {
    registerBMP388(read_delay);
  }

  status = INA.begin(0x40, &Wire);
  if (!status) {
    debugI("*** Could not find INA3221 sensor ***");
  } else {
    INA.setAveragingMode(INA3221_AVG_16_SAMPLES);
    registerINA3221(read_delay);
  }
}

// main program loop
void loop() {
  static auto event_loop = sensesp_app->get_event_loop();
  event_loop->tick();
  // BMPDebug();
  // INADebug();
  // delay(5000);
}
