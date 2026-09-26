import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
)
from esphome.core import CORE

from . import AquaMQTT, SensorType

AquaMQTTSensor = ns.class_("AquaMQTTSensor", sensor.Sensor)
CONF_AQUAMQTT_ID = "aquamqtt_id"
CONF_TYPE = "type"

SENSOR_TYPES = {
    "water_temperature": ("WATER_TEMPERATURE", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "water_temperature_min": ("WATER_TEMPERATURE_MIN", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "water_temperature_max": ("WATER_TEMPERATURE_MAX", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "compressor_outlet_temperature": ("COMPRESSOR_OUTLET_TEMPERATURE", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "compressor_outlet_temperature_min": ("COMPRESSOR_OUTLET_TEMPERATURE_MIN", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "compressor_outlet_temperature_max": ("COMPRESSOR_OUTLET_TEMPERATURE_MAX", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "air_inlet_temperature": ("AIR_INLET_TEMPERATURE", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "air_inlet_temperature_min": ("AIR_INLET_TEMPERATURE_MIN", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "air_inlet_temperature_max": ("AIR_INLET_TEMPERATURE_MAX", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "evaporator1_temperature": ("EVAPORATOR1_TEMPERATURE", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "evaporator2_temperature": ("EVAPORATOR2_TEMPERATURE", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "evaporator3_temperature": ("EVAPORATOR3_TEMPERATURE", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "setpoint": ("SETPOINT", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT, 2, None),
    "cycle1_count": ("CYCLE1_COUNT", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "cycle2_count": ("CYCLE2_COUNT", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "cycle3_count": ("CYCLE3_COUNT", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "cycle4_count": ("CYCLE4_COUNT", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "cycle5_count": ("CYCLE5_COUNT", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "cycle6_count": ("CYCLE6_COUNT", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "frames_received": ("FRAMES_RECEIVED", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "frames_relayed": ("FRAMES_RELAYED", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "crc_errors": ("CRC_ERRORS", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "invalid_frames": ("INVALID_FRAMES", None, None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "rx_bytes": ("RX_BYTES", "B", None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
    "tx_bytes": ("TX_BYTES", "B", None, STATE_CLASS_TOTAL_INCREASING, 0, ENTITY_CATEGORY_DIAGNOSTIC),
}

SCHEMAS = {}
for key, (enum_name, unit, device_class, state_class, accuracy, category) in SENSOR_TYPES.items():
    kwargs = {"accuracy_decimals": accuracy}
    if unit is not None:
        kwargs["unit_of_measurement"] = unit
    if device_class is not None:
        kwargs["device_class"] = device_class
    if state_class is not None:
        kwargs["state_class"] = state_class
    if category is not None:
        kwargs["entity_category"] = category
    SCHEMAS[key] = sensor.sensor_schema(AquaMQTTSensor, **kwargs).extend(
        {
            cv.Required(CONF_AQUAMQTT_ID): cv.use_id(AquaMQTT),
        }
    )

CONFIG_SCHEMA = cv.typed_schema(SCHEMAS, key=CONF_TYPE)

async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_AQUAMQTT_ID])
    enum_name = SENSOR_TYPES[config[CONF_TYPE]][0]
    cg.add(var.set_parent(parent))
    enum = getattr(SensorType, enum_name)
    cg.add(var.set_type(enum))
    cg.add(parent.register_sensor(var, enum))
