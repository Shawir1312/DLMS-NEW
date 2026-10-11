// ============================================================================
// AUDIO DSP SPEAKER PROCESSOR (ESP32-S3) - FIRMWARE v1.0
// Berdasarkan PRD — Audio DSP Speaker Processor (ESP32-S3) oleh @SHAWIR
//
// 2 Input Analog (PCM1808 Stereo ADC) -> Patching Matrix 2x4
// 4 Output Analog (2x PCM5102 Stereo DAC: DAC1 = Out 1/2, DAC2 = Out 3/4)
//
// Arsitektur Dual-Core FreeRTOS:
// - Core 1: audio_task (Prioritas Tertinggi, Real-Time I2S Duplex Block 32)
// - Core 0: ui_task (Prioritas 3, 30 FPS ST7735 Sprite Buffer + EC11 Encoder)
// - Core 0: web_task (Prioritas 1, WiFi SoftAP "AudioDSP-xxxx" + REST/Telemetry)
// ============================================================================

#include <Arduino.h>
#include "hal_pins.h"
#include "dsp_types.h"
#include "i2s_duplex.h"
#include "dsp_engine.h"
#include "storage_manager.h"
#include "rotary_encoder.h"
#include "tft_display.h"
#include "web_server_dsp.h"

// Task handles
static TaskHandle_t s_audioTaskHandle = nullptr;
static TaskHandle_t s_uiTaskHandle    = nullptr;

