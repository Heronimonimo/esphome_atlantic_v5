#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::aquamqtt {

static constexpr size_t HEADER_LENGTH = 5;
static constexpr size_t MAX_FRAME_SIZE = 29;
static constexpr size_t MAX_TEXT_SIZE = MAX_FRAME_SIZE - HEADER_LENGTH - 1 - 2;

class AquaMQTT;

struct Frame {
  uint8_t data[MAX_FRAME_SIZE]{};
  uint8_t length{0};
  bool modified{false};

  const uint8_t *payload() const { return &data[HEADER_LENGTH + 1]; }
  uint8_t payload_size() const { return length <= HEADER_LENGTH + 2 ? 0 : length - HEADER_LENGTH - 3; }
  uint64_t header() const;
  void replace_payload(const uint8_t *payload);
};

enum class Channel : uint8_t { LISTENER, HMI, MAIN };

enum class SensorType : uint8_t {
  WATER_TEMPERATURE,
  WATER_TEMPERATURE_MIN,
  WATER_TEMPERATURE_MAX,
  COMPRESSOR_OUTLET_TEMPERATURE,
  COMPRESSOR_OUTLET_TEMPERATURE_MIN,
  COMPRESSOR_OUTLET_TEMPERATURE_MAX,
  AIR_INLET_TEMPERATURE,
  AIR_INLET_TEMPERATURE_MIN,
  AIR_INLET_TEMPERATURE_MAX,
  EVAPORATOR1_TEMPERATURE,
  EVAPORATOR2_TEMPERATURE,
  EVAPORATOR3_TEMPERATURE,
  SETPOINT,
  CYCLE1_COUNT,
  CYCLE2_COUNT,
  CYCLE3_COUNT,
  CYCLE4_COUNT,
  CYCLE5_COUNT,
  CYCLE6_COUNT,
  FRAMES_RECEIVED,
  FRAMES_RELAYED,
  CRC_ERRORS,
  INVALID_FRAMES,
  RX_BYTES,
  TX_BYTES,
};

enum class BinarySensorType : uint8_t {
  INPUT_I1,
  INPUT_I2,
  HEATING_ACTIVE,
  CYCLE1_ACTIVE,
  CYCLE2_ACTIVE,
  CYCLE3_ACTIVE,
  CYCLE4_ACTIVE,
  CYCLE5_ACTIVE,
  CYCLE6_ACTIVE,
  CONNECTED,
};

enum class TextSensorType : uint8_t {
  SERIAL_NUMBER,
  CONTROLLER_MODEL,
  POWER_BOARD_VERSION,
  HMI_VERSION,
  HMI_MODEL,
};

class AquaMQTTSensor final : public sensor::Sensor {
 public:
  void set_parent(AquaMQTT *parent) { parent_ = parent; }
  void set_type(SensorType type) { type_ = type; }
 protected:
  AquaMQTT *parent_{nullptr};
  SensorType type_{SensorType::WATER_TEMPERATURE};
};

class AquaMQTTBinarySensor final : public binary_sensor::BinarySensor {
 public:
  void set_parent(AquaMQTT *parent) { parent_ = parent; }
  void set_type(BinarySensorType type) { type_ = type; }
 protected:
  AquaMQTT *parent_{nullptr};
  BinarySensorType type_{BinarySensorType::CONNECTED};
};

class AquaMQTTTextSensor final : public text_sensor::TextSensor {
 public:
  void set_parent(AquaMQTT *parent) { parent_ = parent; }
  void set_type(TextSensorType type) { type_ = type; }
 protected:
  AquaMQTT *parent_{nullptr};
  TextSensorType type_{TextSensorType::SERIAL_NUMBER};
};

class AquaOperationMode final : public select::Select {
 public:
  void set_parent(AquaMQTT *parent) { parent_ = parent; }
 protected:
  void control(size_t index) override;
  AquaMQTT *parent_{nullptr};
};

