#pragma once
#include <Arduino.h>
#include <math.h>

// ============================================================================
// BIQUAD FILTER CORE & COEFFICIENTS GENERATOR
// Audio EQ Cookbook + Multi-Order Crossover (Butterworth, Linkwitz-Riley, Bessel)
// Direct Form II Transposed for optimal numerical stability and fast execution
// ============================================================================

struct BiquadCoeffs {
    float b0, b1, b2;
    float a1, a2;
};

class Biquad {
public:
    BiquadCoeffs coeffs[2];
    volatile uint8_t activeIdx;
    float s1, s2;

    Biquad() : activeIdx(0), s1(0.0f), s2(0.0f) {
        coeffs[0] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        coeffs[1] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    }

    inline void reset() {
        s1 = 0.0f;
        s2 = 0.0f;
    }

    inline bool isPassthrough() const {
        return (coeffs[activeIdx].b0 == 1.0f && coeffs[activeIdx].b1 == 0.0f);
    }

    inline void setPassthrough() {
        setCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    inline void setCoefficients(float nb0, float nb1, float nb2, float na1, float na2) {
        uint8_t next = 1 - activeIdx;
        coeffs[next].b0 = nb0;
        coeffs[next].b1 = nb1;
        coeffs[next].b2 = nb2;
        coeffs[next].a1 = na1;
        coeffs[next].a2 = na2;
        activeIdx = next;
    }

    // Direct Form II Transposed process: 4 FLOPs per sample (Hardware FPU accelerated)
    inline float process(float in) {
        const BiquadCoeffs& c = coeffs[activeIdx];
        float out = in * c.b0 + s1;
        s1 = in * c.b1 - out * c.a1 + s2;
        s2 = in * c.b2 - out * c.a2;

        // Anti-denormal & NaN / unstable guard (flush-to-zero)
        if (fabsf(s1) < 1e-15f) s1 = 0.0f;
        if (fabsf(s2) < 1e-15f) s2 = 0.0f;
        if (isnan(out) || isinf(out) || fabsf(out) > 8.0f) {
            out = 0.0f;
            s1 = 0.0f;
            s2 = 0.0f;
        }
        return out;
    }

    // Hitung magnitude response (dalam dB) pada frekuensi tertentu
    float getMagnitudeDb(float f, float fs) const {
        if (f <= 0.0f || fs <= 0.0f) return 0.0f;
        float w = 2.0f * (float)M_PI * f / fs;
        float cos_w = cosf(w);
        float sin_w = sinf(w);
        float cos_2w = cosf(2.0f * w);
        float sin_2w = sinf(2.0f * w);

        const BiquadCoeffs& c = coeffs[activeIdx];

        // Numerator: b0 + b1*e^(-jw) + b2*e^(-2jw)
        float num_re = c.b0 + c.b1 * cos_w + c.b2 * cos_2w;
        float num_im = -c.b1 * sin_w - c.b2 * sin_2w;
        float num_mag2 = num_re * num_re + num_im * num_im;

        // Denominator: 1 + a1*e^(-jw) + a2*e^(-2jw)
        float den_re = 1.0f + c.a1 * cos_w + c.a2 * cos_2w;
        float den_im = -c.a1 * sin_w - c.a2 * sin_2w;
        float den_mag2 = den_re * den_re + den_im * den_im;

        if (isnan(num_mag2) || isnan(den_mag2) || den_mag2 <= 1e-12f) return 0.0f;
        float mag = sqrtf(num_mag2 / den_mag2);
        if (isnan(mag) || isinf(mag)) return 0.0f;
        if (mag <= 1e-6f) return -120.0f;
        float db = 20.0f * log10f(mag);
        if (isnan(db) || isinf(db)) return 0.0f;
        return db;
    }

    // ------------------------------------------------------------------------
    // Standar Filter Low Pass & High Pass Orde 2 (Biquad)
    // ------------------------------------------------------------------------
    void setLowPass(float freq, float fs, float q = 0.7071f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        if (q < 0.01f) q = 0.01f;

        float w0 = 2.0f * (float)M_PI * freq / fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float alpha = sin_w0 / (2.0f * q);

        float a0 = 1.0f + alpha;
        float inv_a0 = 1.0f / a0;

        float b0 = ((1.0f - cos_w0) * 0.5f) * inv_a0;
        float b1 = (1.0f - cos_w0) * inv_a0;
        float b2 = b0;
        float a1 = (-2.0f * cos_w0) * inv_a0;
        float a2 = (1.0f - alpha) * inv_a0;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    void setHighPass(float freq, float fs, float q = 0.7071f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        if (q < 0.01f) q = 0.01f;

        float w0 = 2.0f * (float)M_PI * freq / fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float alpha = sin_w0 / (2.0f * q);

        float a0 = 1.0f + alpha;
        float inv_a0 = 1.0f / a0;

        float b0 = ((1.0f + cos_w0) * 0.5f) * inv_a0;
        float b1 = (-(1.0f + cos_w0)) * inv_a0;
        float b2 = b0;
        float a1 = (-2.0f * cos_w0) * inv_a0;
        float a2 = (1.0f - alpha) * inv_a0;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    // ------------------------------------------------------------------------
    // Filter Orde 1 (untuk Slope 6 dB/okt & seksi ganjil)
    // ------------------------------------------------------------------------
    void setLowPass1stOrder(float freq, float fs) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        float theta = 2.0f * (float)M_PI * freq / fs;
        float gamma = cosf(theta) / (1.0f + sinf(theta));
        float b0 = (1.0f - gamma) * 0.5f;
        float b1 = b0;
        float b2 = 0.0f;
        float a1 = -gamma;
        float a2 = 0.0f;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    void setHighPass1stOrder(float freq, float fs) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        float theta = 2.0f * (float)M_PI * freq / fs;
        float gamma = cosf(theta) / (1.0f + sinf(theta));
        float b0 = (1.0f + gamma) * 0.5f;
        float b1 = -b0;
        float b2 = 0.0f;
        float a1 = -gamma;
        float a2 = 0.0f;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    // ------------------------------------------------------------------------
    // PEQ: Peaking, Low Shelf, High Shelf, Notch
    // ------------------------------------------------------------------------
    void setPeaking(float freq, float fs, float gain_db, float q = 1.0f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        if (q < 0.05f) q = 0.05f;

        float A = powf(10.0f, gain_db / 40.0f);
        float w0 = 2.0f * (float)M_PI * freq / fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float alpha = sin_w0 / (2.0f * q);

        float a0 = 1.0f + alpha / A;
        float inv_a0 = 1.0f / a0;

        float b0 = (1.0f + alpha * A) * inv_a0;
        float b1 = (-2.0f * cos_w0) * inv_a0;
        float b2 = (1.0f - alpha * A) * inv_a0;
        float a1 = (-2.0f * cos_w0) * inv_a0;
        float a2 = (1.0f - alpha / A) * inv_a0;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    void setLowShelf(float freq, float fs, float gain_db, float q = 0.7071f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        if (q < 0.05f) q = 0.05f;

        float A = powf(10.0f, gain_db / 40.0f);
        float w0 = 2.0f * (float)M_PI * freq / fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float shelf_term = (A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f;
        if (shelf_term < 0.0f) shelf_term = 0.0f;
        float alpha = (sin_w0 * 0.5f) * sqrtf(shelf_term);
        float two_sqrt_A_alpha = 2.0f * sqrtf(A) * alpha;

        float a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + two_sqrt_A_alpha;
        float inv_a0 = 1.0f / a0;

        float b0 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + two_sqrt_A_alpha)) * inv_a0;
        float b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0)) * inv_a0;
        float b2 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha)) * inv_a0;
        float a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0)) * inv_a0;
        float a2 = ((A + 1.0f) + (A - 1.0f) * cos_w0 - two_sqrt_A_alpha) * inv_a0;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    void setHighShelf(float freq, float fs, float gain_db, float q = 0.7071f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        if (q < 0.05f) q = 0.05f;

        float A = powf(10.0f, gain_db / 40.0f);
        float w0 = 2.0f * (float)M_PI * freq / fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float shelf_term = (A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f;
        if (shelf_term < 0.0f) shelf_term = 0.0f;
        float alpha = (sin_w0 * 0.5f) * sqrtf(shelf_term);
        float two_sqrt_A_alpha = 2.0f * sqrtf(A) * alpha;

        float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + two_sqrt_A_alpha;
        float inv_a0 = 1.0f / a0;

        float b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + two_sqrt_A_alpha)) * inv_a0;
        float b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) * inv_a0;
        float b2 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha)) * inv_a0;
        float a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) * inv_a0;
        float a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha) * inv_a0;
        setCoefficients(b0, b1, b2, a1, a2);
    }

    void setNotch(float freq, float fs, float q = 10.0f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > fs * 0.45f) freq = fs * 0.45f;
        if (q < 0.1f) q = 0.1f;

        float w0 = 2.0f * (float)M_PI * freq / fs;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float alpha = sin_w0 / (2.0f * q);

        float a0 = 1.0f + alpha;
        float inv_a0 = 1.0f / a0;

        float b0 = 1.0f * inv_a0;
        float b1 = (-2.0f * cos_w0) * inv_a0;
        float b2 = 1.0f * inv_a0;
        float a1 = (-2.0f * cos_w0) * inv_a0;
        float a2 = (1.0f - alpha) * inv_a0;
        setCoefficients(b0, b1, b2, a1, a2);
    }
};

