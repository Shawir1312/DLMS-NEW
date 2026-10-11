#include "i2s_duplex.h"

I2sDuplex i2sDuplex;

I2sDuplex::I2sDuplex()
    : _sampleRate(AUDIO_SAMPLE_RATE),
      _running(false)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    _txChanDac1 = nullptr;
    _rxChanAdc  = nullptr;
    _txChanDac2 = nullptr;
#else
    _i2sPort0 = 0;
    _i2sPort1 = 1;
#endif
}

I2sDuplex::~I2sDuplex() {
    end();
}

void I2sDuplex::setHardwareMute(bool mute_dac1, bool mute_dac2) {
    // XSMT PCM5102: HIGH = Suara Hidup, LOW = Soft Mute Perangkat Keras
    digitalWrite(DAC1_XSMT_PIN, mute_dac1 ? LOW : HIGH);
    digitalWrite(DAC2_XSMT_PIN, mute_dac2 ? LOW : HIGH);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/i2s_std.h>
#include <soc/rtc.h>

bool I2sDuplex::begin(uint32_t sample_rate) {
    if (_running) end();
    _sampleRate = (sample_rate > 0) ? sample_rate : AUDIO_SAMPLE_RATE;

    // 1. Inisialisasi pin kontrol Soft-Mute Hardware dalam kondisi MUTE
    pinMode(DAC1_XSMT_PIN, OUTPUT);
    pinMode(DAC2_XSMT_PIN, OUTPUT);
    digitalWrite(DAC1_XSMT_PIN, LOW);
    digitalWrite(DAC2_XSMT_PIN, LOW);

    // 2. Konfigurasi I2S1 sebagai SLAVE TX (DAC2 Out 3/4)
    // Sesuai Layout PCB: BCK = GPIO 15, LRCK = GPIO 16, DOUT = GPIO 5
    i2s_chan_config_t chan_cfg1 = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
    chan_cfg1.dma_desc_num = 8;
    chan_cfg1.dma_frame_num = AUDIO_BLOCK_FRAMES * 2;
    chan_cfg1.auto_clear = true;

    i2s_chan_handle_t tx_dac2 = NULL;
    esp_err_t err = i2s_new_channel(&chan_cfg1, &tx_dac2, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Error membuat channel I2S1 Slave: %d\n", err);
        return false;
    }
    _txChanDac2 = (void*)tx_dac2;

    i2s_std_config_t std_cfg1 = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sampleRate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCK_PIN,       // GPIO 15 (Clock input dari BT Pin 26)
            .ws   = (gpio_num_t)I2S_LRCK_PIN,      // GPIO 16 (Clock input dari BT Pin 25)
            .dout = (gpio_num_t)I2S_DAC2_DATA_PIN, // GPIO 5  (Output ke DAC2)
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(tx_dac2, &std_cfg1);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Error init I2S1 std mode: %d\n", err);
        return false;
    }

    // 3. Konfigurasi I2S0 sebagai SLAVE FULL-DUPLEX (DAC1 Out 1/2 TX + BT Audio In A/B RX)
    // Sesuai Layout PCB: BCK = GPIO 15, LRCK = GPIO 16, DAC1 = GPIO 17, ADC = GPIO 4
    i2s_chan_config_t chan_cfg0 = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    chan_cfg0.dma_desc_num = 8;
    chan_cfg0.dma_frame_num = AUDIO_BLOCK_FRAMES * 2;
    chan_cfg0.auto_clear = true;

    i2s_chan_handle_t tx_dac1 = NULL;
    i2s_chan_handle_t rx_adc  = NULL;
    err = i2s_new_channel(&chan_cfg0, &tx_dac1, &rx_adc);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Error membuat channel I2S0 Slave Full-Duplex: %d\n", err);
        return false;
    }
    _txChanDac1 = (void*)tx_dac1;
    _rxChanAdc  = (void*)rx_adc;

    i2s_std_clk_config_t clk_cfg0 = I2S_STD_CLK_DEFAULT_CONFIG(_sampleRate);
    i2s_std_slot_config_t slot_cfg0 = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

    i2s_std_gpio_config_t gpio_cfg_duplex = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = (gpio_num_t)I2S_BCK_PIN,           // GPIO 15 (Clock input dari BT Pin 26)
        .ws   = (gpio_num_t)I2S_LRCK_PIN,          // GPIO 16 (Clock input dari BT Pin 25)
        .dout = (gpio_num_t)I2S_DAC1_DATA_PIN,     // GPIO 17 (Output ke DAC1)
        .din  = (gpio_num_t)I2S_ADC_DATA_PIN,      // GPIO 4  (Audio in dari BT Pin 22)
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    };

    i2s_std_config_t std_cfg_duplex = {
        .clk_cfg  = clk_cfg0,
        .slot_cfg = slot_cfg0,
        .gpio_cfg = gpio_cfg_duplex,
    };

    err = i2s_channel_init_std_mode(tx_dac1, &std_cfg_duplex);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Error init I2S0 TX std mode: %d\n", err);
        return false;
    }

    err = i2s_channel_init_std_mode(rx_adc, &std_cfg_duplex);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Error init I2S0 RX std mode: %d\n", err);
        return false;
    }

    // 4. Urutan start: Enable I2S1 Slave terlebih dahulu, baru I2S0 Slave
    i2s_channel_enable(tx_dac2);
    i2s_channel_enable(rx_adc);
    i2s_channel_enable(tx_dac1);

    // 5. Unmute hardware DAC setelah bus I2S aktif
    delay(20);
    setHardwareMute(false, false);

    _running = true;
    Serial.printf("[I2S] Bluetooth Slave Input + Dual DAC Ready @ %u Hz (16-bit)\n", _sampleRate);
    Serial.printf("      Clock Source (Master BT): BCK=Pin 26 -> GPIO %d | LRCK=Pin 25 -> GPIO %d\n",
                  I2S_BCK_PIN, I2S_LRCK_PIN);
    Serial.printf("      Audio IN (BT Data):       DOUT=Pin 22 -> GPIO %d\n", I2S_ADC_DATA_PIN);
    Serial.printf("      Audio OUT (Dual DAC):     DAC1=GPIO %d (Out 1/2) | DAC2=GPIO %d (Out 3/4)\n",
                  I2S_DAC1_DATA_PIN, I2S_DAC2_DATA_PIN);
    return true;
}

