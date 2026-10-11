#include "dsp_engine.h"
#include <esp_timer.h>
#include <math.h>
#include <string.h>

DspEngine dspEngine;

DspEngine::DspEngine()
    : _sampleRate(AUDIO_SAMPLE_RATE),
      _pendingConfigUpdate(false),
      _gainRampStep(0.002f), // ~10 ms ramp pada 48 kHz
      _delayBufferSize(AUDIO_MAX_DELAY_SAMP),
      _testSignalEnabled(false),
      _testSignalType(0),
      _testSignalFreq(1000.0f),
      _testSignalLinear(0.1f),
      _testSignalTarget(-1),
      _sinePhase(0.0f),
      _pinkB0(0.0f), _pinkB1(0.0f), _pinkB2(0.0f),
      _cpuUsageAvg(0.0f),
      _lastRawInA(0),
      _lastRawInB(0)
{
    _paramMux = portMUX_INITIALIZER_UNLOCKED;

    for (int i = 0; i < 4; i++) {
        _delayBuffer[i] = nullptr;
        _delayWriteIdx[i] = 0;
        _delayTargetSamples[i] = 0.0f;

        _outGainLinear[i] = 1.0f;
        _outTargetGainLinear[i] = 1.0f;

        _limiterLookaheadIdx[i] = 0;
        _limiterEnvelope[i] = 0.0f;
        _limiterThresholdLinear[i] = 1.0f;
        _limiterAttackCoeff[i] = 0.9f;
        _limiterReleaseCoeff[i] = 0.999f;
        _limiterGainReductionDb[i] = 0.0f;

        _polarityGain[i] = 1.0f;
        _polarityTarget[i] = 1.0f;

        memset(_limiterLookahead[i], 0, sizeof(_limiterLookahead[i]));
    }

    _inGainLinear[0] = 1.0f;
    _inGainLinear[1] = 1.0f;
    _inTargetGainLinear[0] = 1.0f;
    _inTargetGainLinear[1] = 1.0f;

    memset(&_config, 0, sizeof(_config));
    memset(&_meters, 0, sizeof(_meters));
    for (int i = 0; i < 2; i++) {
        _meters.in_peak_db[i] = -60.0f;
        _meters.in_rms_db[i]  = -60.0f;
    }
    for (int i = 0; i < 4; i++) {
        _meters.out_peak_db[i] = -60.0f;
        _meters.out_rms_db[i]  = -60.0f;
        _meters.limiter_gr_db[i] = 0.0f;
        _limiterMinGr[i] = 1.0f;
    }
    _meterDecimateCounter = 0;
}

DspEngine::~DspEngine() {
    for (int i = 0; i < 4; i++) {
        if (_delayBuffer[i]) {
            free(_delayBuffer[i]);
            _delayBuffer[i] = nullptr;
        }
    }
}

