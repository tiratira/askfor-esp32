#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "display/oled_display.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "power_save_timer.h"
#include "press_to_talk_mcp_tool.h"
#include "settings.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <esp_efuse_table.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#define TAG "Esp32S3DaxiongBoard"

class Esp32S3DaxiongBoard : public WifiBoard {
private:
  i2c_master_bus_handle_t codec_i2c_bus_;
  Display *display_ = nullptr;
  Button boot_button_;
  PowerSaveTimer *power_save_timer_ = nullptr;
  PressToTalkMcpTool *press_to_talk_tool_ = nullptr;

  void InitializePowerSaveTimer() {
#if CONFIG_USE_ESP_WAKE_WORD
    power_save_timer_ = new PowerSaveTimer(160, 300);
#else
    power_save_timer_ = new PowerSaveTimer(160, 60);
#endif
    power_save_timer_->OnEnterSleepMode(
        [this]() { GetDisplay()->SetPowerSaveMode(true); });
    power_save_timer_->OnExitSleepMode(
        [this]() { GetDisplay()->SetPowerSaveMode(false); });
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

    // Print I2C bus info
    if (i2c_master_probe(codec_i2c_bus_, 0x18, 1000) != ESP_OK) {
      while (true) {
        ESP_LOGE(TAG, "Failed to probe I2C bus, please check if you have "
                      "installed the correct firmware");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    }
  }

  void InitializeButtons() {
    boot_button_.OnClick([this]() {
      auto &app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      if (!press_to_talk_tool_ ||
          !press_to_talk_tool_->IsPressToTalkEnabled()) {
        app.ToggleChatState();
      }
    });
    boot_button_.OnPressDown([this]() {
      if (power_save_timer_) {
        power_save_timer_->WakeUp();
      }
      if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
        Application::GetInstance().StartListening();
      }
    });
    boot_button_.OnPressUp([this]() {
      if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
        Application::GetInstance().StopListening();
      }
    });
  }

  void InitializeTools() {
    press_to_talk_tool_ = new PressToTalkMcpTool();
    press_to_talk_tool_->Initialize();
  }

public:
  Esp32S3DaxiongBoard() : boot_button_(BOOT_BUTTON_GPIO) {
    InitializeCodecI2c();
    display_ = new NoDisplay();
    InitializeButtons();
    InitializePowerSaveTimer();
    InitializeTools();
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual Display *GetDisplay() override { return display_; }

  virtual AudioCodec *GetAudioCodec() override {
    static Es8311AudioCodec audio_codec(
        codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE,
        AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
    return &audio_codec;
  }

  virtual void SetPowerSaveMode(bool enabled) override {
    if (!enabled) {
      power_save_timer_->WakeUp();
    }
    WifiBoard::SetPowerSaveMode(enabled);
  }
};

DECLARE_BOARD(Esp32S3DaxiongBoard);