void I2sDuplex::end() {
    if (!_running) return;
    setHardwareMute(true, true);
    delay(10);

    i2s_chan_handle_t tx_dac1 = (i2s_chan_handle_t)_txChanDac1;
    i2s_chan_handle_t rx_adc  = (i2s_chan_handle_t)_rxChanAdc;
    i2s_chan_handle_t tx_dac2 = (i2s_chan_handle_t)_txChanDac2;

    if (tx_dac1) { i2s_channel_disable(tx_dac1); i2s_del_channel(tx_dac1); _txChanDac1 = nullptr; }
    if (rx_adc)  { i2s_channel_disable(rx_adc);  i2s_del_channel(rx_adc);  _rxChanAdc = nullptr; }
    if (tx_dac2) { i2s_channel_disable(tx_dac2); i2s_del_channel(tx_dac2); _txChanDac2 = nullptr; }

    _running = false;
}

size_t I2sDuplex::readAdc(int32_t* rx_buffer, size_t frame_count) {
    if (!_running || !rx_buffer || frame_count == 0) return 0;
    i2s_chan_handle_t rx_adc = (i2s_chan_handle_t)_rxChanAdc;
    int16_t temp16[AUDIO_BLOCK_FRAMES * 2];
    size_t bytes_read = 0;
    size_t bytes_needed = frame_count * 2 * sizeof(int16_t);
    esp_err_t err = i2s_channel_read(rx_adc, temp16, bytes_needed, &bytes_read, pdMS_TO_TICKS(50));
    if (err == ESP_OK && bytes_read > 0) {
        size_t samples_read = bytes_read / sizeof(int16_t);
        for (size_t i = 0; i < samples_read; i++) {
            rx_buffer[i] = ((int32_t)temp16[i]) << 16;
        }
        return samples_read / 2;
    }
    return 0;
}

size_t I2sDuplex::writeDac1(const int32_t* tx_buffer, size_t frame_count) {
    if (!_running || !tx_buffer || frame_count == 0) return 0;
    i2s_chan_handle_t tx_dac1 = (i2s_chan_handle_t)_txChanDac1;
    int16_t temp16[AUDIO_BLOCK_FRAMES * 2];
    for (size_t i = 0; i < frame_count * 2; i++) {
        int32_t val = tx_buffer[i];
        if (val > 32767) val = 32767;
        else if (val < -32768) val = -32768;
        temp16[i] = (int16_t)val;
    }
    size_t bytes_written = 0;
    size_t bytes_needed = frame_count * 2 * sizeof(int16_t);
    esp_err_t err = i2s_channel_write(tx_dac1, temp16, bytes_needed, &bytes_written, pdMS_TO_TICKS(50));
    return (err == ESP_OK) ? (bytes_written / (2 * sizeof(int16_t))) : 0;
}

