import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import AquaMQTT, TextSensorType

AquaMQTTTextSensor = ns.class_("AquaMQTTTextSensor", text_sensor.TextSensor)
CONF_AQUAMQTT_ID = "aquamqtt_id"
CONF_TYPE = "type"
TYPES = {
    "serial_number": "SERIAL_NUMBER", "controller_model": "CONTROLLER_MODEL",
    "power_board_version": "POWER_BOARD_VERSION", "hmi_version": "HMI_VERSION", "hmi_model": "HMI_MODEL",
}
CONFIG_SCHEMA = cv.typed_schema({
    key: text_sensor.text_sensor_schema(AquaMQTTTextSensor).extend({cv.Required(CONF_AQUAMQTT_ID): cv.use_id(AquaMQTT)})
    for key in TYPES
}, key=CONF_TYPE)

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config[CONF_AQUAMQTT_ID])
    enum = getattr(TextSensorType, TYPES[config[CONF_TYPE]])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(enum))
    cg.add(parent.register_text_sensor(var, enum))
