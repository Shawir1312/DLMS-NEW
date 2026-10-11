#include "rotary_encoder.h"

RotaryControl rotaryControl;

// State table quadrature 4-step gray code
static const int8_t QUAD_TABLE[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

static volatile uint8_t s_quadState = 0;

void IRAM_ATTR isrEncoderPins() {
    rotaryControl.handleEncoderIsr();
}

RotaryControl::RotaryControl()
    : _rawSteps(0),
      _lastReadSteps(0),
      _lastTurnTime(0),
      _reversed(false),
      _useAccel(true),
      _swState(false),
      _lastSwReading(false),
      _swDebounceTime(0),
      _swPressTime(0),
      _swLongReported(false),
      _swEvent(BTN_NONE),
      _k0State(false),
      _lastK0Reading(false),
      _k0DebounceTime(0),
      _k0PressTime(0),
      _k0LongReported(false),
      _k0Event(BTN_NONE),
      _dualHoldReported(false)
{
}

void RotaryControl::begin() {
    pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
    pinMode(ENCODER_DT_PIN,  INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN,  INPUT_PULLUP);
    pinMode(BUTTON_K0_PIN,   INPUT_PULLUP);

    uint8_t a = digitalRead(ENCODER_CLK_PIN);
    uint8_t b = digitalRead(ENCODER_DT_PIN);
    s_quadState = (a << 1) | b;

    attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), isrEncoderPins, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_DT_PIN),  isrEncoderPins, CHANGE);
}

void IRAM_ATTR RotaryControl::handleEncoderIsr() {
    uint8_t a = digitalRead(ENCODER_CLK_PIN);
    uint8_t b = digitalRead(ENCODER_DT_PIN);
    uint8_t current = (a << 1) | b;
    uint8_t index = ((s_quadState & 0x03) << 2) | current;
    int8_t step = QUAD_TABLE[index & 0x0F];
    s_quadState = current;

    if (step != 0) {
        _rawSteps += step;
    }
}

void RotaryControl::update() {
    uint32_t now = millis();

    // 1. Debounce Tombol Encoder SW (Active LOW)
    bool swRaw = (digitalRead(ENCODER_SW_PIN) == LOW);
    if (swRaw != _lastSwReading) {
        _swDebounceTime = now;
        _lastSwReading = swRaw;
    }
    if ((now - _swDebounceTime) > 30) {
        if (swRaw != _swState) {
            _swState = swRaw;
            if (_swState) {
                // Tombol baru saja ditekan
                _swPressTime = now;
                _swLongReported = false;
            } else {
                // Tombol dilepas
                if (!_swLongReported && !_dualHoldReported) {
                    _swEvent = BTN_CLICK;
                }
            }
        }
    }

    // 2. Debounce Tombol K0 (Active LOW)
    bool k0Raw = (digitalRead(BUTTON_K0_PIN) == LOW);
    if (k0Raw != _lastK0Reading) {
        _k0DebounceTime = now;
        _lastK0Reading = k0Raw;
    }
    if ((now - _k0DebounceTime) > 30) {
        if (k0Raw != _k0State) {
            _k0State = k0Raw;
            if (_k0State) {
                _k0PressTime = now;
                _k0LongReported = false;
            } else {
                if (!_k0LongReported && !_dualHoldReported) {
                    _k0Event = BTN_CLICK;
                }
            }
        }
    }

    // 3. Deteksi Dual Hold 2 Detik (K0 + Encoder SW ditekan bersamaan)
    if (_swState && _k0State) {
        uint32_t dualDuration = now - ((_swPressTime > _k0PressTime) ? _swPressTime : _k0PressTime);
        if (dualDuration >= 2000 && !_dualHoldReported) {
            _dualHoldReported = true;
            _swLongReported = true;
            _k0LongReported = true;
            _swEvent = BTN_DUAL_HOLD_2S;
            _k0Event = BTN_DUAL_HOLD_2S;
        }
    } else {
        _dualHoldReported = false;
    }

    // 4. Deteksi Long Press 1 Detik untuk Encoder SW
    if (_swState && !_swLongReported && !_dualHoldReported) {
        if ((now - _swPressTime) >= 1000) {
            _swLongReported = true;
            _swEvent = BTN_LONG_PRESS_1S;
        }
    }

    // 5. Deteksi Long Press 1 Detik untuk K0
    if (_k0State && !_k0LongReported && !_dualHoldReported) {
        if ((now - _k0PressTime) >= 1000) {
            _k0LongReported = true;
            _k0Event = BTN_LONG_PRESS_1S;
        }
    }
}

int16_t RotaryControl::getEncoderDelta() {
    int32_t currentSteps = _rawSteps;
    int32_t diff = currentSteps - _lastReadSteps;
    int16_t delta = (int16_t)(diff / 4);
    if (delta == 0) return 0;

    _lastReadSteps += delta * 4;

    if (_reversed) delta = -delta;

    return delta;
}

ButtonEvent RotaryControl::getEncoderButtonEvent() {
    ButtonEvent ev = _swEvent;
    _swEvent = BTN_NONE;
    return ev;
}

ButtonEvent RotaryControl::getK0ButtonEvent() {
    ButtonEvent ev = _k0Event;
    _k0Event = BTN_NONE;
    return ev;
}

bool RotaryControl::isEncoderPressed() const {
    return _swState;
}

bool RotaryControl::isK0Pressed() const {
    return _k0State;
}

bool RotaryControl::isDualHoldTriggered() {
    return _dualHoldReported;
}
