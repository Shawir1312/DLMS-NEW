#pragma once
#include <Arduino.h>
#include "hal_pins.h"
#include <esp_idf_version.h>

// ============================================================================
// DUAL I2S DRIVER (MASTER FULL-DUPLEX + SLAVE TX)
// I2S0: Master (MCLK GPIO 6, BCK GPIO 15, LRCK GPIO 16, DIN GPIO 4, DOUT1 GPIO 17)
// I2S1: Slave TX (BCK GPIO 15, LRCK GPIO 16, DOUT2 GPIO 5)
// ============================================================================

class I2sDuplex {
public:
    I2sDuplex();
    ~I2sDuplex();

    bool begin(uint32_t sample_rate = AUDIO_SAMPLE_RATE);
    void end();

    // Baca 1 blok audio dari ADC (2 kanal interleaved int32_t)
    // Mengembalikan jumlah frame (stereo pairs) yang berhasil dibaca
    size_t readAdc(int32_t* rx_buffer, size_t frame_count);

    // Tulis 1 blok audio ke DAC1 (Output 1/2) dan DAC2 (Output 3/4)
    // Tiap buffer berisi stereo pairs interleaved int32_t
    size_t writeDac1(const int32_t* tx_buffer, size_t frame_count);
    size_t writeDac2(const int32_t* tx_buffer, size_t frame_count);

    // Kontrol Soft-Mute Hardware XSMT (High = Unmute, Low = Mute)
    void setHardwareMute(bool mute_dac1, bool mute_dac2);

    // Flush dan sinkronisasi ulang antrean DMA I2S (menghilangkan audio serak / slip)
    void flushDmaBuffers();

    bool isRunning() const { return _running; }
    uint32_t getSampleRate() const { return _sampleRate; }

private:
    uint32_t _sampleRate;
    bool _running;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    void* _txChanDac1;
    void* _rxChanAdc;
    void* _txChanDac2;
#else
    uint8_t _i2sPort0;
    uint8_t _i2sPort1;
#endif
};

extern I2sDuplex i2sDuplex;
