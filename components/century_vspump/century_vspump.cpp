// SPDX-License-Identifier: GPL-3.0-only

#include "century_vspump.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::century_vspump {

static const char *const TAG = "century_vspump";
static constexpr uint8_t ACK_REQUEST = 0x20;
static constexpr uint8_t ACK_RESPONSE = 0x10;
static constexpr uint8_t MAX_RETRIES = 3;

static constexpr uint8_t FUNC_GO = 0x41;
static constexpr uint8_t FUNC_STOP = 0x42;
static constexpr uint8_t FUNC_STATUS = 0x43;
static constexpr uint8_t FUNC_SET_DEMAND = 0x44;
static constexpr uint8_t FUNC_READ_SENSOR = 0x45;

static constexpr uint8_t SENSOR_RPM = 0x00;
static constexpr uint8_t SENSOR_CURRENT = 0x01;
static constexpr uint8_t SENSOR_DEMAND = 0x03;
static constexpr uint8_t SENSOR_INPUT_POWER = 0x05;
static constexpr uint8_t SENSOR_DC_BUS_VOLTAGE = 0x06;
static constexpr uint8_t SENSOR_DRIVE_TEMPERATURE = 0x07;
static constexpr uint8_t SENSOR_FAULT = 0x09;
static constexpr uint8_t SENSOR_OUTPUT_POWER = 0x0A;

void CenturyVSPumpRunSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->request_run(state);
}

void CenturyVSPumpSpeedNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->request_speed(value);
}

void CenturyVSPump::setup() {
  if (this->maintenance_sensor_ != nullptr) {
    this->maintenance_sensor_->add_on_state_callback([this](bool state) { this->set_maintenance(state); });
    this->set_maintenance(this->maintenance_sensor_->state);
  }
  if (this->run_switch_ != nullptr)
    this->run_switch_->publish_state(false);
  if (this->speed_number_ != nullptr)
    this->speed_number_->publish_state(this->desired_rpm_);
}

void CenturyVSPump::loop() { this->send_next_(); }

void CenturyVSPump::update() {
  // Do not let repeated polling grow an unbounded queue if the motor is absent.
  if (this->queue_.size() > 2 || this->command_in_flight_)
    return;

  // A maintained physical inhibit continually reasserts STOP. Releasing it never
  // queues GO, so a local or Home Assistant command is required to restart.
  if (this->maintenance_active_)
    this->enqueue_stop_();

  this->enqueue_status_();
  this->enqueue_sensor_(SENSOR_RPM);
  this->enqueue_sensor_(SENSOR_DEMAND);
  this->enqueue_sensor_(SENSOR_CURRENT);
  this->enqueue_sensor_(SENSOR_INPUT_POWER);
  this->enqueue_sensor_(SENSOR_OUTPUT_POWER);
  this->enqueue_sensor_(SENSOR_DC_BUS_VOLTAGE);
  this->enqueue_sensor_(SENSOR_DRIVE_TEMPERATURE);
  this->enqueue_sensor_(SENSOR_FAULT);
}

void CenturyVSPump::dump_config() {
  ESP_LOGCONFIG(TAG, "Century/Regal variable-speed pump:");
  ESP_LOGCONFIG(TAG, "  Modbus address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Maintenance inhibit: %s", YESNO(this->maintenance_active_));
}

void CenturyVSPump::request_run(bool run) {
  if (run && this->maintenance_active_) {
    ESP_LOGW(TAG, "Run rejected: physical maintenance inhibit is active");
    if (this->run_switch_ != nullptr)
      this->run_switch_->publish_state(false);
    return;
  }

  this->clear_tx_queue_for_device();
  this->queue_.clear();
  this->command_in_flight_ = false;
  if (run) {
    // The drive protocol requires demand to be established before GO.
    this->enqueue_demand_(this->desired_rpm_);
    this->enqueue_({CommandKind::GO, FUNC_GO, {}});
  } else {
    this->enqueue_stop_();
  }
}