void DspEngine::init(uint32_t sample_rate) {
    _sampleRate = (sample_rate > 0) ? sample_rate : AUDIO_SAMPLE_RATE;
    _gainRampStep = 1.0f / (0.010f * (float)_sampleRate); // 10ms ramp

    // Alokasi memori buffer delay di SRAM internal (~19.2 KB x 4 = ~76.8 KB)
    for (int i = 0; i < 4; i++) {
        if (!_delayBuffer[i]) {
            _delayBuffer[i] = (float*)heap_caps_malloc(_delayBufferSize * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!_delayBuffer[i]) {
                // Fallback ke general heap jika tidak cukup
                _delayBuffer[i] = (float*)malloc(_delayBufferSize * sizeof(float));
            }
        }
        if (_delayBuffer[i]) {
            memset(_delayBuffer[i], 0, _delayBufferSize * sizeof(float));
        }
        _delayWriteIdx[i] = 0;
        _delayTargetSamples[i] = 0.0f;
    }

    // Default Configuration sesuai PRD
    _config.magic = 0x444C4D53;
    _config.version = 1;
    strncpy(_config.name, "Default", sizeof(_config.name));
    _config.master_mute = false;

    // Default Input A & B
    for (int i = 0; i < 2; i++) {
        _config.in[i].gain_db = 0.0f;
        _config.in[i].mute = false;
        for (int b = 0; b < 4; b++) {
            _config.in[i].peq[b].enabled = false;
            _config.in[i].peq[b].type = PEQ_PEAKING;
            _config.in[i].peq[b].freq = (b == 0) ? 100.0f : ((b == 1) ? 500.0f : ((b == 2) ? 2000.0f : 8000.0f));
            _config.in[i].peq[b].gain_db = 0.0f;
            _config.in[i].peq[b].q = 1.0f;
            _config.in[i].peq[b].bypass = false;
        }
    }

    // Default Output 1..4 (OUT 1, OUT 2, OUT 3, OUT 4)
    const char* default_names[4] = {"OUT 1", "OUT 2", "OUT 3", "OUT 4"};
    uint8_t default_sources[4]   = {ROUTING_IN_A, ROUTING_IN_B, ROUTING_IN_A, ROUTING_IN_B};

    for (int i = 0; i < 4; i++) {
        strncpy(_config.out[i].name, default_names[i], sizeof(_config.out[i].name));
        _config.out[i].source = default_sources[i];
        _config.out[i].gain_db = 0.0f;
        _config.out[i].mute = false;
        _config.out[i].polarity_invert = false;

        // Crossover defaults
        _config.out[i].hpf.enabled = false;
        _config.out[i].hpf.freq = 40.0f;
        _config.out[i].hpf.type = FILTER_LINKWITZ_RILEY;
        _config.out[i].hpf.slope = SLOPE_24DB;

        _config.out[i].lpf.enabled = false;
        _config.out[i].lpf.freq = 18000.0f;
        _config.out[i].lpf.type = FILTER_LINKWITZ_RILEY;
        _config.out[i].lpf.slope = SLOPE_24DB;

        // 8-Band Output PEQ
        float default_freqs[8] = {63.0f, 160.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 8000.0f, 12500.0f};
        for (int b = 0; b < 8; b++) {
            _config.out[i].peq[b].enabled = false;
            _config.out[i].peq[b].type = PEQ_PEAKING;
            _config.out[i].peq[b].freq = default_freqs[b];
            _config.out[i].peq[b].gain_db = 0.0f;
            _config.out[i].peq[b].q = 1.0f;
            _config.out[i].peq[b].bypass = false;
        }

        // Delay default
        _config.out[i].delay.delay_ms = 0.0f;
        _config.out[i].delay.unit = DELAY_UNIT_MS;
        _config.out[i].delay.temp_c = 20.0f;

        // Limiter default: On, 0 dBFS, 1ms att, 200ms rel
        _config.out[i].limiter.enabled = true;
        _config.out[i].limiter.threshold_db = 0.0f;
        _config.out[i].limiter.attack_ms = 1.0f;
        _config.out[i].limiter.release_ms = 200.0f;
    }

    // Default System Setup
    _config.setup.brightness_pct = 90;
    _config.setup.auto_dim_sec = 60;
    _config.setup.theme = THEME_DARK;
    _config.setup.orientation = 1;
    _config.setup.encoder_reversed = false;
    _config.setup.encoder_accel = true;
    _config.setup.hold_time_ms = 1000;
    _config.setup.auto_lock_screen = false;
    _config.setup.mute_on_boot = false;
    _config.setup.wifi_mode = DSP_WIFI_AP;
    _config.setup.web_access = WEB_ACCESS_FULL;
    strncpy(_config.setup.wifi_ssid, "", sizeof(_config.setup.wifi_ssid));
    strncpy(_config.setup.wifi_pass, "", sizeof(_config.setup.wifi_pass));
    strncpy(_config.setup.web_password, "admin123", sizeof(_config.setup.web_password));

    updateInternalFilters();
    for (int i = 0; i < 4; i++) {
        updateLimiterCoeffs(i);
        updateDelaySamples(i);
    }
}

void DspEngine::setConfig(const DspPreset& config) {
    portENTER_CRITICAL(&_paramMux);
    _config = config;
    for (int i = 0; i < 2; i++) {
        _inTargetGainLinear[i] = _config.in[i].mute ? 0.0f : powf(10.0f, _config.in[i].gain_db / 20.0f);
    }
    for (int i = 0; i < 4; i++) {
        _outTargetGainLinear[i] = (_config.out[i].mute || _config.master_mute) ? 0.0f : powf(10.0f, _config.out[i].gain_db / 20.0f);
        _polarityTarget[i] = _config.out[i].polarity_invert ? -1.0f : 1.0f;
    }
    _pendingConfigUpdate = true;
    portEXIT_CRITICAL(&_paramMux);
}

const DspPreset& DspEngine::getConfig() const {
    return _config;
}

