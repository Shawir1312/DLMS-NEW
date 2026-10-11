#pragma once
#include <Arduino.h>

// ============================================================================
// AUDIO DSP SPEAKER PROCESSOR - DATA STRUCTURES & TYPES
// Sesuai PRD Bagian 4 (Requirement Fitur DSP) & Bagian 5 (UI/UX)
// ============================================================================

// Routing Source Options untuk tiap Output (Patching Matrix 2x4)
enum RoutingSource : uint8_t {
    ROUTING_IN_A = 0,    // Input A
    ROUTING_IN_B = 1,    // Input B
    ROUTING_SUM_AB = 2,  // A + B (Mono Sum)
    ROUTING_OFF = 3      // Jalur dimatikan
};

// Tipe Filter Crossover
enum FilterType : uint8_t {
    FILTER_BUTTERWORTH = 0,
    FILTER_LINKWITZ_RILEY = 1,
    FILTER_BESSEL = 2
};

// Slope Filter Crossover (dB per oktaf)
enum FilterSlope : uint8_t {
    SLOPE_6DB  = 6,
    SLOPE_12DB = 12,
    SLOPE_18DB = 18,
    SLOPE_24DB = 24,
    SLOPE_36DB = 36,
    SLOPE_48DB = 48
};

// Tipe Band Parametric Equalizer (PEQ)
enum PeqFilterType : uint8_t {
    PEQ_PEAKING = 0,
    PEQ_LOW_SHELF = 1,
    PEQ_HIGH_SHELF = 2,
    PEQ_NOTCH = 3
};

// Satuan tampilan delay
enum DelayUnit : uint8_t {
    DELAY_UNIT_MS = 0,
    DELAY_UNIT_METER = 1,
    DELAY_UNIT_FEET = 2
};

// Tema Tampilan UI Layar TFT
enum UiTheme : uint8_t {
    THEME_DARK = 0,
    THEME_LIGHT = 1,
    THEME_HIGH_CONTRAST = 2
};

// Mode WiFi
enum DspWifiMode : uint8_t {
    DSP_WIFI_OFF = 0,
    DSP_WIFI_AP = 1,
    DSP_WIFI_STA = 2
};

// Hak Akses Web
enum WebAccessMode : uint8_t {
    WEB_ACCESS_FULL = 0,
    WEB_ACCESS_READ_ONLY = 1,
    WEB_ACCESS_OFF = 2
};

// ============================================================================
// STRUKTUR KONFIGURASI FILTER & PROSESOR
// ============================================================================

// Konfigurasi 1 Band PEQ
struct PeqBandConfig {
    bool enabled;        // Aktif / Nonaktif
    uint8_t type;        // PeqFilterType: Peaking, Low Shelf, High Shelf, Notch
    float freq;          // 20.0 Hz sampai 20000.0 Hz
    float gain_db;       // -15.0 dB sampai +15.0 dB (langkah 0.1 dB)
    float q;             // 0.1 sampai 20.0 (default 1.0)
    bool bypass;         // Bypass individual band
};

// Konfigurasi Crossover (HPF atau LPF)
struct XoverFilterConfig {
    bool enabled;        // Aktif / Nonaktif
    float freq;          // 20.0 Hz sampai 20000.0 Hz (1/12 oktaf)
    uint8_t type;        // FilterType: Butterworth, Linkwitz-Riley, Bessel
    uint8_t slope;       // FilterSlope: 6, 12, 18, 24, 36, 48 dB/okt
};

// Konfigurasi Limiter per Output
struct LimiterConfig {
    bool enabled;        // On / Off
    float threshold_db;  // -40.0 dBFS sampai 0.0 dBFS (langkah 0.5 dB)
    float attack_ms;     // 0.1 ms sampai 100.0 ms (default 1.0 ms)
    float release_ms;    // 10.0 ms sampai 2000.0 ms (default 200.0 ms)
};

// Konfigurasi Delay per Output
struct DelayConfig {
    float delay_ms;      // 0.0 sampai 100.0 ms (resolusi 1 sampel = ~20.8 us)
    uint8_t unit;        // DelayUnit: ms, meter, feet
    float temp_c;        // Suhu udara (0.0 sampai 40.0 C, default 20.0 C)
};