// ============================================================================
// CORE 1: REAL-TIME AUDIO DSP PROCESSING TASK
// Dilarang: Serial.print, malloc/free dinamis, mutex pemblokir
// ============================================================================
static void audioTask(void* param) {
    // Buffer alokasi DMA di stack audio_task
    static int32_t inPcm[AUDIO_BLOCK_FRAMES * 2];   // 32 frame interleaved L/R dari PCM1808
    static int32_t out1Pcm[AUDIO_BLOCK_FRAMES * 2]; // 32 frame interleaved Out 1 & 2 ke DAC1
    static int32_t out2Pcm[AUDIO_BLOCK_FRAMES * 2]; // 32 frame interleaved Out 3 & 4 ke DAC2

    memset(inPcm, 0, sizeof(inPcm));
    memset(out1Pcm, 0, sizeof(out1Pcm));
    memset(out2Pcm, 0, sizeof(out2Pcm));

    while (1) {
        // 1. Baca 32 sampel stereo dari ADC (PCM1808 via I2S0 Master)
        size_t framesRead = i2sDuplex.readAdc(inPcm, AUDIO_BLOCK_FRAMES);

        if (framesRead == 0) {
            // Fallback jika belum ada sinyal I2S agar tidak spinlock membebani CPU
            memset(inPcm, 0, sizeof(inPcm));
            framesRead = AUDIO_BLOCK_FRAMES;
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        // 2. Jalankan Rantai DSP Lengkap (Float 32-bit):
        dspEngine.processBlock(inPcm, out1Pcm, out2Pcm, framesRead);

        // 3. Tulis Out 1 & 2 ke PCM5102 #1 (I2S1 Master TX)
        i2sDuplex.writeDac1(out1Pcm, framesRead);

        // 4. Tulis Out 3 & 4 ke PCM5102 #2
        i2sDuplex.writeDac2(out2Pcm, framesRead);
    }
}

// ============================================================================
// CORE 0: USER INTERFACE & NAVIGATION TASK (30 FPS)
// ============================================================================
static void uiTask(void* param) {
    const TickType_t framePeriod = pdMS_TO_TICKS(67); // ~15 FPS — cukup responsif, hemat CPU Core 0
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (1) {
        // 1. Update pembacaan rotary encoder dan tombol fisik
        rotaryControl.update();

        int16_t delta   = rotaryControl.getEncoderDelta();
        ButtonEvent swEv = rotaryControl.getEncoderButtonEvent();
        ButtonEvent k0Ev = rotaryControl.getK0ButtonEvent();
        bool k0Held     = rotaryControl.isK0Pressed();

        // 2. Dispatch navigasi layar TFT
        tftDisplay.handleNavigation(delta, swEv, k0Ev, k0Held);

        // 3. Render 1 frame tampilan penuh (160x128 sprite di SRAM)
        tftDisplay.update();

        // 4. Periksa debounce penyimpanan otomatis preset (3 detik)
        storageManager.tickAutoSave();

        vTaskDelayUntil(&lastWakeTime, framePeriod);
    }
}

void printBootBanner() {
    Serial.println("\n");
    Serial.println("================================================================");
    Serial.println("  AUDIO DSP SPEAKER PROCESSOR (ESP32-S3) - DLMS v1.0");
    Serial.println("  Author : @SHAWIR | Core Architecture : Dual-Core LX7");
    Serial.println("  Input Source: ESP32 Bluetooth A2DP / ADC (24/32-bit / 48.0 kHz)");
    Serial.println("  Output: 4-Channel Dual DAC (DAC1: Out 1/2 | DAC2: Out 3/4)");
    Serial.println("================================================================");
    Serial.println();
}

void setup() {
    // 1. Inisialisasi Serial Debug
    Serial.begin(SERIAL_DEBUG_BAUD);
    delay(200);
    printBootBanner();

    // 2. Inisialisasi Layar TFT ST7735 1.8" & Tampilkan Layar Booting Segera
    tftDisplay.begin();
    tftDisplay.drawBootScreen(15, "Inisialisasi sistem...");
    delay(100);

    // 3. Inisialisasi Storage Manager & Muat Preset
    tftDisplay.drawBootScreen(35, "Mounting Flash Storage...");
    storageManager.begin();
    static DspPreset activePreset;
    tftDisplay.drawBootScreen(50, "Memeriksa slot aktif...");
    if (storageManager.loadPreset(storageManager.getCurrentSlot(), activePreset)) {
        Serial.printf("[Storage] Preset aktif dimuat: Slot %u (\"%s\")\n",
                      storageManager.getCurrentSlot(), activePreset.name);
    } else {
        activePreset = StorageManager::getFactoryPreset();
        Serial.println("[Storage] Memakai Factory Default Preset");
    }
    delay(120);

    // 4. Inisialisasi DSP Engine (Core 1) — Persis 66% sesuai referensi UI Menu LCD
    tftDisplay.drawBootScreen(66, "Memuat preset terakhir...");
    dspEngine.init(AUDIO_SAMPLE_RATE);
    dspEngine.setConfig(activePreset);
    delay(200);

    // 5. Inisialisasi Hardware I2S Duplex (Bluetooth Slave Input + Dual DAC)
    tftDisplay.drawBootScreen(82, "Driver I2S Duplex...");
    if (i2sDuplex.begin(AUDIO_SAMPLE_RATE)) {
        Serial.println("[I2S] Bluetooth Slave + Dual DAC Driver: READY");
    } else {
        Serial.println("[I2S] ERROR menginisialisasi I2S Duplex!");
    }
    delay(150);

    // 6. Inisialisasi Rotary Encoder & Tombol K0
    tftDisplay.drawBootScreen(90, "Encoder EC11 & K0...");
    rotaryControl.begin();
    Serial.printf("[Control] Encoder EC11 & K0: READY (CLK=%d, DT=%d, SW=%d, K0=%d)\n",
                  ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_SW_PIN, BUTTON_K0_PIN);
    delay(100);

    // 7. Inisialisasi Web Server & WiFi SoftAP
    tftDisplay.drawBootScreen(96, "Web Server SoftAP...");
    webServerDsp.begin();
    Serial.printf("[Web] Remote Dashboard: http://%s\n", webServerDsp.getIpAddress().c_str());
    delay(150);

    // 8. Booting Selesai 100%
    tftDisplay.drawBootScreen(100, "Sistem siap!");
    delay(400);

    // 9. Buat Task Real-Time Audio di Core 1 (Prioritas 5)
    xTaskCreatePinnedToCore(
        audioTask,
        "audio_task",
        8192,
        NULL,
        5, // Prioritas real-time audio terstandar ESP-IDF
        &s_audioTaskHandle,
        1 // Core 1
    );

    // 10. Buat Task UI di Core 0 (Prioritas 3, Stack 10KB)
    tftDisplay.setScreen(SCREEN_HOME);
    xTaskCreatePinnedToCore(
        uiTask,
        "ui_task",
        10240,
        NULL,
        3,
        &s_uiTaskHandle,
        0 // Core 0
    );

    // 11. Startup Audio (Booting Senyap - Langsung Siap Live Audio)
    dspEngine.setTestSignal(false, 0, 1000.0f, 0.0f, -1);

    Serial.println("\n[System] Inisialisasi selesai. Sistem berjalan normal.\n");
}

void loop() {
    // Loop idle tanpa beban serial monitor agar tidak membebani ESP32-S3
    vTaskDelay(pdMS_TO_TICKS(100));
}