void DspEngine::setInputGain(uint8_t in_ch, float gain_db) {
    if (in_ch >= 2) return;
    if (gain_db < -60.0f) gain_db = -60.0f;
    if (gain_db > 12.0f) gain_db = 12.0f;

    portENTER_CRITICAL(&_paramMux);
    _config.in[in_ch].gain_db = gain_db;
    _inTargetGainLinear[in_ch] = _config.in[in_ch].mute ? 0.0f : powf(10.0f, gain_db / 20.0f);
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setInputMute(uint8_t in_ch, bool mute) {
    if (in_ch >= 2) return;
    portENTER_CRITICAL(&_paramMux);
    _config.in[in_ch].mute = mute;
    _inTargetGainLinear[in_ch] = mute ? 0.0f : powf(10.0f, _config.in[in_ch].gain_db / 20.0f);
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setInputPeq(uint8_t in_ch, uint8_t band, bool enabled, uint8_t type, float freq, float gain_db, float q, bool bypass) {
    if (in_ch >= 2 || band >= 4) return;
    if (isnan(freq) || freq < 20.0f) freq = 20.0f;
    if (freq > (float)_sampleRate * 0.45f) freq = (float)_sampleRate * 0.45f;
    if (isnan(gain_db)) gain_db = 0.0f;
    if (gain_db < -15.0f) gain_db = -15.0f;
    if (gain_db > 15.0f) gain_db = 15.0f;
    if (isnan(q) || q < 0.1f) q = 0.1f;
    if (q > 20.0f) q = 20.0f;

    portENTER_CRITICAL(&_paramMux);
    _config.in[in_ch].peq[band].enabled = enabled;
    _config.in[in_ch].peq[band].type = type;
    _config.in[in_ch].peq[band].freq = freq;
    _config.in[in_ch].peq[band].gain_db = gain_db;
    _config.in[in_ch].peq[band].q = q;
    _config.in[in_ch].peq[band].bypass = bypass;
    portEXIT_CRITICAL(&_paramMux);

    updateSingleInputPeq(in_ch, band);
}

static inline uint8_t getPairChannel(uint8_t ch) {
    return (ch == 0) ? 1 : ((ch == 1) ? 0 : ((ch == 2) ? 3 : 2));
}

void DspEngine::setOutputSource(uint8_t out_ch, uint8_t source) {
    if (out_ch >= 4 || source > 3) return;
    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].source = source;
    // Routing source dibiarkan independen agar Out 1 bisa In A (L) dan Out 2 bisa In B (R)
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setOutputGain(uint8_t out_ch, float gain_db) {
    if (out_ch >= 4) return;
    if (isnan(gain_db)) gain_db = 0.0f;
    if (gain_db < -60.0f) gain_db = -60.0f;
    if (gain_db > 12.0f) gain_db = 12.0f;

    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].gain_db = gain_db;
    _outTargetGainLinear[out_ch] = (_config.out[out_ch].mute || _config.master_mute) ? 0.0f : powf(10.0f, gain_db / 20.0f);
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].gain_db = gain_db;
        _outTargetGainLinear[p] = (_config.out[p].mute || _config.master_mute) ? 0.0f : powf(10.0f, gain_db / 20.0f);
    }
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setOutputMute(uint8_t out_ch, bool mute) {
    if (out_ch >= 4) return;
    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].mute = mute;
    _outTargetGainLinear[out_ch] = (mute || _config.master_mute) ? 0.0f : powf(10.0f, _config.out[out_ch].gain_db / 20.0f);
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].mute = mute;
        _outTargetGainLinear[p] = (mute || _config.master_mute) ? 0.0f : powf(10.0f, _config.out[p].gain_db / 20.0f);
    }
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setOutputPolarity(uint8_t out_ch, bool invert) {
    if (out_ch >= 4) return;
    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].polarity_invert = invert;
    _polarityTarget[out_ch] = invert ? -1.0f : 1.0f;
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].polarity_invert = invert;
        _polarityTarget[p] = invert ? -1.0f : 1.0f;
    }
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setOutputHpf(uint8_t out_ch, bool enabled, float freq, uint8_t type, uint8_t slope) {
    if (out_ch >= 4) return;
    if (isnan(freq) || freq < 20.0f) freq = 20.0f;
    if (freq > (float)_sampleRate * 0.45f) freq = (float)_sampleRate * 0.45f;

    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].hpf.enabled = enabled;
    _config.out[out_ch].hpf.freq = freq;
    _config.out[out_ch].hpf.type = type;
    _config.out[out_ch].hpf.slope = slope;
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].hpf.enabled = enabled;
        _config.out[p].hpf.freq = freq;
        _config.out[p].hpf.type = type;
        _config.out[p].hpf.slope = slope;
    }
    portEXIT_CRITICAL(&_paramMux);

    updateSingleOutputHpf(out_ch);
    if (_config.setup.out_mode == 0) {
        updateSingleOutputHpf(getPairChannel(out_ch));
    }
}

void DspEngine::setOutputLpf(uint8_t out_ch, bool enabled, float freq, uint8_t type, uint8_t slope) {
    if (out_ch >= 4) return;
    if (isnan(freq) || freq < 20.0f) freq = 20.0f;
    if (freq > (float)_sampleRate * 0.45f) freq = (float)_sampleRate * 0.45f;

    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].lpf.enabled = enabled;
    _config.out[out_ch].lpf.freq = freq;
    _config.out[out_ch].lpf.type = type;
    _config.out[out_ch].lpf.slope = slope;
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].lpf.enabled = enabled;
        _config.out[p].lpf.freq = freq;
        _config.out[p].lpf.type = type;
        _config.out[p].lpf.slope = slope;
    }
    portEXIT_CRITICAL(&_paramMux);

    updateSingleOutputLpf(out_ch);
    if (_config.setup.out_mode == 0) {
        updateSingleOutputLpf(getPairChannel(out_ch));
    }
}