// ============================================================================
// STRUKTUR KANAL INPUT & OUTPUT
// ============================================================================

// Kanal Input (In A dan In B)
struct InputChannelConfig {
    float gain_db;           // -60.0 sampai +12.0 dB (langkah 0.5 dB, default 0 dB)
    bool mute;               // On / Off
    PeqBandConfig peq[4];    // 4 Band PEQ per Input
};

// Kanal Output (Out 1, Out 2, Out 3, Out 4)
struct OutputChannelConfig {
    char name[12];           // Nama kanal: "Sub", "Low", "Mid", "High", atau custom
    uint8_t source;          // RoutingSource: In A, In B, A+B, Off
    float gain_db;           // -60.0 sampai +12.0 dB (langkah 0.5 dB, default 0 dB)
    bool mute;               // On / Off
    bool polarity_invert;    // Normal (0 deg) / Invert (180 deg)
    XoverFilterConfig hpf;   // High Pass Filter
    XoverFilterConfig lpf;   // Low Pass Filter
    PeqBandConfig peq[8];    // 8 Band PEQ per Output
    DelayConfig delay;       // Delay 0-100 ms
    LimiterConfig limiter;   // Peak Limiter
};

// Konfigurasi Setup Sistem
struct SetupConfig {
    uint8_t brightness_pct;  // Kecerahan layar TFT (10 - 100%)
    uint16_t auto_dim_sec;   // Redup otomatis (0 = mati, 10 - 300 detik)
    uint8_t theme;           // UiTheme: Dark, Light, High Contrast
    uint8_t orientation;     // 1 = Landscape (160x128), 3 = Inverted Landscape
    bool encoder_reversed;   // Balik arah rotary encoder
    bool encoder_accel;      // Akselerasi putar cepat (x10)
    uint16_t hold_time_ms;   // Waktu deteksi tombol tahan (default 1000 ms)
    bool auto_lock_screen;   // Kunci layar otomatis setelah tidak ada aktivitas
    bool mute_on_boot;       // Mute output saat booting
    uint8_t wifi_mode;       // WifiMode: Off, AP, Station
    uint8_t web_access;      // WebAccessMode: Penuh, Hanya lihat, Mati
    uint8_t out_mode;        // 0 = Mode 1 (Pair: Out 1-2 & Out 3-4), 1 = Mode 2 (Individual: Out 1, 2, 3, 4)
    char wifi_ssid[32];      // SSID Station
    char wifi_pass[64];      // Password Station
    char web_password[32];   // Password Web Interface
};

// Struktur Lengkap Preset / State Sistem DSP
struct DspPreset {
    uint32_t magic;          // Magic ID untuk validasi (0x444C4D53 = "DLMS")
    uint16_t version;        // Format versi (1)
    char name[16];           // Nama preset (maksimal 12 karakter + null)
    
    bool master_mute;        // Global Mute All
    InputChannelConfig in[2];// In A (0) & In B (1)
    OutputChannelConfig out[4]; // Out 1, Out 2, Out 3, Out 4
    SetupConfig setup;       // Setup & preferensi sistem
    
    uint32_t crc32;          // Checksum integritas data
};

// ============================================================================
// TELEMETRI LEVEL METER & INDIKATOR REAL-TIME
// ============================================================================
struct VuMeterData {
    // Input Level (-60.0 sampai 0.0 dBFS)
    float in_peak_db[2];     // [0] = In A, [1] = In B
    float in_rms_db[2];
    bool  in_clip[2];

    // Output Level (-60.0 sampai 0.0 dBFS)
    float out_peak_db[4];    // Out 1, Out 2, Out 3, Out 4
    float out_rms_db[4];
    bool  out_clip[4];

    // Indikator Gain Reduction Limiter per Output (0.0 sampai -30.0 dB)
    float limiter_gr_db[4];
    bool  limiter_active[4]; // true jika limiter sedang meredam sinyal

    // Telemetry Sistem
    float cpu_usage_pct;     // Beban CPU DSP Core 1
    uint32_t uptime_sec;     // Waktu aktif perangkat
    uint8_t wifi_clients;    // Jumlah klien WiFi yang terhubung
    bool screen_locked;      // Status kunci layar
};
