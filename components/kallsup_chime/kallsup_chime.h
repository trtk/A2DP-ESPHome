#pragma once
#include <atomic>
#include <string>
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esp_a2dp_api.h"
#include "esp_gap_bt_api.h"
#include "freertos/FreeRTOS.h"
#include "chime_pcm.h"
#include "chime_player.h"
#include "audio_math.h"

namespace esphome { namespace kallsup_chime {
class KallsupChime : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  void set_speaker_name(const std::string &name) { speaker_name_ = name; }
  void set_address(const std::string &address);
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { connected_sensor_ = sensor; }
  void set_audio_ready_sensor(binary_sensor::BinarySensor *sensor) { audio_ready_sensor_ = sensor; }
  void set_volume_percent(float percent);
  bool ring();
  bool is_ready() const;

 protected:
  struct Peer { uint8_t address[6]; };
  static KallsupChime *instance_;
  static void gap_callback_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
  static void a2dp_callback_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
  static int32_t audio_callback_(uint8_t *data, int32_t length);
  bool check_(esp_err_t result, const char *operation);
  void copy_peer_(uint8_t *out);
  void accept_peer_(const uint8_t *address);
  std::string speaker_name_{"KALLSUP"};
  Peer peer_{};
  portMUX_TYPE peer_mux_ = portMUX_INITIALIZER_UNLOCKED;
  ESPPreferenceObject peer_pref_;
  bool explicit_address_{false};
  std::atomic<bool> have_peer_{false};
  std::atomic<bool> peer_dirty_{false};
  std::atomic<bool> profile_ready_{false};
  std::atomic<bool> scanning_{false};
  std::atomic<bool> stream_started_{false};
  std::atomic<bool> start_pending_{false};
  std::atomic<bool> have_audio_{false};
  std::atomic<int> connection_{ESP_A2D_CONNECTION_STATE_DISCONNECTED};
  std::atomic<uint32_t> stream_since_{0};
  std::atomic<uint32_t> last_audio_{0};
  std::atomic<unsigned> volume_{47};
  std::atomic<unsigned> gain_{gain_for_percent(47)};
  ChimePlayer player_{CHIME_PCM, CHIME_SAMPLES};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  binary_sensor::BinarySensor *audio_ready_sensor_{nullptr};
  int reported_connection_{-1};
  bool reported_ready_{false};
  bool sensors_initialized_{false};
  uint32_t next_attempt_{0};
  uint32_t connect_since_{0};
  uint32_t next_media_check_{0};
};
} }
