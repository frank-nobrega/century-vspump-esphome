// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace esphome::century_vspump {

class CenturyVSPump;

class CenturyVSPumpRunSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(CenturyVSPump *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;
  CenturyVSPump *parent_{nullptr};
};

class CenturyVSPumpSpeedNumber : public number::Number, public Component {
 public:
  void set_parent(CenturyVSPump *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  CenturyVSPump *parent_{nullptr};
};

class CenturyVSPump : public PollingComponent, public modbus::ModbusClientDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override;
  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override;
  void on_not_sent(std::span<const uint8_t> request_pdu) override;
  bool on_no_response(std::span<const uint8_t> request_pdu) override;

  void set_maintenance_sensor(binary_sensor::BinarySensor *sensor) { this->maintenance_sensor_ = sensor; }
  void set_run_switch(CenturyVSPumpRunSwitch *entity) { this->run_switch_ = entity; }
  void set_speed_number(CenturyVSPumpSpeedNumber *entity) { this->speed_number_ = entity; }
  void set_rpm_sensor(sensor::Sensor *entity) { this->rpm_sensor_ = entity; }
  void set_current_sensor(sensor::Sensor *entity) { this->current_sensor_ = entity; }
  void set_input_power_sensor(sensor::Sensor *entity) { this->input_power_sensor_ = entity; }
  void set_output_power_sensor(sensor::Sensor *entity) { this->output_power_sensor_ = entity; }
  void set_dc_bus_voltage_sensor(sensor::Sensor *entity) { this->dc_bus_voltage_sensor_ = entity; }
  void set_drive_temperature_sensor(sensor::Sensor *entity) { this->drive_temperature_sensor_ = entity; }
  void set_fault_code_sensor(sensor::Sensor *entity) { this->fault_code_sensor_ = entity; }
  void set_status_text_sensor(text_sensor::TextSensor *entity) { this->status_text_sensor_ = entity; }

  void request_run(bool run);
  void request_speed(float rpm);
  void set_maintenance(bool active);

 protected:
  enum class CommandKind : uint8_t { STATUS, READ_SENSOR, GO, STOP, SET_DEMAND };

  struct Command {
    CommandKind kind;
    uint8_t function;
    std::vector<uint8_t> payload;
    uint8_t sensor_address{0};
    uint8_t retries{0};
  };

  void enqueue_(Command command);
  void enqueue_front_(Command command);
  void enqueue_status_();
  void enqueue_sensor_(uint8_t address);
  void enqueue_stop_();
  void enqueue_demand_(uint16_t rpm);
  void send_next_();
  void finish_command_();
  void handle_sensor_(uint8_t address, uint16_t raw);
  void publish_status_(uint8_t status);
  static const char *status_to_string_(uint8_t status);

  binary_sensor::BinarySensor *maintenance_sensor_{nullptr};
  CenturyVSPumpRunSwitch *run_switch_{nullptr};
  CenturyVSPumpSpeedNumber *speed_number_{nullptr};
  sensor::Sensor *rpm_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *input_power_sensor_{nullptr};
  sensor::Sensor *output_power_sensor_{nullptr};
  sensor::Sensor *dc_bus_voltage_sensor_{nullptr};
  sensor::Sensor *drive_temperature_sensor_{nullptr};
  sensor::Sensor *fault_code_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};

  std::deque<Command> queue_;
  bool command_in_flight_{false};
  bool maintenance_active_{false};
  bool running_{false};
  uint16_t desired_rpm_{1800};
};

}  // namespace esphome::century_vspump
