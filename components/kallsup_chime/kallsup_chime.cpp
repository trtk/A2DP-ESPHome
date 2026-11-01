#include "kallsup_chime.h"
#include "audio_math.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include <cmath>
#include <cstring>

namespace esphome { namespace kallsup_chime {
static const char *const TAG = "kallsup_chime";
KallsupChime *KallsupChime::instance_ = nullptr;

void KallsupChime::set_address(const std::string &address) {
  explicit_address_ = parse_address(address.c_str(), peer_.address);
  have_peer_.store(explicit_address_);
}
void KallsupChime::set_volume_percent(float percent) {
  if (!std::isfinite(percent)) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  const auto value = static_cast<unsigned>(std::lround(percent));
  volume_.store(value);
  gain_.store(gain_for_percent(value));
}
bool KallsupChime::check_(esp_err_t result, const char *operation) {
  if (result == ESP_OK) return true;
  ESP_LOGE(TAG, "%s failed: %s", operation, esp_err_to_name(result));
  mark_failed();
  return false;
}
void KallsupChime::setup() {
  if (instance_ != nullptr || esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
    ESP_LOGE(TAG, "Bluetooth stack already in use");
    mark_failed();
    return;
  }
  instance_ = this;
  peer_pref_ = global_preferences->make_preference<Peer>(0x4b414c4c);
  if (!explicit_address_) {
    Peer saved{};
    if (peer_pref_.load(&saved)) {
      const uint8_t empty[6]{};
      if (std::memcmp(saved.address, empty, 6) != 0) { peer_ = saved; have_peer_.store(true); }
    }
  }
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  cfg.mode = ESP_BT_MODE_CLASSIC_BT;
  if (!check_(esp_bt_controller_init(&cfg), "controller init")) return;
  if (!check_(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), "controller enable")) return;
  if (!check_(esp_bluedroid_init(), "Bluedroid init")) return;
  if (!check_(esp_bluedroid_enable(), "Bluedroid enable")) return;
  if (!check_(esp_bt_gap_register_callback(gap_callback_), "GAP callback")) return;
  if (!check_(esp_bt_gap_set_device_name("ESPHome KALLSUP"), "device name")) return;
  esp_bt_io_cap_t capability = ESP_BT_IO_CAP_NONE;
  if (!check_(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &capability, sizeof(capability)), "pairing")) return;
  if (!check_(esp_a2d_register_callback(a2dp_callback_), "A2DP callback")) return;
  if (!check_(esp_a2d_source_register_data_callback(audio_callback_), "PCM callback")) return;
  if (!check_(esp_a2d_source_init(), "A2DP init")) return;
  ESP_LOGI(TAG, "Classic Bluetooth starting; continuous PCM at 44.1 kHz");
}
void KallsupChime::copy_peer_(uint8_t *out) {
  portENTER_CRITICAL(&peer_mux_);
  std::memcpy(out, peer_.address, 6);
  portEXIT_CRITICAL(&peer_mux_);
}
void KallsupChime::accept_peer_(const uint8_t *address) {
  portENTER_CRITICAL(&peer_mux_);
  std::memcpy(peer_.address, address, 6);
  portEXIT_CRITICAL(&peer_mux_);
  have_peer_.store(true);
  peer_dirty_.store(true);
}
bool KallsupChime::is_ready() const {
  const uint32_t now = millis();
  return connection_.load() == ESP_A2D_CONNECTION_STATE_CONNECTED && stream_started_.load()
      && have_audio_.load() && now - last_audio_.load() < 1000 && now - stream_since_.load() >= 1000;
}
bool KallsupChime::ring() {
  if (is_failed() || !is_ready()) { ESP_LOGW(TAG, "Speaker not ready; ring ignored"); return false; }
  if (!player_.trigger()) { ESP_LOGD(TAG, "Already ringing"); return false; }
  ESP_LOGI(TAG, "Ding-dong");
  return true;
}
void KallsupChime::loop() {
  const uint32_t now = millis();
  const int connection = connection_.load();
  const bool connected = connection == ESP_A2D_CONNECTION_STATE_CONNECTED;
  const bool ready = is_ready();
  if (!sensors_initialized_ || reported_connection_ != connection) {
    ESP_LOGI(TAG, "Bluetooth connection state: %d", connection);
    if (connected_sensor_ != nullptr) connected_sensor_->publish_state(connected);
    reported_connection_ = connection;
  }
  if (!sensors_initialized_ || reported_ready_ != ready) {
    if (audio_ready_sensor_ != nullptr) audio_ready_sensor_->publish_state(ready);
    reported_ready_ = ready;
  }
  sensors_initialized_ = true;
  if (peer_dirty_.exchange(false)) {
    Peer saved;
    copy_peer_(saved.address);
    peer_pref_.save(&saved);
  }
  if (!profile_ready_.load()) return;
  if (connection == ESP_A2D_CONNECTION_STATE_CONNECTING) {
    if (connect_since_ == 0) connect_since_ = now;
    if (now - connect_since_ >= 20000) {
      uint8_t address[6]; copy_peer_(address);
      esp_a2d_source_disconnect(address);  // Recovery only, never idle power saving.
      connect_since_ = 0;
      connection_.store(ESP_A2D_CONNECTION_STATE_DISCONNECTED);
      next_attempt_ = now + 5000;
    }
    return;
  }
  if (connected) {
    connect_since_ = 0;
    if (start_pending_.exchange(false)) {
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
      next_media_check_ = now + 2000;
    } else if (!stream_started_.load() && static_cast<int32_t>(now - next_media_check_) >= 0) {
      next_media_check_ = now + 2000;
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
    }
    return;  // Never stop/suspend/disconnect a healthy idle connection.
  }
  if (connection != ESP_A2D_CONNECTION_STATE_DISCONNECTED || scanning_.load()) return;
  if (static_cast<int32_t>(now - next_attempt_) < 0) return;
  next_attempt_ = now + 5000;
  if (have_peer_.load()) {
    uint8_t address[6]; copy_peer_(address);
    ESP_LOGI(TAG, "Connecting to %02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3], address[4], address[5]);
    if (esp_a2d_source_connect(address) == ESP_OK) { connect_since_ = now; connection_.store(ESP_A2D_CONNECTION_STATE_CONNECTING); }
  } else {
    ESP_LOGI(TAG, "Searching for %s", speaker_name_.c_str());
    if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 8, 0) == ESP_OK) scanning_.store(true);
  }
}
void KallsupChime::gap_callback_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  auto *self = instance_;
  if (self == nullptr) return;
  if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
    self->scanning_.store(param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
  } else if (event == ESP_BT_GAP_DISC_RES_EVT) {
    if (self->have_peer_.load()) return;
    char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1]{};
    for (int i = 0; i < param->disc_res.num_prop; ++i) {
      auto *property = &param->disc_res.prop[i];
      if (property->type == ESP_BT_GAP_DEV_PROP_EIR) {
        uint8_t length = 0;
        auto *value = esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(property->val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &length);
        if (value == nullptr) value = esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(property->val), ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &length);
        if (value != nullptr) std::memcpy(name, value, length);
      } else if (property->type == ESP_BT_GAP_DEV_PROP_BDNAME) {
        size_t length = property->len < ESP_BT_GAP_MAX_BDNAME_LEN ? property->len : ESP_BT_GAP_MAX_BDNAME_LEN;
        std::memcpy(name, property->val, length);
      }
    }
    if (self->speaker_name_ == name) {
      self->accept_peer_(param->disc_res.bda);
      esp_bt_gap_cancel_discovery();
    }
  } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
    esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
  } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
    esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
    esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
  }
}
void KallsupChime::a2dp_callback_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  auto *self = instance_;
  if (self == nullptr) return;
  if (event == ESP_A2D_PROF_STATE_EVT) {
    self->profile_ready_.store(param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS);
  } else if (event == ESP_A2D_CONNECTION_STATE_EVT) {
    self->connection_.store(param->conn_stat.state);
    if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) self->accept_peer_(param->conn_stat.remote_bda);
    else { self->stream_started_.store(false); self->have_audio_.store(false); self->player_.cancel(); }
  } else if (event == ESP_A2D_AUDIO_STATE_EVT) {
    const bool started = param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED;
    self->stream_started_.store(started);
    if (started) self->stream_since_.store(millis());
    else { self->have_audio_.store(false); self->player_.cancel(); }
  } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
    if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
        param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) self->start_pending_.store(true);
  }
}
int32_t KallsupChime::audio_callback_(uint8_t *data, int32_t length) {
  auto *self = instance_;
  if (self == nullptr || data == nullptr || length <= 0) return 0;
  self->last_audio_.store(millis());
  self->have_audio_.store(true);
  self->player_.render(data, length);
  apply_gain(data, length, self->gain_.load());
  return length;
}
void KallsupChime::dump_config() {
  ESP_LOGCONFIG(TAG, "KALLSUP continuous A2DP chime:");
  ESP_LOGCONFIG(TAG, "  Speaker name: %s", speaker_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Volume: %u%%", volume_.load());
  ESP_LOGCONFIG(TAG, "  PCM: 44100 Hz, 16-bit stereo; idle silence, no disconnect");
}
} }
