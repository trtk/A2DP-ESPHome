#include "chime_player.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
  const int16_t samples[] = {0, 32767, -32768, 1234};
  ChimePlayer p(samples, 4);
  uint8_t out[32];
  p.render(out, sizeof(out));
  for (auto c : out) assert(c == 0);
  assert(p.trigger());
  assert(!p.trigger());
  p.render(out, 8);
  assert(p.busy());
  assert(out[0] == 0 && out[4] == 255 && out[5] == 127);
  assert(out[4] == out[6] && out[5] == out[7]);
  p.render(out, sizeof(out));
  assert(!p.busy());
  assert(out[0] == 0 && out[1] == 128);  // Signed negative full scale.
  assert(out[4] == 210 && out[5] == 4);
  for (size_t i = 8; i < sizeof(out); ++i) assert(out[i] == 0);
  assert(p.trigger());
  p.cancel();
  p.render(out, sizeof(out));
  assert(!p.busy());
  for (auto c : out) assert(c == 0);
  assert(p.trigger());
  p.render(out, sizeof(out));
  assert(!p.busy());
  return 0;
}
