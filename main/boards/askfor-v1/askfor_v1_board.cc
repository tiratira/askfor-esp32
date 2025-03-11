#include "application.h"
#include "assets/lang_config.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/oled_display.h"
#include "font_awesome_symbols.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "power_save_timer.h"
#include "settings.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <esp_efuse_table.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#define TAG "AskforV1Board"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class AskforV1Board : public WifiBoard {
private:
  i2c_master_bus_handle_t codec_i2c_bus_;
  Button boot_button_;
  Button volume_down_button_;
  Button volume_up_button_;
  bool press_to_talk_enabled_ = false;
  PowerSaveTimer *power_save_timer_;

  void InitializePowerSaveTimer() {
    power_save_timer_ = new PowerSaveTimer(160, 60);
    power_save_timer_->OnEnterSleepMode([this]() {
      ESP_LOGI(TAG, "Enabling sleep mode");

      auto codec = GetAudioCodec();
      codec->EnableInput(false);
    });
    power_save_timer_->OnExitSleepMode([this]() {
      auto codec = GetAudioCodec();
      codec->EnableInput(true);
    });
    power_save_timer_->SetEnabled(true);
  }

  void InitializeCodecI2c() {
    // Initialize I2C peripheral
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
  }

  void InitializeButtons() {
    boot_button_.OnClick([this]() {
      auto &app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      if (!press_to_talk_enabled_) {
        app.ToggleChatState();
      }
    });
    boot_button_.OnPressDown([this]() {
      power_save_timer_->WakeUp();
      if (press_to_talk_enabled_) {
        Application::GetInstance().StartListening();
      }
    });
    boot_button_.OnPressUp([this]() {
      if (press_to_talk_enabled_) {
        Application::GetInstance().StopListening();
      }
    });

    volume_down_button_.OnClick([&] {
      auto &app = Application::GetInstance();
      auto codec = Board::GetInstance().GetAudioCodec();
      int volume = codec->output_volume();
      if (volume >= 20)
        volume -= 10;
      volume = volume / 10 * 10;
      codec->SetOutputVolume(volume);
      if (app.GetDeviceState() == kDeviceStateIdle)
        app.PlaySound(Lang::Sounds::P3_TICK);
    });

    volume_up_button_.OnClick([&]() {
      auto &app = Application::GetInstance();
      auto codec = Board::GetInstance().GetAudioCodec();
      int volume = codec->output_volume();
      if (volume <= 80)
        volume += 10;
      volume = volume / 10 * 10;
      codec->SetOutputVolume(volume);
      if (app.GetDeviceState() == kDeviceStateIdle)
        app.PlaySound(Lang::Sounds::P3_TICK);
    });
  }

  // 物联网初始化，添加对 AI 可见设备
  void InitializeIot() {
    Settings settings("vendor");
    press_to_talk_enabled_ = settings.GetInt("press_to_talk", 0) != 0;

    auto &thing_manager = iot::ThingManager::GetInstance();
    thing_manager.AddThing(iot::CreateThing("Speaker"));
    thing_manager.AddThing(iot::CreateThing("PressToTalk"));
  }

public:
  AskforV1Board()
      : boot_button_(BOOT_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO) {
    // 把 ESP32C3 的 VDD SPI 引脚作为普通 GPIO 口使用
    esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);

    InitializeCodecI2c();
    InitializeButtons();
    InitializePowerSaveTimer();
    InitializeIot();
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  // virtual Display *GetDisplay() override { return display_; }

  virtual AudioCodec *GetAudioCodec() override {
    static Es8311AudioCodec audio_codec(
        codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE,
        AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
    return &audio_codec;
  }

  void SetPressToTalkEnabled(bool enabled) {
    press_to_talk_enabled_ = enabled;

    Settings settings("vendor", true);
    settings.SetInt("press_to_talk", enabled ? 1 : 0);
    ESP_LOGI(TAG, "Press to talk enabled: %d", enabled);
  }

  bool IsPressToTalkEnabled() { return press_to_talk_enabled_; }
};

DECLARE_BOARD(AskforV1Board);

namespace iot {

class PressToTalk : public Thing {
public:
  PressToTalk()
      : Thing("PressToTalk",
              "控制对话模式，一种是长按对话，一种是单击后连续对话。") {
    // 定义设备的属性
    properties_.AddBooleanProperty(
        "enabled", "true 表示长按说话模式，false 表示单击说话模式",
        []() -> bool {
          auto board = static_cast<AskforV1Board *>(&Board::GetInstance());
          return board->IsPressToTalkEnabled();
        });

    // 定义设备可以被远程执行的指令
    methods_.AddMethod(
        "SetEnabled", "启用或禁用长按说话模式，调用前需要经过用户确认",
        ParameterList({Parameter(
            "enabled", "true 表示长按说话模式，false 表示单击说话模式",
            kValueTypeBoolean, true)}),
        [](const ParameterList &parameters) {
          bool enabled = parameters["enabled"].boolean();
          auto board = static_cast<AskforV1Board *>(&Board::GetInstance());
          board->SetPressToTalkEnabled(enabled);
        });
  }
};

} // namespace iot

DECLARE_THING(PressToTalk);