size_t I2sDuplex::writeDac2(const int32_t* tx_buffer, size_t frame_count) {
    if (!_running || !tx_buffer || frame_count == 0) return 0;
    i2s_chan_handle_t tx_dac2 = (i2s_chan_handle_t)_txChanDac2;
    if (!tx_dac2) return frame_count;
    int16_t temp16[AUDIO_BLOCK_FRAMES * 2];
    for (size_t i = 0; i < frame_count * 2; i++) {
        int32_t val = tx_buffer[i];
        if (val > 32767) val = 32767;
        else if (val < -32768) val = -32768;
        temp16[i] = (int16_t)val;
    }
    size_t bytes_written = 0;
    size_t bytes_needed = frame_count * 2 * sizeof(int16_t);
    esp_err_t err = i2s_channel_write(tx_dac2, temp16, bytes_needed, &bytes_written, pdMS_TO_TICKS(50));
    return (err == ESP_OK) ? (bytes_written / (2 * sizeof(int16_t))) : 0;
}

#else
// Fallback implementasi untuk Arduino Core lama
#include <driver/i2s.h>

bool I2sDuplex::begin(uint32_t sample_rate) {
    _sampleRate = sample_rate;
    pinMode(DAC1_XSMT_PIN, OUTPUT);
    pinMode(DAC2_XSMT_PIN, OUTPUT);
    digitalWrite(DAC1_XSMT_PIN, LOW);
    digitalWrite(DAC2_XSMT_PIN, LOW);

    i2s_config_t i2s_cfg0 = {
        .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = (int)_sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = AUDIO_BLOCK_FRAMES * 2,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_cfg0 = {
        .mck_io_num = -1,
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DAC1_DATA_PIN,
        .data_in_num = I2S_ADC_DATA_PIN
    };

    i2s_driver_install(I2S_NUM_0, &i2s_cfg0, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_cfg0);

    i2s_config_t i2s_cfg1 = {
        .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_TX),
        .sample_rate = (int)_sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = AUDIO_BLOCK_FRAMES * 2,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_cfg1 = {
        .mck_io_num = -1,
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DAC2_DATA_PIN,
        .data_in_num = -1
    };

    i2s_driver_install(I2S_NUM_1, &i2s_cfg1, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &pin_cfg1);

    delay(20);
    setHardwareMute(false, false);
    _running = true;
    return true;
}

void I2sDuplex::end() {
    if (!_running) return;
    setHardwareMute(true, true);
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_1);
    _running = false;
}

size_t I2sDuplex::readAdc(int32_t* rx_buffer, size_t frame_count) {
    if (!_running || !rx_buffer || frame_count == 0) return 0;
    int16_t temp16[AUDIO_BLOCK_FRAMES * 2];
    size_t bytes_read = 0;
    size_t bytes_needed = frame_count * 2 * sizeof(int16_t);
    esp_err_t err = i2s_read(I2S_NUM_0, temp16, bytes_needed, &bytes_read, pdMS_TO_TICKS(50));
    if (err == ESP_OK && bytes_read > 0) {
        size_t samples_read = bytes_read / sizeof(int16_t);
        for (size_t i = 0; i < samples_read; i++) {
            rx_buffer[i] = ((int32_t)temp16[i]) << 16;
        }
        return samples_read / 2;
    }
    return 0;
}

size_t I2sDuplex::writeDac1(const int32_t* tx_buffer, size_t frame_count) {
    if (!_running || !tx_buffer || frame_count == 0) return 0;
    int16_t temp16[AUDIO_BLOCK_FRAMES * 2];
    for (size_t i = 0; i < frame_count * 2; i++) {
        temp16[i] = (int16_t)(tx_buffer[i] >> 16);
    }
    size_t bytes_written = 0;
    size_t bytes_needed = frame_count * 2 * sizeof(int16_t);
    esp_err_t err = i2s_write(I2S_NUM_0, temp16, bytes_needed, &bytes_written, pdMS_TO_TICKS(50));
    return (err == ESP_OK) ? (bytes_written / (2 * sizeof(int16_t))) : 0;
}

size_t I2sDuplex::writeDac2(const int32_t* tx_buffer, size_t frame_count) {
    if (!_running || !tx_buffer || frame_count == 0) return 0;
    int16_t temp16[AUDIO_BLOCK_FRAMES * 2];
    for (size_t i = 0; i < frame_count * 2; i++) {
        temp16[i] = (int16_t)(tx_buffer[i] >> 16);
    }
    size_t bytes_written = 0;
    size_t bytes_needed = frame_count * 2 * sizeof(int16_t);
    esp_err_t err = i2s_write(I2S_NUM_1, temp16, bytes_needed, &bytes_written, pdMS_TO_TICKS(50));
    return (err == ESP_OK) ? (bytes_written / (2 * sizeof(int16_t))) : 0;
}

#endif
