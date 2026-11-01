#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace esphome { namespace kallsup_chime {
inline bool parse_address(const char *text, uint8_t *address) {
  if (std::strlen(text) != 17) return false;
  auto hex = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (unsigned i = 0; i < 6; ++i) {
    int hi = hex(text[i * 3]), lo = hex(text[i * 3 + 1]);
    if (hi < 0 || lo < 0 || (i < 5 && text[i * 3 + 2] != ':')) return false;
    address[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}
// Match the previous ESP32-A2DP exponential volume curve. Compute on changes,
// not in the PCM callback; a 47% slider must not suddenly become much louder.
inline unsigned gain_for_percent(unsigned percent) {
  if (percent == 0) return 0;
  if (percent >= 100) return 4096;
  const unsigned raw = (percent * 127u + 50u) / 100u;
  const float offset = std::pow(1.4f, -12.0f);
  const float gain = (std::pow(1.4f, raw * 12.0f / 127.0f - 12.0f) - offset) * 4096.0f / (1.0f - offset);
  return static_cast<unsigned>(gain);
}
inline void apply_gain(uint8_t *data, size_t length, unsigned gain_q12) {
  if (gain_q12 > 4096) gain_q12 = 4096;
  for (size_t i = 0; i + 1 < length; i += 2) {
    int16_t sample = static_cast<int16_t>(static_cast<uint16_t>(data[i]) | (static_cast<uint16_t>(data[i + 1]) << 8));
    int16_t scaled = static_cast<int16_t>((static_cast<int32_t>(sample) * static_cast<int32_t>(gain_q12)) / 4096);
    data[i] = static_cast<uint8_t>(scaled);
    data[i + 1] = static_cast<uint8_t>(static_cast<uint16_t>(scaled) >> 8);
  }
}
} }
