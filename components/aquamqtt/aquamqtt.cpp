#include "aquamqtt.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <soc/uart_periph.h>
#include "esphome/components/uart/uart_component_esp_idf.h"
#endif

namespace esphome::aquamqtt {

static const char *const TAG = "aquamqtt";
static constexpr const char *const OPERATION_MODES[] = {"use input", "normal", "eager", "off", "boost"};

uint64_t Frame::header() const {
  uint64_t value = 0;
  for (size_t i = 0; i < HEADER_LENGTH; i++) value = (value << 8) | data[i];
  return value;
}

void Frame::replace_payload(const uint8_t *payload) {
  const auto size = payload_size();
  if (size == 0) return;
  memcpy(&data[HEADER_LENGTH + 1], payload, size);
  const uint16_t crc = AquaMQTT::crc16_modbus(data, length - 2);
  data[length - 2] = static_cast<uint8_t>(crc & 0xff);
  data[length - 1] = static_cast<uint8_t>(crc >> 8);
  modified = true;
}

void AquaOperationMode::control(size_t index) {
  if (parent_ != nullptr) parent_->set_operation_mode(index);
}

uint16_t AquaMQTT::crc16_modbus(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; pos++) {
    crc ^= data[pos];
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

bool AquaMQTT::valid_crc_(const uint8_t *data, size_t len) {
  if (len < HEADER_LENGTH + 2) return false;
  const uint16_t expected = static_cast<uint16_t>(data[len - 2]) | (static_cast<uint16_t>(data[len - 1]) << 8);
  return crc16_modbus(data, len - 2) == expected;
}

float AquaMQTT::parse_temperature_(const uint8_t *data) {
  const int16_t value = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
  return static_cast<float>(value) / 100.0f;
}

uint32_t AquaMQTT::parse_uint32_(const uint8_t *data) {
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

void AquaMQTT::safe_text_(char *out, size_t out_size, const uint8_t *data, size_t len) {
  if (out_size == 0) return;
  const size_t n = std::min(len, out_size - 1);
  memcpy(out, data, n);
  out[n] = '\0';
}

void AquaMQTT::register_sensor(AquaMQTTSensor *sensor, SensorType type) {
  sensor->set_parent(this);
  sensor->set_type(type);
  sensors_[static_cast<size_t>(type)] = sensor;
}

void AquaMQTT::register_binary_sensor(AquaMQTTBinarySensor *sensor, BinarySensorType type) {
  sensor->set_parent(this);
  sensor->set_type(type);
  binary_sensors_[static_cast<size_t>(type)] = sensor;
}

void AquaMQTT::register_text_sensor(AquaMQTTTextSensor *sensor, TextSensorType type) {
  sensor->set_parent(this);
  sensor->set_type(type);
  text_sensors_[static_cast<size_t>(type)] = sensor;
}

void AquaMQTT::register_operation_mode(AquaOperationMode *select) {
  operation_mode_select_ = select;
  select->set_parent(this);
}

void AquaMQTT::set_operation_mode(size_t index) {
  if (index >= 5) return;
  operation_mode_ = index;
  if (operation_mode_select_ != nullptr) operation_mode_select_->publish_state(index);
  ESP_LOGD(TAG, "Operation mode: %s", OPERATION_MODES[index]);
}

void AquaMQTT::setup() {
  if (mode_ == "listener") {
    if (listener_uart_ == nullptr) {
      ESP_LOGE(TAG, "Listener UART is not configured");
      mark_failed();
      return;
    }
    listener_uart_->set_rx_full_threshold_ms(2);
  } else {
    if (main_uart_ == nullptr || hmi_uart_ == nullptr) {
      ESP_LOGE(TAG, "MITM UARTs are not configured");
      mark_failed();
      return;
    }
    if (main_enable_tx_pin_ != nullptr) {
      main_enable_tx_pin_->setup();
      main_enable_tx_pin_->digital_write(false);
    }
    if (hmi_enable_tx_pin_ != nullptr) {
      hmi_enable_tx_pin_->setup();
      hmi_enable_tx_pin_->digital_write(false);
    }
    setup_one_wire_();
    main_uart_->set_rx_full_threshold_ms(2);
    hmi_uart_->set_rx_full_threshold_ms(2);
  }

  if (operation_mode_select_ != nullptr) operation_mode_select_->publish_state(operation_mode_);
  last_valid_frame_ms_ = millis();
  ESP_LOGI(TAG, "AquaMQTT v5 ready (%s)", mode_.c_str());
}

void AquaMQTT::loop() {
  if (mode_ == "listener") listener_loop_();
  else mitm_loop_();
  publish_connected_();
}

void AquaMQTT::listener_loop_() {
  if (listener_uart_ == nullptr) return;
  while (listener_uart_->available()) {
    uint8_t byte;
    if (!listener_uart_->read_byte(&byte)) break;
    rx_bytes_++;
    auto &b = listener_buffer_;
    if (b.length >= MAX_FRAME_SIZE) {
      b.length = 0;
      b.active = false;
      invalid_frames_++;
    }
    b.data[b.length++] = byte;
    b.active = true;
    b.last_byte_ms = millis();
    if (frame_ready_(Channel::LISTENER, b.data, b.length)) {
      if (process_frame_(Channel::LISTENER, b.data, b.length)) frames_received_++;
      b.length = 0;
      b.active = false;
    }
  }
  if (listener_buffer_.active && listener_buffer_.length > 0 && millis() - listener_buffer_.last_byte_ms >= frame_silence_ms_) {
    if (process_frame_(Channel::LISTENER, listener_buffer_.data, listener_buffer_.length)) frames_received_++;
    listener_buffer_.length = 0;
    listener_buffer_.active = false;
  }
}

void AquaMQTT::consume_uart_(uart::UARTComponent *uart, Channel channel, RxBuffer &buffer, uart::UARTComponent *destination) {
  if (uart == nullptr) return;
  while (uart->available()) {
    uint8_t byte;
    if (!uart->read_byte(&byte)) break;
    rx_bytes_++;
    const uint32_t now = millis();
    if (buffer.active && now - buffer.last_byte_ms >= frame_silence_ms_) {
      if (buffer.length > 0) {
        Frame frame;
        frame.length = buffer.length;
        memcpy(frame.data, buffer.data, buffer.length);
        if (process_frame_(channel, frame.data, frame.length)) forward_(channel, frame, destination);
        else invalid_frames_++;
      }
      buffer.length = 0;
      buffer.active = false;
    }
    if (buffer.length >= MAX_FRAME_SIZE) {
      buffer.length = 0;
      buffer.active = false;
      invalid_frames_++;
    }
    buffer.data[buffer.length++] = byte;
    buffer.last_byte_ms = now;
    buffer.active = true;
    if (frame_ready_(channel, buffer.data, buffer.length)) {
      Frame frame;
      frame.length = buffer.length;
      memcpy(frame.data, buffer.data, buffer.length);
      if (process_frame_(channel, frame.data, frame.length)) forward_(channel, frame, destination);
      else invalid_frames_++;
      buffer.length = 0;
      buffer.active = false;
    }
  }
  if (buffer.active && buffer.length > 0 && millis() - buffer.last_byte_ms >= frame_silence_ms_) {
    Frame frame;
    frame.length = buffer.length;
    memcpy(frame.data, buffer.data, buffer.length);
    if (process_frame_(channel, frame.data, frame.length)) forward_(channel, frame, destination);
    else invalid_frames_++;
    buffer.length = 0;
    buffer.active = false;
  }
}

void AquaMQTT::mitm_loop_() {
  consume_uart_(hmi_uart_, Channel::HMI, hmi_buffer_, main_uart_);
  consume_uart_(main_uart_, Channel::MAIN, main_buffer_, hmi_uart_);
}

bool AquaMQTT::frame_ready_(Channel channel, const uint8_t *data, uint8_t len) const {
  if (len == 1) return data[0] != 0x01;
  if (len == 2) return data[1] != 0x64 && data[1] != 0x65;
  if (len < HEADER_LENGTH + 2) return false;
  if (len == HEADER_LENGTH + 2) {
    if (channel == Channel::HMI && data[1] == 0x64) return true;
    if (channel == Channel::MAIN && data[1] == 0x65) return true;
    return false;
  }
  const uint8_t payload_len = data[HEADER_LENGTH];
  const uint16_t frame_len = HEADER_LENGTH + 1 + payload_len + 2;
  return frame_len <= MAX_FRAME_SIZE && len >= frame_len;
}

bool AquaMQTT::process_frame_(Channel channel, uint8_t *data, uint8_t len) {
  if (len < HEADER_LENGTH + 2 || len > MAX_FRAME_SIZE) return false;
  if (data[0] != 0x01 || (data[1] != 0x64 && data[1] != 0x65)) return false;
  if (len > HEADER_LENGTH + 2 && static_cast<uint8_t>(data[HEADER_LENGTH]) != len - HEADER_LENGTH - 3) return false;
  if (!valid_crc_(data, len)) {
    crc_errors_++;
    return false;
  }
  Frame frame;
  frame.length = len;
  memcpy(frame.data, data, len);
  if (!process_protocol_(channel, frame)) return false;
  memcpy(data, frame.data, frame.length);
  last_valid_frame_ms_ = millis();
  return true;
}

bool AquaMQTT::process_protocol_(Channel channel, Frame &frame) {
  const uint64_t h = frame.header();
  const uint8_t *p = frame.payload();
  const uint8_t n = frame.payload_size();

  auto temp = [&](SensorType type, size_t off) { if (n >= off + 2) publish_sensor_(type, parse_temperature_(p + off)); };
  auto extremes = [&](SensorType min_type, SensorType max_type) {
    if (n == 5 && p[0] == 0x00) { publish_sensor_(min_type, parse_temperature_(p + 1)); publish_sensor_(max_type, parse_temperature_(p + 3)); }
  };

  switch (h) {
    case 0x0164006601ULL: publish_text_(TextSensorType::SERIAL_NUMBER, p, n); break;
    case 0x0164006701ULL: publish_text_(TextSensorType::POWER_BOARD_VERSION, p, n); break;
    case 0x0164006E01ULL: publish_text_(TextSensorType::CONTROLLER_MODEL, p, n); break;
    case 0x016414B701ULL: if (n == 2) temp(SensorType::SETPOINT, 0); break;
    case 0x0164FEB006ULL:
      if (n == 12) {
        temp(SensorType::WATER_TEMPERATURE, 0);
        temp(SensorType::COMPRESSOR_OUTLET_TEMPERATURE, 2);
        temp(SensorType::AIR_INLET_TEMPERATURE, 4);
        temp(SensorType::EVAPORATOR1_TEMPERATURE, 6);
        temp(SensorType::EVAPORATOR2_TEMPERATURE, 8);
        temp(SensorType::EVAPORATOR3_TEMPERATURE, 10);
      }
      break;
    case 0x0164FF1403ULL:
      if (n == 3) {
        publish_binary_(BinarySensorType::INPUT_I2, p[0] != 0);
        publish_binary_(BinarySensorType::INPUT_I1, p[1] != 0);
        publish_binary_(BinarySensorType::HEATING_ACTIVE, p[2] != 0);
        if (operation_mode_ != 0 && channel != Channel::LISTENER) {
          uint8_t modified[3] = {0, 0, p[2]};
          if (operation_mode_ == 2) modified[1] = 1;
          else if (operation_mode_ == 3) modified[0] = 1;
          else if (operation_mode_ == 4) { modified[0] = 1; modified[1] = 1; }
          frame.replace_payload(modified);
        }
      }
      break;
    case 0x0164FEBA03ULL: extremes(SensorType::WATER_TEMPERATURE_MIN, SensorType::WATER_TEMPERATURE_MAX); break;
    case 0x0164FEBD03ULL: extremes(SensorType::COMPRESSOR_OUTLET_TEMPERATURE_MIN, SensorType::COMPRESSOR_OUTLET_TEMPERATURE_MAX); break;
    case 0x0164FEC003ULL: extremes(SensorType::AIR_INLET_TEMPERATURE_MIN, SensorType::AIR_INLET_TEMPERATURE_MAX); break;
    case 0x0164FEE203ULL: publish_cycle_(1, p); break;
    case 0x0164FEE503ULL: publish_cycle_(2, p); break;
    case 0x0164FEE803ULL: publish_cycle_(3, p); break;
    case 0x0164FEEB03ULL: publish_cycle_(4, p); break;
    case 0x0164FEEE03ULL: publish_cycle_(5, p); break;
    case 0x0164FEF103ULL: publish_cycle_(6, p); break;
    case 0x0165000301ULL: publish_text_(TextSensorType::HMI_VERSION, p, n); break;
    case 0x0165000A01ULL: publish_text_(TextSensorType::HMI_MODEL, p, n); break;
    default: break;
  }
  return true;
}

void AquaMQTT::forward_(Channel channel, Frame &frame, uart::UARTComponent *destination) {
  if (destination == nullptr) return;
  if (mode_ == "mitm" && one_wire_uart_) prepare_tx_(channel);
  destination->write_array(frame.data, frame.length);
  destination->flush();
  tx_bytes_ += frame.length;
  frames_relayed_++;
  if (mode_ == "mitm" && one_wire_uart_) restore_rx_(channel);
}

void AquaMQTT::setup_one_wire_() {
#ifdef USE_ESP32
  if (!one_wire_uart_) return;
  if (main_enable_tx_pin_ != nullptr) main_enable_tx_pin_->digital_write(false);
  if (hmi_enable_tx_pin_ != nullptr) hmi_enable_tx_pin_->digital_write(false);
  if (main_tx_pin_ != nullptr) {
    main_tx_pin_->setup();
    const int gpio = main_tx_pin_->get_pin();
    esp_rom_gpio_connect_out_signal(gpio, SIG_GPIO_OUT_IDX, false, false);
    main_tx_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  if (hmi_tx_pin_ != nullptr) {
    hmi_tx_pin_->setup();
    const int gpio = hmi_tx_pin_->get_pin();
    esp_rom_gpio_connect_out_signal(gpio, SIG_GPIO_OUT_IDX, false, false);
    hmi_tx_pin_->pin_mode(gpio::FLAG_INPUT);
  }
#endif
}

void AquaMQTT::prepare_tx_(Channel channel) {
#ifdef USE_ESP32
  auto *dst = channel == Channel::HMI ? hmi_uart_ : main_uart_;
  auto *idfuart = static_cast<uart::IDFUARTComponent *>(dst);
  if (idfuart == nullptr) return;
  const int uart_num = idfuart->get_hw_serial_number();
  if (uart_num < 0 || uart_num >= SOC_UART_NUM) return;
  const int tx_signal = uart_periph_signal[uart_num].pins[SOC_UART_TX_PIN_IDX].signal;
  const int rx_signal = uart_periph_signal[uart_num].pins[SOC_UART_RX_PIN_IDX].signal;
  InternalGPIOPin *tx_pin = channel == Channel::HMI ? main_tx_pin_ : hmi_tx_pin_;
  InternalGPIOPin *rx_pin = channel == Channel::HMI ? main_rx_pin_ : hmi_rx_pin_;
  InternalGPIOPin *enable = channel == Channel::HMI ? main_enable_tx_pin_ : hmi_enable_tx_pin_;
  if (tx_pin == nullptr || rx_pin == nullptr || enable == nullptr) return;
  const int tx_gpio = tx_pin->get_pin();
  const int rx_gpio = rx_pin->get_pin();
  gpio_set_direction((gpio_num_t) tx_gpio, GPIO_MODE_OUTPUT);
  esp_rom_gpio_connect_out_signal(tx_gpio, tx_signal, false, false);
  gpio_set_direction((gpio_num_t) rx_gpio, GPIO_MODE_INPUT_OUTPUT);
  esp_rom_gpio_connect_out_signal(rx_gpio, tx_signal, false, false);
  delayMicroseconds(10);
  enable->digital_write(true);
#else
  (void) channel;
#endif
}

void AquaMQTT::restore_rx_(Channel channel) {
#ifdef USE_ESP32
  auto *dst = channel == Channel::HMI ? hmi_uart_ : main_uart_;
  auto *idfuart = static_cast<uart::IDFUARTComponent *>(dst);
  if (idfuart == nullptr) return;
  const int uart_num = idfuart->get_hw_serial_number();
  if (uart_num < 0 || uart_num >= SOC_UART_NUM) return;
  const int rx_signal = uart_periph_signal[uart_num].pins[SOC_UART_RX_PIN_IDX].signal;
  InternalGPIOPin *tx_pin = channel == Channel::HMI ? main_tx_pin_ : hmi_tx_pin_;
  InternalGPIOPin *rx_pin = channel == Channel::HMI ? main_rx_pin_ : hmi_rx_pin_;
  InternalGPIOPin *enable = channel == Channel::HMI ? main_enable_tx_pin_ : hmi_enable_tx_pin_;
  if (tx_pin == nullptr || rx_pin == nullptr || enable == nullptr) return;
  const int tx_gpio = tx_pin->get_pin();
  const int rx_gpio = rx_pin->get_pin();
  enable->digital_write(false);
  esp_rom_gpio_connect_out_signal(tx_gpio, SIG_GPIO_OUT_IDX, false, false);
  gpio_set_direction((gpio_num_t) tx_gpio, GPIO_MODE_INPUT);
  esp_rom_gpio_connect_out_signal(rx_gpio, SIG_GPIO_OUT_IDX, false, false);
  gpio_set_direction((gpio_num_t) rx_gpio, GPIO_MODE_INPUT);
  esp_rom_gpio_connect_in_signal(rx_gpio, rx_signal, false);
  delayMicroseconds(200);
  uint8_t ignored;
  while (dst->read_byte(&ignored)) {}
#else
  (void) channel;
#endif
}

void AquaMQTT::publish_sensor_(SensorType type, float value) {
  auto *s = sensors_[static_cast<size_t>(type)];
  if (s != nullptr) s->publish_state(value);
}

void AquaMQTT::publish_binary_(BinarySensorType type, bool value) {
  auto *s = binary_sensors_[static_cast<size_t>(type)];
  if (s != nullptr) s->publish_state(value);
}

void AquaMQTT::publish_text_(TextSensorType type, const uint8_t *data, size_t len) {
  auto *s = text_sensors_[static_cast<size_t>(type)];
  if (s == nullptr || len == 0 || data[len - 1] != 0) return;
  char text[MAX_TEXT_SIZE + 1]{};
  safe_text_(text, sizeof(text), data, len - 1);
  s->publish_state(text);
}

void AquaMQTT::publish_cycle_(uint8_t cycle, const uint8_t *payload) {
  if (cycle < 1 || cycle > 6) return;
  // 12-byte payload: seconds1, seconds2, total cycle count.
  // The upstream implementation uses seconds2 > 0 as the active flag.
  const uint32_t active_seconds = parse_uint32_(payload + 4);
  const uint32_t count = parse_uint32_(payload + 8);
  publish_binary_(static_cast<BinarySensorType>(static_cast<size_t>(BinarySensorType::CYCLE1_ACTIVE) + cycle - 1), active_seconds > 0);
  publish_sensor_(static_cast<SensorType>(static_cast<size_t>(SensorType::CYCLE1_COUNT) + cycle - 1), static_cast<float>(count));
}

void AquaMQTT::publish_connected_() {
  const bool connected = millis() - last_valid_frame_ms_ < 30000;
  publish_binary_(BinarySensorType::CONNECTED, connected);
  if (millis() - last_diag_publish_ms_ < 1000) return;
  last_diag_publish_ms_ = millis();
  publish_sensor_(SensorType::FRAMES_RECEIVED, static_cast<float>(frames_received_));
  publish_sensor_(SensorType::FRAMES_RELAYED, static_cast<float>(frames_relayed_));
  publish_sensor_(SensorType::CRC_ERRORS, static_cast<float>(crc_errors_));
  publish_sensor_(SensorType::INVALID_FRAMES, static_cast<float>(invalid_frames_));
  publish_sensor_(SensorType::RX_BYTES, static_cast<float>(rx_bytes_));
  publish_sensor_(SensorType::TX_BYTES, static_cast<float>(tx_bytes_));
}

void AquaMQTT::dump_config() {
  ESP_LOGCONFIG(TAG, "AquaMQTT v5:");
  ESP_LOGCONFIG(TAG, "  Mode: %s", mode_.c_str());
  ESP_LOGCONFIG(TAG, "  Frame silence: %u ms", frame_silence_ms_);
  if (mode_ == "mitm") ESP_LOGCONFIG(TAG, "  One-wire UART: %s", YESNO(one_wire_uart_));
}

}  // namespace esphome::aquamqtt