void CenturyVSPump::request_speed(float rpm) {
  uint16_t requested = static_cast<uint16_t>(std::lround(rpm / 50.0f) * 50);
  if (requested < 600)
    requested = 600;
  if (requested > 3450)
    requested = 3450;

  if (this->maintenance_active_) {
    ESP_LOGW(TAG, "Speed change rejected: physical maintenance inhibit is active");
    if (this->speed_number_ != nullptr)
      this->speed_number_->publish_state(this->desired_rpm_);
    return;
  }

  this->desired_rpm_ = requested;
  this->enqueue_demand_(requested);
}

void CenturyVSPump::set_maintenance(bool active) {
  if (active == this->maintenance_active_ && !active)
    return;

  this->maintenance_active_ = active;
  if (active) {
    ESP_LOGW(TAG, "Physical maintenance inhibit ACTIVE: forcing pump stop");
    this->clear_tx_queue_for_device();
    this->queue_.clear();
    this->command_in_flight_ = false;
    this->enqueue_front_({CommandKind::STOP, FUNC_STOP, {}});
    if (this->run_switch_ != nullptr)
      this->run_switch_->publish_state(false);
  } else {
    ESP_LOGI(TAG, "Physical maintenance inhibit released; pump remains stopped");
  }
}

void CenturyVSPump::enqueue_(Command command) { this->queue_.push_back(std::move(command)); }

void CenturyVSPump::enqueue_front_(Command command) { this->queue_.push_front(std::move(command)); }

void CenturyVSPump::enqueue_status_() { this->enqueue_({CommandKind::STATUS, FUNC_STATUS, {}}); }

void CenturyVSPump::enqueue_sensor_(uint8_t address) {
  this->enqueue_({CommandKind::READ_SENSOR, FUNC_READ_SENSOR, {0x00, address}, address});
}

void CenturyVSPump::enqueue_stop_() { this->enqueue_({CommandKind::STOP, FUNC_STOP, {}}); }

void CenturyVSPump::enqueue_demand_(uint16_t rpm) {
  uint16_t scaled = rpm * 4;
  this->enqueue_({CommandKind::SET_DEMAND,
                  FUNC_SET_DEMAND,
                  {0x00, static_cast<uint8_t>(scaled & 0xFF), static_cast<uint8_t>(scaled >> 8)}});
}

void CenturyVSPump::send_next_() {
  if (this->command_in_flight_ || this->queue_.empty() || !this->ready_for_immediate_send())
    return;

  auto &command = this->queue_.front();
  std::vector<uint8_t> pdu{command.function, ACK_REQUEST};
  pdu.insert(pdu.end(), command.payload.begin(), command.payload.end());
  if (this->queue_pdu(pdu)) {
    this->command_in_flight_ = true;
  } else {
    ESP_LOGW(TAG, "Pump command 0x%02X was refused by the Modbus queue", command.function);
  }
}

void CenturyVSPump::finish_command_() {
  if (!this->queue_.empty())
    this->queue_.pop_front();
  this->command_in_flight_ = false;
}

void CenturyVSPump::on_response(std::span<const uint8_t> request_pdu,
                               std::span<const uint8_t> response_pdu) {
  if (this->queue_.empty()) {
    this->command_in_flight_ = false;
    ESP_LOGW(TAG, "Received response with no matching command");
    return;
  }

  const Command command = this->queue_.front();
  if (request_pdu.empty() || request_pdu[0] != command.function || response_pdu.empty() ||
      response_pdu[0] != command.function) {
    ESP_LOGW(TAG, "Response did not match queued function 0x%02X", command.function);
    this->finish_command_();
    return;
  }

  // ESPHome 2026.8 supplies the complete response PDU for custom function
  // codes. The Century payload begins immediately after the function byte.
  const auto data = response_pdu.subspan(1);
  if (data.empty() || data[0] != ACK_RESPONSE) {
    ESP_LOGW(TAG, "Function 0x%02X returned an invalid acknowledgement", command.function);
    this->finish_command_();
    return;
  }

  switch (command.kind) {
    case CommandKind::STATUS:
      if (data.size() >= 2)
        this->publish_status_(data[1]);
      break;
    case CommandKind::READ_SENSOR:
      if (data.size() >= 5 && data[1] == 0x00 && data[2] == command.sensor_address) {
        uint16_t raw = static_cast<uint16_t>(data[3]) | (static_cast<uint16_t>(data[4]) << 8);
        this->handle_sensor_(command.sensor_address, raw);
      } else {
        ESP_LOGW(TAG, "Malformed sensor response for address 0x%02X", command.sensor_address);
      }
      break;
    case CommandKind::GO:
      this->running_ = true;
      if (this->run_switch_ != nullptr)
        this->run_switch_->publish_state(true);
      break;
    case CommandKind::STOP:
      this->running_ = false;
      if (this->run_switch_ != nullptr)
        this->run_switch_->publish_state(false);
      break;
    case CommandKind::SET_DEMAND:
      if (this->speed_number_ != nullptr)
        this->speed_number_->publish_state(this->desired_rpm_);
      break;
  }

  this->finish_command_();
}