void DspEngine::setOutputPeq(uint8_t out_ch, uint8_t band, bool enabled, uint8_t type, float freq, float gain_db, float q, bool bypass) {
    if (out_ch >= 4 || band >= 8) return;
    if (isnan(freq) || freq < 20.0f) freq = 20.0f;
    if (freq > (float)_sampleRate * 0.45f) freq = (float)_sampleRate * 0.45f;
    if (isnan(gain_db)) gain_db = 0.0f;
    if (gain_db < -15.0f) gain_db = -15.0f;
    if (gain_db > 15.0f) gain_db = 15.0f;
    if (isnan(q) || q < 0.1f) q = 0.1f;
    if (q > 20.0f) q = 20.0f;

    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].peq[band].enabled = enabled;
    _config.out[out_ch].peq[band].type = type;
    _config.out[out_ch].peq[band].freq = freq;
    _config.out[out_ch].peq[band].gain_db = gain_db;
    _config.out[out_ch].peq[band].q = q;
    _config.out[out_ch].peq[band].bypass = bypass;
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].peq[band].enabled = enabled;
        _config.out[p].peq[band].type = type;
        _config.out[p].peq[band].freq = freq;
        _config.out[p].peq[band].gain_db = gain_db;
        _config.out[p].peq[band].q = q;
        _config.out[p].peq[band].bypass = bypass;
    }
    portEXIT_CRITICAL(&_paramMux);

    updateSingleOutputPeq(out_ch, band);
    if (_config.setup.out_mode == 0) {
        updateSingleOutputPeq(getPairChannel(out_ch), band);
    }
}

void DspEngine::setOutputDelay(uint8_t out_ch, float delay_ms, uint8_t unit, float temp_c) {
    if (out_ch >= 4) return;
    if (isnan(delay_ms) || delay_ms < 0.0f) delay_ms = 0.0f;
    if (delay_ms > AUDIO_MAX_DELAY_MS) delay_ms = AUDIO_MAX_DELAY_MS;

    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].delay.delay_ms = delay_ms;
    _config.out[out_ch].delay.unit = unit;
    _config.out[out_ch].delay.temp_c = temp_c;
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].delay.delay_ms = delay_ms;
        _config.out[p].delay.unit = unit;
        _config.out[p].delay.temp_c = temp_c;
    }
    portEXIT_CRITICAL(&_paramMux);

    updateDelaySamples(out_ch);
    if (_config.setup.out_mode == 0) {
        updateDelaySamples(getPairChannel(out_ch));
    }
}

void DspEngine::setOutputLimiter(uint8_t out_ch, bool enabled, float threshold_db, float attack_ms, float release_ms) {
    if (out_ch >= 4) return;
    if (isnan(threshold_db)) threshold_db = 0.0f;
    if (threshold_db > 0.0f) threshold_db = 0.0f;
    if (threshold_db < -40.0f) threshold_db = -40.0f;
    if (isnan(attack_ms) || attack_ms < 0.1f) attack_ms = 0.1f;
    if (attack_ms > 100.0f) attack_ms = 100.0f;
    if (isnan(release_ms) || release_ms < 10.0f) release_ms = 10.0f;
    if (release_ms > 2000.0f) release_ms = 2000.0f;

    portENTER_CRITICAL(&_paramMux);
    _config.out[out_ch].limiter.enabled = enabled;
    _config.out[out_ch].limiter.threshold_db = threshold_db;
    _config.out[out_ch].limiter.attack_ms = attack_ms;
    _config.out[out_ch].limiter.release_ms = release_ms;
    if (_config.setup.out_mode == 0) {
        uint8_t p = getPairChannel(out_ch);
        _config.out[p].limiter.enabled = enabled;
        _config.out[p].limiter.threshold_db = threshold_db;
        _config.out[p].limiter.attack_ms = attack_ms;
        _config.out[p].limiter.release_ms = release_ms;
    }
    portEXIT_CRITICAL(&_paramMux);

    updateLimiterCoeffs(out_ch);
    if (_config.setup.out_mode == 0) {
        updateLimiterCoeffs(getPairChannel(out_ch));
    }
}

