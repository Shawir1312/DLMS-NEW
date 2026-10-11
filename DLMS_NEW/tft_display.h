#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "dsp_types.h"
#include "hal_pins.h"
#include "rotary_encoder.h"

// ============================================================================
// TFT ST7735 1.8" (160x128 LANDSCAPE) FULL-SPRITE DISPLAY ENGINE
// Desain UI/UX Modern Sesuai PRD Audio DSP & Referensi Layar
// 100% Flicker-Free 25-30 FPS Sprite Buffer di SRAM (40 KB)
// ============================================================================

enum ScreenMode {
    SCREEN_HOME = 0,
    SCREEN_MAIN_MENU,
    SCREEN_MODE_SELECT,
    SCREEN_ROUTING,
    SCREEN_CROSSOVER,
    SCREEN_PEQ,
    SCREEN_LIMITER,
    SCREEN_DELAY,
    SCREEN_POLARITY,
    SCREEN_PRESET,
    SCREEN_SETUP,
    SCREEN_BOOT
};

class TftDisplay {
public:
    TftDisplay();

    void begin();
    void update(); // Dipanggil dari ui_task Core 0 (~30 FPS)

    // Animasi Boot Screen Futuristik (Sesuai Referensi UI Menu LCD Audio DSP)
    void drawBootScreen(uint8_t progress, const char* statusMsg);

    // Navigasi & Input dari Rotary Encoder & K0
    void handleNavigation(int16_t delta, ButtonEvent swEv, ButtonEvent k0Ev, bool k0Held, bool swHeld = false);

    // Kecerahan & Auto-Dim Backlight PWM di GPIO 8
    void setBrightness(uint8_t pct);
    void resetUserActivityTimer();

    // Toast Notification (tampil 1.5 detik)
    void showToast(const char* message, uint16_t duration_ms = 1500);

    ScreenMode getCurrentScreen() const { return _currentScreen; }
    void setScreen(ScreenMode mode);

    bool isScreenLocked() const { return _screenLocked; }
    void toggleScreenLock();

private:
    ScreenMode _currentScreen;
    bool _isInitialized;
    bool _inEditMode;
    bool _screenLocked;

    // Lock/Unlock Button Hold Timing (Home Screen: 3s lock, 4s unlock)
    uint32_t _swHoldStartTime;
    bool _swHoldHandled;
    uint8_t _lockProgressSec;

    // Kecerahan Layar & Timer
    uint8_t _brightnessPct;
    uint32_t _lastActivityTime;
    bool _isDimmed;

    // Toast
    char _toastMsg[24];
    uint32_t _toastEndTime;

    // Status Navigasi Menu
    int8_t _homeSelectedCh;     // 0=In A, 1=In B, 2=Out 1, 3=Out 2, 4=Out 3, 5=Out 4
    int8_t _menuIndex;          // Index item Main Menu (0..8)
    int8_t _subItemIndex;       // Index field di dalam subscreen
    int8_t _modeOptionSel;      // 0 = Mode 1 (Pair), 1 = Mode 2 (Individual)
    int8_t _peqTarget;          // 0=In A, 1=In B, 2=Out 1, 3=Out 2, 4=Out 3, 5=Out 4 (or 2=Out 1-2, 3=Out 3-4 in Mode 1)
    int8_t _peqBand;            // 0..7 (Output) atau 0..3 (Input)
    int8_t _peqEditField;       // 0=Channel, 1=Band, 2=Freq, 3=Gain, 4=Q, 5=Type, 6=Bypass
    int8_t _xoverCh;            // 0=Out 1, 1=Out 2, 2=Out 3, 3=Out 4 (or 0=Out 1-2, 2=Out 3-4 in Mode 1)
    int8_t _xoverEditField;     // 0=Tab, 1=HPF On/Off, 2=HPF Freq, 3=HPF Type, 4=HPF Slope, 5=LPF On/Off, 6=LPF Freq, 7=LPF Type, 8=LPF Slope
    int8_t _routingRow;         // 0..5 (In A, In B, Out 1..4)
    int8_t _routingEditCol;     // 0=Source, 1=Gain, 2=Mute
    int8_t _presetSlotSel;      // 1..10
    bool _presetActionActive;   // true saat dialog modal Load/Simpan aktif
    int8_t _presetActionChoice; // 0=Load, 1=Simpan, 2=Batal
    bool _resetConfirmActive;   // true saat menunggu konfirmasi Factory Reset
    uint32_t _resetConfirmTime; // timestamp batas konfirmasi reset

    // Peak Hold Tracker untuk 6 Kanal VU Meter
    float _peakHoldDb[6];
    uint32_t _peakHoldTime[6];

    // Nilai cadangan saat editing (untuk pembatalan K0)
    float _originalFloatVal;
    int32_t _originalIntVal;

    // Sub-renderers Layar
    void renderHeaderBar(const char* title, const char* rightTag = nullptr, uint16_t titleCol = 0xFFFF, bool showBackArrow = true);
    void renderFooterBar(const char* hint1, const char* hint2, const char* hint3);

    void renderHomeScreen();
    void renderMainMenu();
    void renderModeSelectScreen();
    void renderRoutingScreen();
    void renderCrossoverScreen();
    void renderPeqScreen();
    void renderLimiterScreen();
    void renderDelayScreen();
    void renderPolarityScreen();
    void renderPresetScreen();
    void renderSetupScreen();
    void renderToast();

    // Helper Graphics & Icons
    void drawWifiIcon(int16_t x, int16_t y, uint16_t col);
    void drawArrowBack(int16_t x, int16_t y, uint16_t col);
    void drawMenuIcon(int16_t x, int16_t y, uint8_t idx, uint16_t col);
};

extern TftDisplay tftDisplay;