class AquaMQTT final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_mode(const std::string &mode) { mode_ = mode; }
  void set_frame_silence_ms(uint32_t value) { frame_silence_ms_ = value; }
  void set_one_wire_uart(bool value) { one_wire_uart_ = value; }
  void set_listener_uart(uart::UARTComponent *uart) { listener_uart_ = uart; }
  void set_main_uart(uart::UARTComponent *uart) { main_uart_ = uart; }
  void set_hmi_uart(uart::UARTComponent *uart) { hmi_uart_ = uart; }
  void set_main_tx_pin(InternalGPIOPin *pin) { main_tx_pin_ = pin; }
  void set_main_rx_pin(InternalGPIOPin *pin) { main_rx_pin_ = pin; }
  void set_hmi_tx_pin(InternalGPIOPin *pin) { hmi_tx_pin_ = pin; }
  void set_hmi_rx_pin(InternalGPIOPin *pin) { hmi_rx_pin_ = pin; }
  void set_main_enable_tx_pin(InternalGPIOPin *pin) { main_enable_tx_pin_ = pin; }
  void set_hmi_enable_tx_pin(InternalGPIOPin *pin) { hmi_enable_tx_pin_ = pin; }

  void register_sensor(AquaMQTTSensor *sensor, SensorType type);
  void register_binary_sensor(AquaMQTTBinarySensor *sensor, BinarySensorType type);
  void register_text_sensor(AquaMQTTTextSensor *sensor, TextSensorType type);
  void register_operation_mode(AquaOperationMode *select);

  void set_operation_mode(size_t index);
  size_t operation_mode() const { return operation_mode_; }

  static uint16_t crc16_modbus(const uint8_t *data, size_t len);

 private:
  struct RxBuffer {
    uint8_t data[MAX_FRAME_SIZE]{};
    uint8_t length{0};
    uint32_t last_byte_ms{0};
    bool active{false};
  };

  void listener_loop_();
  void mitm_loop_();
  void consume_uart_(uart::UARTComponent *uart, Channel channel, RxBuffer &buffer, uart::UARTComponent *destination);
  bool frame_ready_(Channel channel, const uint8_t *data, uint8_t len) const;
  bool process_frame_(Channel channel, uint8_t *data, uint8_t len);
  bool process_protocol_(Channel channel, Frame &frame);
  void forward_(Channel channel, Frame &frame, uart::UARTComponent *destination);
  void setup_one_wire_();
  void prepare_tx_(Channel channel);
  void restore_rx_(Channel channel);

  static bool valid_crc_(const uint8_t *data, size_t len);
  static float parse_temperature_(const uint8_t *data);
  static uint32_t parse_uint32_(const uint8_t *data);
  static void safe_text_(char *out, size_t out_size, const uint8_t *data, size_t len);

  void publish_sensor_(SensorType type, float value);
  void publish_binary_(BinarySensorType type, bool value);
  void publish_text_(TextSensorType type, const uint8_t *data, size_t len);
  void publish_cycle_(uint8_t cycle, const uint8_t *payload);
  void publish_connected_();

  const char *channel_name_(Channel channel) const;

  std::string mode_{"listener"};
  uint32_t frame_silence_ms_{4};
  bool one_wire_uart_{true};
  size_t operation_mode_{0};

  uart::UARTComponent *listener_uart_{nullptr};
  uart::UARTComponent *main_uart_{nullptr};
  uart::UARTComponent *hmi_uart_{nullptr};

  InternalGPIOPin *main_tx_pin_{nullptr};
  InternalGPIOPin *main_rx_pin_{nullptr};
  InternalGPIOPin *hmi_tx_pin_{nullptr};
  InternalGPIOPin *hmi_rx_pin_{nullptr};
  InternalGPIOPin *main_enable_tx_pin_{nullptr};
  InternalGPIOPin *hmi_enable_tx_pin_{nullptr};

  RxBuffer listener_buffer_{};
  RxBuffer hmi_buffer_{};
  RxBuffer main_buffer_{};

  std::array<sensor::Sensor *, 25> sensors_{};
  std::array<binary_sensor::BinarySensor *, 10> binary_sensors_{};
  std::array<text_sensor::TextSensor *, 5> text_sensors_{};
  AquaOperationMode *operation_mode_select_{nullptr};

  uint32_t frames_received_{0};
  uint32_t frames_relayed_{0};
  uint32_t crc_errors_{0};
  uint32_t invalid_frames_{0};
  uint32_t rx_bytes_{0};
  uint32_t tx_bytes_{0};
  uint32_t last_valid_frame_ms_{0};
  uint32_t last_diag_publish_ms_{0};
};

}  // namespace esphome::aquamqtt
