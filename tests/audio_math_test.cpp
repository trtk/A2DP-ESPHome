#include "audio_math.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
using namespace esphome::kallsup_chime;
int main() {
  uint8_t address[6]{};
  assert(parse_address("AA:BB:CC:DD:EE:FF", address));
  assert(address[0] == 0xAA && address[5] == 0xFF);
  assert(parse_address("aa:bb:cc:dd:ee:ff", address));
  for (const char *bad : {"", "AA:BB", "GG:BB:CC:DD:EE:FF", "AA-BB-CC-DD-EE-FF", "AA:BB:CC:DD:EE:FFx"})
    assert(!parse_address(bad, address));
  uint8_t pcm[] = {0x00,0x80, 0xFF,0x7F, 0xD2,0x04, 0x2E,0xFB};
  uint8_t original[sizeof(pcm)]; std::memcpy(original, pcm, sizeof(pcm));
  assert(gain_for_percent(0) == 0 && gain_for_percent(100) == 4096);
  assert(gain_for_percent(47) > 350 && gain_for_percent(47) < 500);
  for (unsigned p = 1; p <= 100; ++p) assert(gain_for_percent(p) >= gain_for_percent(p - 1));
  apply_gain(pcm, sizeof(pcm), 4096);
  assert(std::memcmp(pcm, original, sizeof(pcm)) == 0);
  apply_gain(pcm, sizeof(pcm), 2048);
  assert(pcm[0] == 0x00 && pcm[1] == 0xC0);  // -32768 -> -16384
  assert(pcm[2] == 0xFF && pcm[3] == 0x3F);  // 32767 -> 16383
  assert(pcm[4] == 0x69 && pcm[5] == 0x02);  // 1234 -> 617
  assert(pcm[6] == 0x97 && pcm[7] == 0xFD);  // -1234 -> -617
  apply_gain(pcm, sizeof(pcm), 0);
  for (auto value : pcm) assert(value == 0);
  uint8_t odd[] = {0xD2,0x04,0xAF};
  apply_gain(odd, sizeof(odd), 2048);
  assert(odd[2] == 0xAF);
}
