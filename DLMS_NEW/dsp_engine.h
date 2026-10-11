#pragma once
#include <Arduino.h>
#include "hal_pins.h"
#include "dsp_types.h"
#include "biquad.h"

// ============================================================================
// DSP ENGINE CORE (ESP32-S3 CORE 1 DEDICATED)
// 2 Input (In A, In B) -> Matrix Patching 2x4 -> 4 Output (Out 1..4)
// Urutan Pemrosesan:
// In Gain/Mute -> In PEQ(4) -> Matrix -> Crossover HPF/LPF -> Out PEQ(8) ->
// Polarity -> Delay(100ms) -> Out Gain -> Limiter -> Master Mute -> DAC
// ============================================================================

class DspEngine {
public:
    DspEngine();
    ~DspEngine();

    void init(uint32_t sample_rate = AUDIO_SAMPLE_RATE);

    // Ambil dan set konfigurasi aktif (Thread-safe atomic exchange)
    void setConfig(const DspPreset& config);
    const DspPreset& getConfig() const;

    // Perubahan parameter individual secara instan (live tanpa pop/klik)
    void setInputGain(uint8_t in_ch, float gain_db);
    void setInputMute(uint8_t in_ch, bool mute);
    void setInputPeq(uint8_t in_ch, uint8_t band, bool enabled, uint8_t type, float freq, float gain_db, float q, bool bypass);

    void setOutputSource(uint8_t out_ch, uint8_t source);
    void setOutputGain(uint8_t out_ch, float gain_db);
    void setOutputMute(uint8_t out_ch, bool mute);
    void setOutputPolarity(uint8_t out_ch, bool invert);
    void setOutputHpf(uint8_t out_ch, bool enabled, float freq, uint8_t type, uint8_t slope);
    void setOutputLpf(uint8_t out_ch, bool enabled, float freq, uint8_t type, uint8_t slope);
    void setOutputPeq(uint8_t out_ch, uint8_t band, bool enabled, uint8_t type, float freq, float gain_db, float q, bool bypass);
    void setOutputDelay(uint8_t out_ch, float delay_ms, uint8_t unit, float temp_c);
    void setOutputLimiter(uint8_t out_ch, bool enabled, float threshold_db, float attack_ms, float release_ms);

    void setMasterMute(bool mute);

    // Test tone generator
    void setTestSignal(bool enabled, uint8_t type, float freq, float level_db, int8_t target_ch);

    // Jalankan pemrosesan audio 1 blok (Dipanggil dari audio_task Core 1)
    // in_pcm: 32 sampel stereo int32_t dari ADC (L=In A, R=In B)
    // out1_pcm: 32 sampel stereo int32_t ke DAC1 (L=Out 1, R=Out 2)
    // out2_pcm: 32 sampel stereo int32_t ke DAC2 (L=Out 3, R=Out 4)
    void processBlock(const int32_t* in_pcm, int32_t* out1_pcm, int32_t* out2_pcm, size_t frame_count);

    // Ambil data VU Meter & Telemetri Real-time
    VuMeterData getMeterData();

    // Nilai mentah ADC terakhir (untuk diagnosa koneksi bus I2S)
    int32_t getLastRawInA() const { return _lastRawInA; }
    int32_t getLastRawInB() const { return _lastRawInB; }

    // Reset meter clip flags
    void clearClipFlags();

    uint32_t getSampleRate() const { return _sampleRate; }
    uint32_t getConfigVersion() const { return _configVersion; }

private:
    uint32_t _sampleRate;
    portMUX_TYPE _paramMux;

    DspPreset _config;
    bool _pendingConfigUpdate;
    volatile uint32_t _configVersion;

    // Linear gains dengan 10ms smooth ramp
    float _inGainLinear[2];
    float _inTargetGainLinear[2];
    float _outGainLinear[4];
    float _outTargetGainLinear[4];
    float _gainRampStep;

    // Filter Biquads
    Biquad _inPeq[2][4];               // 2 Input x 4 Band PEQ
    CrossoverCascade _outHpf[4];       // 4 Output x HPF Cascade
    CrossoverCascade _outLpf[4];       // 4 Output x LPF Cascade
    Biquad _outPeq[4][8];              // 4 Output x 8 Band PEQ

    // Delay lines (circular buffer 4800 sampel per channel = ~19.2 KB float per kanal)
    float* _delayBuffer[4];
    uint32_t _delayBufferSize;
    uint32_t _delayWriteIdx[4];
    float _delayTargetSamples[4];

    // Limiter state & lookahead buffer
    static const int LIMITER_LOOKAHEAD = 32;
    float _limiterLookahead[4][LIMITER_LOOKAHEAD];
    uint8_t _limiterLookaheadIdx[4];
    float _limiterEnvelope[4];
    float _limiterThresholdLinear[4];
    float _limiterAttackCoeff[4];
    float _limiterReleaseCoeff[4];
    float _limiterGainReductionDb[4];
    float _limiterMinGr[4];            // Minimum linear gain reduction per decimated block
    uint16_t _meterDecimateCounter;    // Counter decimation VU meter dB conversion

    // Polarity crossfade
    float _polarityGain[4];
    float _polarityTarget[4];

    // Test tone state
    bool _testSignalEnabled;
    uint8_t _testSignalType; // 0 = Sine, 1 = Pink Noise
    float _testSignalFreq;
    float _testSignalLinear;
    int8_t _testSignalTarget; // -1 = All, 0..3 = Out 1..4
    float _sinePhase;
    float _pinkB0, _pinkB1, _pinkB2; // Pink noise filter state

    // Telemetri VU Meter
    VuMeterData _meters;
    float _cpuUsageAvg;
    int32_t _lastRawInA;
    int32_t _lastRawInB;

    void updateInternalFilters();
    void updateSingleInputPeq(uint8_t in_ch, uint8_t band);
    void updateSingleOutputPeq(uint8_t out_ch, uint8_t band);
    void updateSingleOutputHpf(uint8_t out_ch);
    void updateSingleOutputLpf(uint8_t out_ch);
    void updateLimiterCoeffs(uint8_t ch);
    void updateDelaySamples(uint8_t ch);
};

extern DspEngine dspEngine;