void CenturyVSPump::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  const uint8_t function_code = request_pdu.empty() ? 0 : request_pdu[0];
  ESP_LOGW(TAG, "Modbus exception 0x%02X for function 0x%02X", static_cast<uint8_t>(exception_code), function_code);
  this->finish_command_();
}

void CenturyVSPump::on_not_sent(std::span<const uint8_t> request_pdu) {
  const uint8_t function_code = request_pdu.empty() ? 0 : request_pdu[0];
  ESP_LOGW(TAG, "Pump command 0x%02X was dropped before transmission", function_code);
  this->finish_command_();
}

bool CenturyVSPump::on_no_response(std::span<const uint8_t> request_pdu) {
  if (this->queue_.empty()) {
    this->command_in_flight_ = false;
    return false;
  }
  auto &command = this->queue_.front();
  const uint8_t function_code = request_pdu.empty() ? command.function : request_pdu[0];
  if (++command.retries < MAX_RETRIES) {
    ESP_LOGW(TAG, "No response to function 0x%02X; retry %u/%u", function_code, command.retries,
             MAX_RETRIES - 1);
    return true;
  }
  ESP_LOGW(TAG, "No response to function 0x%02X after %u attempts", function_code, MAX_RETRIES);
  this->finish_command_();
  return false;
}

void CenturyVSPump::handle_sensor_(uint8_t address, uint16_t raw) {
  switch (address) {
    case SENSOR_RPM:
      if (this->rpm_sensor_ != nullptr)
        this->rpm_sensor_->publish_state(raw / 4.0f);
      break;
    case SENSOR_CURRENT:
      if (this->current_sensor_ != nullptr)
        this->current_sensor_->publish_state(raw / 1000.0f);
      break;
    case SENSOR_DEMAND:
      this->desired_rpm_ = raw / 4;
      if (this->speed_number_ != nullptr)
        this->speed_number_->publish_state(this->desired_rpm_);
      break;
    case SENSOR_INPUT_POWER:
      if (this->input_power_sensor_ != nullptr)
        this->input_power_sensor_->publish_state(raw);
      break;
    case SENSOR_OUTPUT_POWER:
      if (this->output_power_sensor_ != nullptr)
        this->output_power_sensor_->publish_state(raw);
      break;
    case SENSOR_DC_BUS_VOLTAGE:
      if (this->dc_bus_voltage_sensor_ != nullptr)
        this->dc_bus_voltage_sensor_->publish_state(raw / 64.0f);
      break;
    case SENSOR_DRIVE_TEMPERATURE:
      if (this->drive_temperature_sensor_ != nullptr)
        this->drive_temperature_sensor_->publish_state(static_cast<int16_t>(raw) / 128.0f);
      break;
    case SENSOR_FAULT:
      if (this->fault_code_sensor_ != nullptr)
        this->fault_code_sensor_->publish_state(raw);
      break;
    default:
      break;
  }
}

void CenturyVSPump::publish_status_(uint8_t status) {
  this->running_ = status == 0x0B;
  if (this->run_switch_ != nullptr)
    this->run_switch_->publish_state(this->running_);
  if (this->status_text_sensor_ != nullptr)
    this->status_text_sensor_->publish_state(this->status_to_string_(status));
}

const char *CenturyVSPump::status_to_string_(uint8_t status) {
  switch (status) {
    case 0x00:
      return "Stopped";
    case 0x09:
      return "Starting";
    case 0x0B:
      return "Running";
    case 0x20:
      return "Fault";
    default:
      return "Unknown";
  }
}

}  // namespace esphome::century_vspump
