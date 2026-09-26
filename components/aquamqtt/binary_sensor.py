import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from . import AquaMQTT, BinarySensorType

AquaMQTTBinarySensor = ns.class_("AquaMQTTBinarySensor", binary_sensor.BinarySensor)
CONF_AQUAMQTT_ID = "aquamqtt_id"
CONF_TYPE = "type"
TYPES = {
    "input_i1": "INPUT_I1", "input_i2": "INPUT_I2", "heating_active": "HEATING_ACTIVE",
    "cycle1_active": "CYCLE1_ACTIVE", "cycle2_active": "CYCLE2_ACTIVE", "cycle3_active": "CYCLE3_ACTIVE",
    "cycle4_active": "CYCLE4_ACTIVE", "cycle5_active": "CYCLE5_ACTIVE", "cycle6_active": "CYCLE6_ACTIVE",
    "connected": "CONNECTED",
}
SCHEMAS = {}
for key, enum_name in TYPES.items():
    kwargs = {}
    if key == "connected":
        kwargs = {"device_class": DEVICE_CLASS_CONNECTIVITY, "entity_category": ENTITY_CATEGORY_DIAGNOSTIC}
    SCHEMAS[key] = binary_sensor.binary_sensor_schema(AquaMQTTBinarySensor, **kwargs).extend(
        {cv.Required(CONF_AQUAMQTT_ID): cv.use_id(AquaMQTT)}
    )
CONFIG_SCHEMA = cv.typed_schema(SCHEMAS, key=CONF_TYPE)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_AQUAMQTT_ID])
    enum = getattr(BinarySensorType, TYPES[config[CONF_TYPE]])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(enum))
    cg.add(parent.register_binary_sensor(var, enum))
