#pragma once
#include <Arduino.h>

// ============================================================================
// AUDIO DSP SPEAKER PROCESSOR (ESP32-S3) - HARDWARE PIN MAPPING
// Berdasarkan PRD Bagian 2 (Hardware & Pin Mapping) dan Bagian 3
// Target Hardware: ESP32-S3 (Dual-Core LX7) + PCM1808 ADC + 2x PCM5102 DAC
// ============================================================================

// 1. I2S Audio Bus Pins (Sinkronisasi Clock Bluetooth ESP32 -> ESP32-S3 & Dual DAC)
// Clock I2S dikirim oleh ESP32 Bluetooth (Master) ke header bus PCB ESP32-S3:
#define I2S_BCK_PIN          15  // Bit Clock (BCK) - Pin 26 ESP32 BT -> Pin BCK PCB (GPIO 15)
#define I2S_LRCK_PIN         16  // Word Select (LRCK) - Pin 25 ESP32 BT -> Pin LRCK PCB (GPIO 16)

// I2S Data Pins
#define I2S_DAC1_DATA_PIN    17  // Data Out ke DAC1 (PCM5102 #1: Output 1 & 2) - Solder PCB
#define I2S_DAC2_DATA_PIN     5  // Data Out ke DAC2 (PCM5102 #2: Output 3 & 4) - Solder PCB (TETAP AKTIF!)
#define I2S_ADC_DATA_PIN      4  // Data In Audio dari ESP32 BT - Pin 22 ESP32 BT -> Pin OUT/DATA PCB (GPIO 4)
#define I2S_MCLK_PIN          6  // Master Clock (Bebas / Tidak terpakai dalam mode Bluetooth)

// Soft-Mute Hardware Pins (PCM5102 XSMT: High = Suara Hidup, Low = Mute)
#define DAC1_XSMT_PIN         7  // XSMT PCM5102 #1 (Output 1 & 2) - Solder PCB
#define DAC2_XSMT_PIN        18  // XSMT PCM5102 #2 (Output 3 & 4) - Solder PCB

// Alias Kompatibilitas
#define I2S_ADC_BCK_PIN      I2S_BCK_PIN
#define I2S_ADC_LRCK_PIN     I2S_LRCK_PIN
#define I2S_ADC_MCLK_PIN     I2S_MCLK_PIN
#define I2S_DAC_BCK_PIN      I2S_BCK_PIN
#define I2S_DAC_LRCK_PIN     I2S_LRCK_PIN
#define I2S_TX_BCK_PIN       I2S_BCK_PIN
#define I2S_TX_LRCK_PIN      I2S_LRCK_PIN
#define I2S_TX_DATA_PIN      I2S_DAC1_DATA_PIN
#define I2S_DATA_PIN         I2S_DAC1_DATA_PIN
#define I2S_RX_BCK_PIN       I2S_BCK_PIN
#define I2S_RX_LRCK_PIN      I2S_LRCK_PIN
#define I2S_RX_DATA_PIN      I2S_ADC_DATA_PIN

// 2. Layar TFT ST7735 1.8" 128x160 (SPI)
#define TFT_CS_PIN           10  // CS (Chip Select)
#define TFT_DC_PIN            9  // DC / RS (Data / Command)
#define TFT_RST_PIN          14  // RES (Reset)
#define TFT_MOSI_PIN         11  // SDA / MOSI
#define TFT_SCLK_PIN         12  // SCL / SCK
#define TFT_BL_PIN            8  // BLK / Backlight (PWM dimming)

// 3. Kontrol Fisik: Rotary Encoder EC11 + Tombol K0 (SZYTF 12-Pin Header)
#define ENCODER_CLK_PIN      41  // Encoder Fase A (CLK)
#define ENCODER_DT_PIN       42  // Encoder Fase B (DT)
#define ENCODER_SW_PIN        2  // Tombol Push Encoder (Select / Edit)
#define BUTTON_K0_PIN         1  // Tombol Fisik K0 (Back / Exit)

// 4. Parameter Sistem Audio
#define AUDIO_SAMPLE_RATE     48000   // 48.0 kHz Standard Pro Audio / DLMS
#define AUDIO_BLOCK_FRAMES    32      // 32 sampel per blok proses (~0.66 ms @ 48kHz)
#define AUDIO_MAX_DELAY_MS    100.0f  // 100 ms delay maksimum
#define AUDIO_MAX_DELAY_SAMP  4800    // 4800 sampel delay buffer (~100 ms @ 48kHz)

// 5. Konfigurasi Serial Debug
#define SERIAL_DEBUG_BAUD     115200
