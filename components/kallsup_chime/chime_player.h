#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

// One producer (Arduino loop), one consumer (Bluetooth audio callback).
// PCM samples are const data in flash. No allocation or I/O in render().
class ChimePlayer {
 public:
  ChimePlayer(const int16_t* pcm, size_t count) : pcm_(pcm), count_(count), pos_(count) {}

  bool trigger() {
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) return false;
    requested_.store(true);
    return true;
  }
  bool busy() const { return busy_.load(); }
  void cancel() { cancelled_.store(true); }

  size_t render(uint8_t* out, size_t bytes) {
    std::memset(out, 0, bytes);
    if (cancelled_.exchange(false)) {
      requested_.store(false);
      pos_ = count_;
      busy_.store(false);
    } else if (requested_.exchange(false)) {
      pos_ = 0;
    }
    // Explicit little-endian stereo PCM, also works with unaligned buffers.
    for (size_t frame = 0; frame < bytes / 4 && pos_ < count_; ++frame) {
      const uint16_t s = static_cast<uint16_t>(pcm_[pos_++]);
      out[frame * 4] = out[frame * 4 + 2] = static_cast<uint8_t>(s);
      out[frame * 4 + 1] = out[frame * 4 + 3] = static_cast<uint8_t>(s >> 8);
      if (pos_ == count_) busy_.store(false);
    }
    return bytes;
  }

 private:
  const int16_t* pcm_;
  const size_t count_;
  size_t pos_;
  std::atomic<bool> busy_{false};
  std::atomic<bool> requested_{false};
  std::atomic<bool> cancelled_{false};
};
