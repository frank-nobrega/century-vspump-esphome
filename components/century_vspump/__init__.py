# SPDX-License-Identifier: GPL-3.0-only

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, modbus, number, sensor, switch, text_sensor
from esphome.const import CONF_ADDRESS, CONF_ID

AUTO_LOAD = ["binary_sensor", "modbus", "number", "sensor", "switch", "text_sensor"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True

CONF_MAINTENANCE_INPUT = "maintenance_input"
CONF_RUN = "run"
CONF_SPEED = "speed"
CONF_MOTOR_RPM = "motor_rpm"
CONF_MOTOR_CURRENT = "motor_current"
CONF_INPUT_POWER = "input_power"
CONF_OUTPUT_POWER = "output_power"
CONF_DC_BUS_VOLTAGE = "dc_bus_voltage"
CONF_DRIVE_TEMPERATURE = "drive_temperature"
CONF_FAULT_CODE = "fault_code"
CONF_STATUS = "status"

century_ns = cg.esphome_ns.namespace("century_vspump")
CenturyVSPump = century_ns.class_(
    "CenturyVSPump", cg.PollingComponent, modbus.ModbusClientDevice
)
CenturyVSPumpRunSwitch = century_ns.class_(
    "CenturyVSPumpRunSwitch", switch.Switch, cg.Component
)
CenturyVSPumpSpeedNumber = century_ns.class_(
    "CenturyVSPumpSpeedNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPump),
            cv.Required(CONF_MAINTENANCE_INPUT): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_RUN): switch.switch_schema(CenturyVSPumpRunSwitch),
            cv.Required(CONF_SPEED): number.number_schema(CenturyVSPumpSpeedNumber),
            cv.Required(CONF_MOTOR_RPM): sensor.sensor_schema(
                unit_of_measurement="rpm",
                accuracy_decimals=0,
                icon="mdi:rotate-right",
            ),
            cv.Required(CONF_MOTOR_CURRENT): sensor.sensor_schema(
                unit_of_measurement="A",
                accuracy_decimals=2,
                device_class="current",
                state_class="measurement",
            ),
            cv.Required(CONF_INPUT_POWER): sensor.sensor_schema(
                unit_of_measurement="W",
                accuracy_decimals=0,
                device_class="power",
                state_class="measurement",
            ),
            cv.Required(CONF_OUTPUT_POWER): sensor.sensor_schema(
                unit_of_measurement="W",
                accuracy_decimals=0,
                device_class="power",
                state_class="measurement",
            ),
            cv.Required(CONF_DC_BUS_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement="V",
                accuracy_decimals=1,
                device_class="voltage",
                state_class="measurement",
            ),
            cv.Required(CONF_DRIVE_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Required(CONF_FAULT_CODE): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:alert-circle-outline",
            ),
            cv.Required(CONF_STATUS): text_sensor.text_sensor_schema(
                icon="mdi:pump"
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x15, role="client"))
)

FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "century_vspump", role="client"
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)

    maintenance = await cg.get_variable(config[CONF_MAINTENANCE_INPUT])
    cg.add(var.set_maintenance_sensor(maintenance))

    run = await switch.new_switch(config[CONF_RUN])
    await cg.register_component(run, config[CONF_RUN])
    cg.add(run.set_parent(var))
    cg.add(var.set_run_switch(run))

    speed = await number.new_number(
        config[CONF_SPEED], min_value=600, max_value=3450, step=50
    )
    await cg.register_component(speed, config[CONF_SPEED])
    cg.add(speed.set_parent(var))
    cg.add(var.set_speed_number(speed))

    for key, setter in (
        (CONF_MOTOR_RPM, "set_rpm_sensor"),
        (CONF_MOTOR_CURRENT, "set_current_sensor"),
        (CONF_INPUT_POWER, "set_input_power_sensor"),
        (CONF_OUTPUT_POWER, "set_output_power_sensor"),
        (CONF_DC_BUS_VOLTAGE, "set_dc_bus_voltage_sensor"),
        (CONF_DRIVE_TEMPERATURE, "set_drive_temperature_sensor"),
        (CONF_FAULT_CODE, "set_fault_code_sensor"),
    ):
        entity = await sensor.new_sensor(config[key])
        cg.add(getattr(var, setter)(entity))

    status = await text_sensor.new_text_sensor(config[CONF_STATUS])
    cg.add(var.set_status_text_sensor(status))