void DspEngine::setMasterMute(bool mute) {
    portENTER_CRITICAL(&_paramMux);
    _config.master_mute = mute;
    for (int i = 0; i < 4; i++) {
        _outTargetGainLinear[i] = (mute || _config.out[i].mute) ? 0.0f : powf(10.0f, _config.out[i].gain_db / 20.0f);
    }
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::setTestSignal(bool enabled, uint8_t type, float freq, float level_db, int8_t target_ch) {
    portENTER_CRITICAL(&_paramMux);
    _testSignalEnabled = enabled;
    _testSignalType = type;
    _testSignalFreq = freq;
    _testSignalLinear = powf(10.0f, level_db / 20.0f);
    _testSignalTarget = target_ch;
    portEXIT_CRITICAL(&_paramMux);
}

void DspEngine::updateSingleInputPeq(uint8_t in_ch, uint8_t band) {
    if (in_ch >= 2 || band >= 4) return;
    float fs = (float)_sampleRate;
    const PeqBandConfig& p = _config.in[in_ch].peq[band];
    if (!p.enabled || p.bypass) {
        _inPeq[in_ch][band].setPassthrough();
    } else {
        switch (p.type) {
            case PEQ_LOW_SHELF:
                _inPeq[in_ch][band].setLowShelf(p.freq, fs, p.gain_db, p.q);
                break;
            case PEQ_HIGH_SHELF:
                _inPeq[in_ch][band].setHighShelf(p.freq, fs, p.gain_db, p.q);
                break;
            case PEQ_NOTCH:
                _inPeq[in_ch][band].setNotch(p.freq, fs, p.q);
                break;
            case PEQ_PEAKING:
            default:
                _inPeq[in_ch][band].setPeaking(p.freq, fs, p.gain_db, p.q);
                break;
        }
    }
}

void DspEngine::updateSingleOutputPeq(uint8_t out_ch, uint8_t band) {
    if (out_ch >= 4 || band >= 8) return;
    float fs = (float)_sampleRate;
    const PeqBandConfig& p = _config.out[out_ch].peq[band];
    if (!p.enabled || p.bypass) {
        _outPeq[out_ch][band].setPassthrough();
    } else {
        switch (p.type) {
            case PEQ_LOW_SHELF:
                _outPeq[out_ch][band].setLowShelf(p.freq, fs, p.gain_db, p.q);
                break;
            case PEQ_HIGH_SHELF:
                _outPeq[out_ch][band].setHighShelf(p.freq, fs, p.gain_db, p.q);
                break;
            case PEQ_NOTCH:
                _outPeq[out_ch][band].setNotch(p.freq, fs, p.q);
                break;
            case PEQ_PEAKING:
            default:
                _outPeq[out_ch][band].setPeaking(p.freq, fs, p.gain_db, p.q);
                break;
        }
    }
}

void DspEngine::updateSingleOutputHpf(uint8_t out_ch) {
    if (out_ch >= 4) return;
    float fs = (float)_sampleRate;
    const XoverFilterConfig& hpf = _config.out[out_ch].hpf;
    _outHpf[out_ch].configure(true, hpf.enabled, hpf.freq, fs, hpf.type, hpf.slope);
}

void DspEngine::updateSingleOutputLpf(uint8_t out_ch) {
    if (out_ch >= 4) return;
    float fs = (float)_sampleRate;
    const XoverFilterConfig& lpf = _config.out[out_ch].lpf;
    _outLpf[out_ch].configure(false, lpf.enabled, lpf.freq, fs, lpf.type, lpf.slope);
}

void DspEngine::updateInternalFilters() {
    for (int in_ch = 0; in_ch < 2; in_ch++) {
        for (int b = 0; b < 4; b++) {
            updateSingleInputPeq(in_ch, b);
        }
    }
    for (int out_ch = 0; out_ch < 4; out_ch++) {
        updateSingleOutputHpf(out_ch);
        updateSingleOutputLpf(out_ch);
        for (int b = 0; b < 8; b++) {
            updateSingleOutputPeq(out_ch, b);
        }
    }
}

void DspEngine::updateLimiterCoeffs(uint8_t ch) {
    if (ch >= 4) return;
    float fs = (float)_sampleRate;
    const LimiterConfig& lim = _config.out[ch].limiter;

    float th_db = (isnan(lim.threshold_db) || isinf(lim.threshold_db)) ? 0.0f : lim.threshold_db;
    if (th_db > 0.0f) th_db = 0.0f;
    if (th_db < -40.0f) th_db = -40.0f;
    _limiterThresholdLinear[ch] = powf(10.0f, th_db / 20.0f);

    float att_ms = (isnan(lim.attack_ms) || lim.attack_ms < 0.05f) ? 0.05f : lim.attack_ms;
    float rel_ms = (isnan(lim.release_ms) || lim.release_ms < 1.0f) ? 1.0f : lim.release_ms;
    float att_sec = att_ms * 0.001f;
    float rel_sec = rel_ms * 0.001f;

    _limiterAttackCoeff[ch]  = expf(-1.0f / (att_sec * fs));
    _limiterReleaseCoeff[ch] = expf(-1.0f / (rel_sec * fs));
}

void DspEngine::updateDelaySamples(uint8_t ch) {
    if (ch >= 4) return;
    float d_ms = (isnan(_config.out[ch].delay.delay_ms) || isinf(_config.out[ch].delay.delay_ms)) ? 0.0f : _config.out[ch].delay.delay_ms;
    if (d_ms < 0.0f) d_ms = 0.0f;
    float samples = (d_ms * 0.001f) * (float)_sampleRate;
    if (samples < 0.0f || isnan(samples)) samples = 0.0f;
    if (samples > (float)(_delayBufferSize - 4)) samples = (float)(_delayBufferSize - 4);
    _delayTargetSamples[ch] = samples;
}

void DspEngine::processBlock(const int32_t* in_pcm, int32_t* out1_pcm, int32_t* out2_pcm, size_t frame_count) {
    int64_t start_time = esp_timer_get_time();

    if (_pendingConfigUpdate) {
        portENTER_CRITICAL(&_paramMux);
        _pendingConfigUpdate = false;
        portEXIT_CRITICAL(&_paramMux);
        updateInternalFilters();
        for (int i = 0; i < 4; i++) {
            updateLimiterCoeffs(i);
            updateDelaySamples(i);
            if (_delayBuffer[i]) {
                memset(_delayBuffer[i], 0, _delayBufferSize * sizeof(float));
            }
            _delayWriteIdx[i] = 0;
            memset(_limiterLookahead[i], 0, sizeof(_limiterLookahead[i]));
            _limiterLookaheadIdx[i] = 0;
            _limiterEnvelope[i] = 0.0f;
        }
    }

    // Telemetri akumulator per blok
    float in_peak[2] = {0.0f, 0.0f};
    float in_sq_sum[2] = {0.0f, 0.0f};
    float out_peak[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float out_sq_sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (frame_count > 0 && in_pcm) {
        _lastRawInA = in_pcm[0];
        _lastRawInB = in_pcm[1];
    }

    // Konversi scale int32_t I2S ke float [-1.0 .. +1.0]
    const float INT32_TO_FLOAT = 1.0f / 2147483648.0f;
    const float FLOAT_TO_INT32 = 2147418112.0f; // 32767.0f * 65536.0f (exact representation in float, zero overflow)

    for (size_t n = 0; n < frame_count; n++) {
        // 1. Baca Input A dan Input B dari frame interleaved
        float s_inA = (float)in_pcm[n * 2 + 0] * INT32_TO_FLOAT;
        float s_inB = (float)in_pcm[n * 2 + 1] * INT32_TO_FLOAT;

        // Ramp smooth input gains
        _inGainLinear[0] += (_inTargetGainLinear[0] - _inGainLinear[0]) * _gainRampStep;
        _inGainLinear[1] += (_inTargetGainLinear[1] - _inGainLinear[1]) * _gainRampStep;

        s_inA *= _inGainLinear[0];
        s_inB *= _inGainLinear[1];

        // 2. Input PEQ (4 band per input) — only run active, non-passthrough bands
        for (int b = 0; b < 4; b++) {
            if (!_inPeq[0][b].isPassthrough()) s_inA = _inPeq[0][b].process(s_inA);
            if (!_inPeq[1][b].isPassthrough()) s_inB = _inPeq[1][b].process(s_inB);
        }

        // Akumulasi input meter
        float abs_inA = fabsf(s_inA);
        float abs_inB = fabsf(s_inB);
        if (abs_inA > in_peak[0]) in_peak[0] = abs_inA;
        if (abs_inB > in_peak[1]) in_peak[1] = abs_inB;
        in_sq_sum[0] += s_inA * s_inA;
        in_sq_sum[1] += s_inB * s_inB;

        // 3. Test Tone Generator (bila aktif)
        float test_val = 0.0f;
        if (_testSignalEnabled) {
            if (_testSignalType == 0) {
                // Sine wave
                test_val = sinf(_sinePhase) * _testSignalLinear;
                _sinePhase += 2.0f * (float)M_PI * _testSignalFreq / (float)_sampleRate;
                if (_sinePhase >= 2.0f * (float)M_PI) _sinePhase -= 2.0f * (float)M_PI;
            } else {
                // Pink noise (Kellet filter)
                float white = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f);
                _pinkB0 = 0.99886f * _pinkB0 + white * 0.0555179f;
                _pinkB1 = 0.99332f * _pinkB1 + white * 0.0750759f;
                _pinkB2 = 0.96900f * _pinkB2 + white * 0.1538520f;
                test_val = (_pinkB0 + _pinkB1 + _pinkB2 + white * 0.5362f) * 0.11f * _testSignalLinear;
            }
        }

        // 4. Proses 4 Kanal Output
        float out_samples[4];

        for (int ch = 0; ch < 4; ch++) {
            float s = 0.0f;

            // Jika generator aktif untuk channel ini, injeksikan
            if (_testSignalEnabled && (_testSignalTarget == -1 || _testSignalTarget == ch)) {
                s = test_val;
            } else {
                // Routing matrix 2x4 — ROUTING_OFF = channel tidak aktif
                switch (_config.out[ch].source) {
                    case ROUTING_IN_A:   s = s_inA; break;
                    case ROUTING_IN_B:   s = s_inB; break;
                    case ROUTING_SUM_AB: s = (s_inA + s_inB) * 0.5f; break;
                    case ROUTING_OFF:
                    default:
                        // Channel OFF — skip semua DSP, output 0
                        out_samples[ch] = 0.0f;
                        out_peak[ch] = 0.0f;
                        continue;
                }
            }

            // Crossover HPF — hanya jika ada filter aktif
            if (_outHpf[ch].activeCount > 0) s = _outHpf[ch].process(s);

            // Crossover LPF — hanya jika ada filter aktif
            if (_outLpf[ch].activeCount > 0) s = _outLpf[ch].process(s);

            // Output PEQ (8 band) — skip band yang passthrough
            for (int b = 0; b < 8; b++) {
                if (!_outPeq[ch][b].isPassthrough()) {
                    s = _outPeq[ch][b].process(s);
                }
            }

            // Polarity inversion — skip crossfade jika sudah settled
            float ptgt = _polarityTarget[ch];
            float pgain = _polarityGain[ch];
            if (fabsf(pgain - ptgt) > 0.001f) {
                pgain += (ptgt - pgain) * 0.05f;
                _polarityGain[ch] = pgain;
            } else {
                pgain = ptgt;
                _polarityGain[ch] = ptgt;
            }
            s *= pgain;

            // Delay Line — fast-path jika delay == 0
            if (_delayBuffer[ch] && _delayTargetSamples[ch] > 0.5f) {
                uint32_t widx = _delayWriteIdx[ch];
                _delayBuffer[ch][widx] = s;

                float d_samp = _delayTargetSamples[ch];
                if (isnan(d_samp) || isinf(d_samp) || d_samp < 0.0f) d_samp = 0.0f;
                if (d_samp > (float)(_delayBufferSize - 4)) d_samp = (float)(_delayBufferSize - 4);

                float rpos = (float)widx - d_samp;
                while (rpos < 0.0f) rpos += (float)_delayBufferSize;
                while (rpos >= (float)_delayBufferSize) rpos -= (float)_delayBufferSize;
                if (isnan(rpos) || isinf(rpos)) rpos = 0.0f;

                uint32_t r_idx0 = ((uint32_t)rpos) % _delayBufferSize;
                uint32_t r_idx1 = (r_idx0 + 1) % _delayBufferSize;
                float frac = rpos - (float)r_idx0;
                if (frac < 0.0f) frac = 0.0f;
                if (frac > 1.0f) frac = 1.0f;

                s = _delayBuffer[ch][r_idx0] * (1.0f - frac) + _delayBuffer[ch][r_idx1] * frac;
                _delayWriteIdx[ch] = (widx + 1) % _delayBufferSize;
            }

            // Output Gain dengan Ramp Smoothing — skip jika sudah converged
            float ogtgt = _outTargetGainLinear[ch];
            float ogain = _outGainLinear[ch];
            if (fabsf(ogain - ogtgt) > 0.0001f) {
                ogain += (ogtgt - ogain) * _gainRampStep;
                _outGainLinear[ch] = ogain;
            } else {
                ogain = ogtgt;
                _outGainLinear[ch] = ogtgt;
            }
            s *= ogain;

            // Limiter (Peak Envelope + Lookahead + Pure Exponential Decay Release)
            if (_config.out[ch].limiter.enabled) {
                // Masukkan sampel ke lookahead buffer
                uint8_t l_idx = _limiterLookaheadIdx[ch];
                float delayed_s = _limiterLookahead[ch][l_idx];
                _limiterLookahead[ch][l_idx] = s;
                _limiterLookaheadIdx[ch] = (l_idx + 1) % LIMITER_LOOKAHEAD;

                // Deteksi envelope puncak (lookahead brickwall instant attack)
                float peak_val = fabsf(s);
                if (peak_val > _limiterEnvelope[ch]) {
                    _limiterEnvelope[ch] = peak_val;
                } else {
                    // Release murni eksponensial (tidak memodulasi gelombang bass 80Hz)
                    _limiterEnvelope[ch] *= _limiterReleaseCoeff[ch];
                }

                // Hitung gain reduction
                float gr = 1.0f;
                if (_limiterEnvelope[ch] > _limiterThresholdLinear[ch] && _limiterEnvelope[ch] > 1e-5f) {
                    gr = _limiterThresholdLinear[ch] / _limiterEnvelope[ch];
                }

                // Terapkan ke delayed sample
                s = delayed_s * gr;

                // Track minimum gain reduction linear — ZERO log10f di dalam sample loop!
                if (gr < _limiterMinGr[ch]) {
                    _limiterMinGr[ch] = gr;
                }
            }

            // Smooth Musical Soft-Knee Saturation [-0.999 .. +0.999]
            // Mencegah distorsi serak / pecah pada sinyal bass besar
            if (s > 0.85f) {
                float d = s - 0.85f;
                s = 0.85f + (d / (1.0f + d * 3.0f)) * 0.149f;
            } else if (s < -0.85f) {
                float d = s + 0.85f;
                s = -0.85f + (d / (1.0f - d * 3.0f)) * 0.149f;
            }

            if (s > 0.999f) s = 0.999f;
            else if (s < -0.999f) s = -0.999f;

            out_samples[ch] = s;

            // Akumulasi output meter
            float abs_s = fabsf(s);
            if (abs_s > out_peak[ch]) out_peak[ch] = abs_s;
            out_sq_sum[ch] += s * s;
        }

        // Tulis ke buffer I2S DAC1 (Out 1 & Out 2) - 16-bit PCM langsung aman tanpa overflow
        out1_pcm[n * 2 + 0] = (int32_t)(out_samples[0] * 32767.0f);
        out1_pcm[n * 2 + 1] = (int32_t)(out_samples[1] * 32767.0f);

        // Tulis ke buffer I2S DAC2 (Out 3 & Out 4)
        out2_pcm[n * 2 + 0] = (int32_t)(out_samples[2] * 32767.0f);
        out2_pcm[n * 2 + 1] = (int32_t)(out_samples[3] * 32767.0f);
    }

    // 5. Update Telemetri VU Meter & Limiter GR (Decimated: setiap 32 blok = ~21 ms / 47 Hz)
    _meterDecimateCounter++;
    if (_meterDecimateCounter >= 32) {
        _meterDecimateCounter = 0;
        float inv_frames = 1.0f / (float)frame_count;

        for (int i = 0; i < 2; i++) {
            float peak_db = (in_peak[i] > 1e-4f) ? (20.0f * log10f(in_peak[i])) : -60.0f;
            float rms_db  = (in_sq_sum[i] > 1e-8f) ? (10.0f * log10f(in_sq_sum[i] * inv_frames)) : -60.0f;
            if (peak_db > 0.0f) peak_db = 0.0f;
            if (rms_db > 0.0f)  rms_db = 0.0f;

            // Low-pass filter meter decay
            _meters.in_peak_db[i] = _meters.in_peak_db[i] * 0.7f + peak_db * 0.3f;
            _meters.in_rms_db[i]  = _meters.in_rms_db[i] * 0.7f + rms_db * 0.3f;
            if (in_peak[i] >= 0.999f) _meters.in_clip[i] = true;
        }

        for (int i = 0; i < 4; i++) {
            float peak_db = (out_peak[i] > 1e-4f) ? (20.0f * log10f(out_peak[i])) : -60.0f;
            float rms_db  = (out_sq_sum[i] > 1e-8f) ? (10.0f * log10f(out_sq_sum[i] * inv_frames)) : -60.0f;
            if (peak_db > 0.0f) peak_db = 0.0f;
            if (rms_db > 0.0f)  rms_db = 0.0f;

            _meters.out_peak_db[i] = _meters.out_peak_db[i] * 0.7f + peak_db * 0.3f;
            _meters.out_rms_db[i]  = _meters.out_rms_db[i] * 0.7f + rms_db * 0.3f;

            float gr_db = (_limiterMinGr[i] < 0.999f) ? (20.0f * log10f(_limiterMinGr[i])) : 0.0f;
            _limiterMinGr[i] = 1.0f; // Reset untuk blok berikutnya
            _meters.limiter_gr_db[i] = gr_db;
            _meters.limiter_active[i] = (gr_db < -0.2f);
            if (out_peak[i] >= 0.999f) _meters.out_clip[i] = true;
        }
    }

    // 6. Pengukuran Beban CPU Core 1 DSP
    int64_t elapsed_us = esp_timer_get_time() - start_time;
    float block_period_us = (float)frame_count / (float)_sampleRate * 1000000.0f; // ~666.7 us
    float current_cpu = ((float)elapsed_us / block_period_us) * 100.0f;
    if (current_cpu > 100.0f) current_cpu = 100.0f;
    _cpuUsageAvg = _cpuUsageAvg * 0.95f + current_cpu * 0.05f;
    _meters.cpu_usage_pct = _cpuUsageAvg;
    _meters.uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

VuMeterData DspEngine::getMeterData() {
    return _meters;
}

void DspEngine::clearClipFlags() {
    for (int i = 0; i < 2; i++) _meters.in_clip[i] = false;
    for (int i = 0; i < 4; i++) _meters.out_clip[i] = false;
}
