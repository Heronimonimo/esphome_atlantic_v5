import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID
from . import AquaMQTT, ns

AquaOperationMode = ns.class_("AquaOperationMode", select.Select)
CONF_AQUAMQTT_ID = "aquamqtt_id"
OPTIONS = ["use input", "normal", "eager", "off", "boost"]

CONFIG_SCHEMA = select.select_schema(AquaOperationMode).extend({
    cv.Required(CONF_AQUAMQTT_ID): cv.use_id(AquaMQTT),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=OPTIONS)
    parent = await cg.get_variable(config[CONF_AQUAMQTT_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.register_operation_mode(var))
