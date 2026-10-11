#pragma once
#include <Arduino.h>
#include "hal_pins.h"

// ============================================================================
// ROTARY ENCODER EC11 & TOMBOL FISIK K0 DRIVER
// Sesuai PRD Bagian 5.2 (Kontrol Fisik)
// Hardware Interrupts Quadrature Decoder + Speed Acceleration + Dual Button Hold
// ============================================================================

enum ButtonEvent {
    BTN_NONE = 0,
    BTN_CLICK,
    BTN_LONG_PRESS_1S,
    BTN_DOUBLE_CLICK,
    BTN_DUAL_HOLD_2S   // K0 + Encoder SW ditekan bersamaan selama 2 detik (Lock Screen)
};

class RotaryControl {
public:
    RotaryControl();

    void begin();
    void update(); // Dipanggil setiap siklus di ui_task Core 0

    // Pembacaan putaran encoder:
    // Mengembalikan delta: +1, -1 (atau +10/-10 saat putar cepat, atau pecahan saat K0 ditahan)
    int16_t getEncoderDelta();

    // Event tombol
    ButtonEvent getEncoderButtonEvent();
    ButtonEvent getK0ButtonEvent();

    // Status real-time
    bool isEncoderPressed() const;
    bool isK0Pressed() const;
    bool isDualHoldTriggered();

    // Arah & akselerasi
    void setReversed(bool rev) { _reversed = rev; }
    void setAcceleration(bool accel) { _useAccel = accel; }

    // Interrupt service routine
    void handleEncoderIsr();

private:
    volatile int32_t _rawSteps;
    int32_t _lastReadSteps;
    uint32_t _lastTurnTime;

    bool _reversed;
    bool _useAccel;

    // Debounce encoder switch (SW)
    bool _swState;
    bool _lastSwReading;
    uint32_t _swDebounceTime;
    uint32_t _swPressTime;
    bool _swLongReported;
    ButtonEvent _swEvent;

    // Debounce K0 button
    bool _k0State;
    bool _lastK0Reading;
    uint32_t _k0DebounceTime;
    uint32_t _k0PressTime;
    bool _k0LongReported;
    ButtonEvent _k0Event;

    // Dual button hold (Lock Screen)
    bool _dualHoldReported;
};

extern RotaryControl rotaryControl;
