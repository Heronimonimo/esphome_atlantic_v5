import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@wowtor"]
DOMAIN = "aquamqtt"
DEPENDENCIES = ["esp32", "uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "select"]

ns = cg.esphome_ns.namespace("aquamqtt")
AquaMQTT = ns.class_("AquaMQTT", cg.Component)
SensorType = ns.enum("SensorType")
BinarySensorType = ns.enum("BinarySensorType")
TextSensorType = ns.enum("TextSensorType")

CONF_MODE = "mode"
CONF_UART_ID = "uart_id"
CONF_MAIN_UART_ID = "main_uart_id"
CONF_HMI_UART_ID = "hmi_uart_id"
CONF_FRAME_SILENCE_MS = "frame_silence_ms"
CONF_MAIN_TX_PIN = "main_tx_pin"
CONF_MAIN_RX_PIN = "main_rx_pin"
CONF_HMI_TX_PIN = "hmi_tx_pin"
CONF_HMI_RX_PIN = "hmi_rx_pin"
CONF_MAIN_ENABLE_TX_PIN = "main_enable_tx_pin"
CONF_HMI_ENABLE_TX_PIN = "hmi_enable_tx_pin"
CONF_ONE_WIRE = "one_wire_uart"

MODE_LISTENER = "listener"
MODE_MITM = "mitm"


def _validate(config):
    mode = config[CONF_MODE]
    if mode == MODE_LISTENER:
        if CONF_UART_ID not in config:
            raise cv.Invalid("listener mode requires uart_id")
    else:
        required = (
            CONF_MAIN_UART_ID,
            CONF_HMI_UART_ID,
            CONF_MAIN_TX_PIN,
            CONF_MAIN_RX_PIN,
            CONF_HMI_TX_PIN,
            CONF_HMI_RX_PIN,
            CONF_MAIN_ENABLE_TX_PIN,
            CONF_HMI_ENABLE_TX_PIN,
        )
        missing = [key for key in required if key not in config]
        if missing:
            raise cv.Invalid("MITM mode requires: " + ", ".join(missing))
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AquaMQTT),
            cv.Required(CONF_MODE): cv.one_of(MODE_LISTENER, MODE_MITM, lower=True),
            cv.Optional(CONF_FRAME_SILENCE_MS, default=4): cv.int_range(min=1, max=50),
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_MAIN_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_HMI_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_MAIN_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_MAIN_RX_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_HMI_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_HMI_RX_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_MAIN_ENABLE_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_HMI_ENABLE_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_ONE_WIRE, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_frame_silence_ms(config[CONF_FRAME_SILENCE_MS]))
    cg.add(var.set_one_wire_uart(config[CONF_ONE_WIRE]))

    if CONF_UART_ID in config:
        uart_var = await cg.get_variable(config[CONF_UART_ID])
        cg.add(var.set_listener_uart(uart_var))

    if CONF_MAIN_UART_ID in config:
        main_uart = await cg.get_variable(config[CONF_MAIN_UART_ID])
        cg.add(var.set_main_uart(main_uart))
    if CONF_HMI_UART_ID in config:
        hmi_uart = await cg.get_variable(config[CONF_HMI_UART_ID])
        cg.add(var.set_hmi_uart(hmi_uart))

    for key, setter in (
        (CONF_MAIN_TX_PIN, "set_main_tx_pin"),
        (CONF_MAIN_RX_PIN, "set_main_rx_pin"),
        (CONF_HMI_TX_PIN, "set_hmi_tx_pin"),
        (CONF_HMI_RX_PIN, "set_hmi_rx_pin"),
        (CONF_MAIN_ENABLE_TX_PIN, "set_main_enable_tx_pin"),
        (CONF_HMI_ENABLE_TX_PIN, "set_hmi_enable_tx_pin"),
    ):
        if key in config:
            pin = await cg.gpio_pin_expression(config[key])
            cg.add(getattr(var, setter)(pin))