// ============================================================================
// HELPER CROSSOVER BUILDER (Kaskade hingga 4 Biquad = 8-th Order / 48 dB/oktaf)
// ============================================================================
class CrossoverCascade {
public:
    static const int MAX_BIQUADS = 4;
    Biquad biquads[MAX_BIQUADS];
    int activeCount;

    CrossoverCascade() : activeCount(0) {
        reset();
    }

    void reset() {
        for (int i = 0; i < MAX_BIQUADS; i++) {
            biquads[i].reset();
            biquads[i].setPassthrough();
        }
        activeCount = 0;
    }

    inline float process(float in) {
        float out = in;
        for (int i = 0; i < activeCount; i++) {
            out = biquads[i].process(out);
        }
        return out;
    }

    float getMagnitudeDb(float f, float fs) const {
        if (activeCount == 0) return 0.0f;
        float total_db = 0.0f;
        for (int i = 0; i < activeCount; i++) {
            total_db += biquads[i].getMagnitudeDb(f, fs);
        }
        return total_db;
    }

    // Bangun filter kaskade berdasarkan tipe, slope, dan frekuensi
    // CATATAN: State internal s1 & s2 dipertahankan saat transisi frekuensi agar TIDAK menimbulkan zipper noise/serak!
    void configure(bool isHighPass, bool enabled, float freq, float fs, uint8_t type, uint8_t slope) {
        if (!enabled || freq <= 10.0f || freq >= fs * 0.49f) {
            activeCount = 0;
            return;
        }

        int oldCount = activeCount;
        int targetCount = 1;

        if (type == 0) {
            // BUTTERWORTH (6, 12, 18, 24, 36, 48 dB/oktaf)
            switch (slope) {
                case 6:
                    if (isHighPass) biquads[0].setHighPass1stOrder(freq, fs);
                    else            biquads[0].setLowPass1stOrder(freq, fs);
                    targetCount = 1;
                    break;
                case 12:
                    if (isHighPass) biquads[0].setHighPass(freq, fs, 0.7071f);
                    else            biquads[0].setLowPass(freq, fs, 0.7071f);
                    targetCount = 1;
                    break;
                case 18:
                    if (isHighPass) {
                        biquads[0].setHighPass1stOrder(freq, fs);
                        biquads[1].setHighPass(freq, fs, 1.0f);
                    } else {
                        biquads[0].setLowPass1stOrder(freq, fs);
                        biquads[1].setLowPass(freq, fs, 1.0f);
                    }
                    targetCount = 2;
                    break;
                case 24:
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.5412f);
                        biquads[1].setHighPass(freq, fs, 1.3066f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.5412f);
                        biquads[1].setLowPass(freq, fs, 1.3066f);
                    }
                    targetCount = 2;
                    break;
                case 36:
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.5176f);
                        biquads[1].setHighPass(freq, fs, 0.7071f);
                        biquads[2].setHighPass(freq, fs, 1.9319f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.5176f);
                        biquads[1].setLowPass(freq, fs, 0.7071f);
                        biquads[2].setLowPass(freq, fs, 1.9319f);
                    }
                    targetCount = 3;
                    break;
                case 48:
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.5098f);
                        biquads[1].setHighPass(freq, fs, 0.6013f);
                        biquads[2].setHighPass(freq, fs, 0.8999f);
                        biquads[3].setHighPass(freq, fs, 2.5629f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.5098f);
                        biquads[1].setLowPass(freq, fs, 0.6013f);
                        biquads[2].setLowPass(freq, fs, 0.8999f);
                        biquads[3].setLowPass(freq, fs, 2.5629f);
                    }
                    targetCount = 4;
                    break;
                default:
                    if (isHighPass) biquads[0].setHighPass(freq, fs, 0.7071f);
                    else            biquads[0].setLowPass(freq, fs, 0.7071f);
                    targetCount = 1;
                    break;
            }
        } else if (type == 1) {
            // LINKWITZ-RILEY (12, 24, 36, 48 dB/oktaf)
            switch (slope) {
                case 12: // LR2 = 1 biquad Q=0.5
                    if (isHighPass) biquads[0].setHighPass(freq, fs, 0.5f);
                    else            biquads[0].setLowPass(freq, fs, 0.5f);
                    targetCount = 1;
                    break;
                case 24: // LR4 = 2x Butterworth Q=0.7071
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.7071f);
                        biquads[1].setHighPass(freq, fs, 0.7071f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.7071f);
                        biquads[1].setLowPass(freq, fs, 0.7071f);
                    }
                    targetCount = 2;
                    break;
                case 36: // LR6 = 2x Butterworth 3rd order
                    if (isHighPass) {
                        biquads[0].setHighPass1stOrder(freq, fs);
                        biquads[1].setHighPass(freq, fs, 1.0f);
                        biquads[2].setHighPass(freq, fs, 1.0f);
                    } else {
                        biquads[0].setLowPass1stOrder(freq, fs);
                        biquads[1].setLowPass(freq, fs, 1.0f);
                        biquads[2].setLowPass(freq, fs, 1.0f);
                    }
                    targetCount = 3;
                    break;
                case 48: // LR8 = 2x Butterworth 4th order (2x LR4)
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.5412f);
                        biquads[1].setHighPass(freq, fs, 1.3066f);
                        biquads[2].setHighPass(freq, fs, 0.5412f);
                        biquads[3].setHighPass(freq, fs, 1.3066f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.5412f);
                        biquads[1].setLowPass(freq, fs, 1.3066f);
                        biquads[2].setLowPass(freq, fs, 0.5412f);
                        biquads[3].setLowPass(freq, fs, 1.3066f);
                    }
                    targetCount = 4;
                    break;
                default:
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.7071f);
                        biquads[1].setHighPass(freq, fs, 0.7071f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.7071f);
                        biquads[1].setLowPass(freq, fs, 0.7071f);
                    }
                    targetCount = 2;
                    break;
            }
        } else if (type == 2) {
            // BESSEL (12, 18, 24 dB/oktaf)
            switch (slope) {
                case 12:
                    if (isHighPass) biquads[0].setHighPass(freq, fs, 0.5773f);
                    else            biquads[0].setLowPass(freq, fs, 0.5773f);
                    targetCount = 1;
                    break;
                case 18:
                    if (isHighPass) {
                        biquads[0].setHighPass1stOrder(freq, fs);
                        biquads[1].setHighPass(freq, fs, 0.6910f);
                    } else {
                        biquads[0].setLowPass1stOrder(freq, fs);
                        biquads[1].setLowPass(freq, fs, 0.6910f);
                    }
                    targetCount = 2;
                    break;
                case 24:
                    if (isHighPass) {
                        biquads[0].setHighPass(freq, fs, 0.5219f);
                        biquads[1].setHighPass(freq, fs, 0.8055f);
                    } else {
                        biquads[0].setLowPass(freq, fs, 0.5219f);
                        biquads[1].setLowPass(freq, fs, 0.8055f);
                    }
                    targetCount = 2;
                    break;
                default:
                    if (isHighPass) biquads[0].setHighPass(freq, fs, 0.5773f);
                    else            biquads[0].setLowPass(freq, fs, 0.5773f);
                    targetCount = 1;
                    break;
            }
        }

        // Jika jumlah biquad bertambah, inisialisasi hanya biquad indeks baru
        for (int i = oldCount; i < targetCount; i++) {
            biquads[i].reset();
        }
        // Set biquad yang tidak aktif menjadi passthrough
        for (int i = targetCount; i < MAX_BIQUADS; i++) {
            biquads[i].setPassthrough();
        }
        activeCount = targetCount;
    }
};
