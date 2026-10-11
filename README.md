# Audio DSP Speaker Processor (ESP32-S3) — DLMS v1.0

Prosesor speaker digital (DSP) 2 Input / 4 Output berbasis ESP32-S3 performa tinggi. Mengatur routing matrix 2x4, input/output gain, crossover filter multi-orde, 40-band PEQ, peak limiter dengan lookahead, delay 0-100 ms akurat sampel, dan polaritas fasa per kanal. Dikontrol penuh secara mandiri lewat layar TFT ST7735 1.8" (160x128) dan Rotary Encoder EC11 + tombol K0, serta kendali nirkabel jarak jauh via WiFi Web Dashboard responsif.

---

## 1. Spesifikasi Utama & Metrik Kinerja

| Parameter | Spesifikasi |
|---|---|
| **MCU Core** | ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz |
| **Arsitektur Task** | Core 1: `audio_task` (Prioritas Real-Time Tertinggi) <br> Core 0: `ui_task` (30 FPS Sprite) & `web_task` (WiFi/REST) |
| **Format Audio I2S** | 48 kHz, 32-bit Philips format, matematika internal float 32-bit |
| **Ukuran Blok Proses** | 32 sampel per blok (~0.67 ms), latensi total sistem < 3 ms |
| **Input Analog** | 2 Kanal (In A & In B) via PCM1808 Stereo ADC I2S |
| **Output Analog** | 4 Kanal via 2x PCM5102 Stereo DAC (DAC1 = Out 1/2, DAC2 = Out 3/4) |
| **Routing Matrix** | 2x4 Patching: In A, In B, A+B Mono Sum (-6dB), Mute/Off |
| **Parametric EQ** | 4-Band per Input (8 band) + 8-Band per Output (32 band) = Total 40 Band PEQ |
| **Crossover Filter** | Cascaded Biquads (HPF & LPF per output): <br> • Butterworth (6, 12, 18, 24, 36, 48 dB/okt) <br> • Linkwitz-Riley (12, 24, 36, 48 dB/okt: LR2, LR4, LR6, LR8) <br> • Bessel (12, 18, 24 dB/okt) |
| **Output Limiter** | Peak Limiter dengan 32-sample lookahead buffer, attack 0.1-100ms, release 10-2000ms, threshold -40 s.d 0 dBFS, real-time GR metering |
| **Time Alignment** | Delay 0 s.d 100 ms per kanal (resolusi 1 sampel = ~20.8 µs) dengan konversi jarak meter/feet dan kompensasi suhu udara |
| **Polaritas Fasa** | Normal (0°) / Invert (180°) dengan crossfade tanpa klik |
| **Penyimpanan Preset**| 10 Slot Pengguna + 1 Slot Factory (Read-Only) di Flash NVS dengan checksum CRC32 dan auto-save debounce 3 detik |
| **Layar & Antarmuka** | TFT ST7735 1.8" (160x128 landscape), Full Sprite Framebuffer 40 KB di SRAM (100% flicker-free @ 30 FPS), PWM Backlight |
| **Kendali Jarak Jauh**| WiFi SoftAP ("AudioDSP-xxxx"), Captive Portal, REST JSON API, 10 Hz Real-Time Telemetri VU Meter |

---

## 2. Diagram Pinout Hardware ESP32-S3

Semua pin telah disesuaikan dengan skema PCB dan rekomendasi PRD:

### Jalur Bus I2S Audio (Layout PCB)
| Fungsi | GPIO | Pin Modul | Keterangan |
|---|---|---|---|
| **I2S BCK (Shared)** | `GPIO 15` | BCK PCM1808 / Pin 26 ESP32 BT | Bit Clock bersama (Solder PCB) |
| **I2S LRCK / WS (Shared)** | `GPIO 16` | LRCK PCM1808 / Pin 25 ESP32 BT | Word Select bersama (Solder PCB) |
| **I2S DATA OUT ke DAC1** | `GPIO 17` | DIN PCM5102 #1 | Output Kanal 1 & 2 (Solder PCB) |
| **I2S DATA OUT ke DAC2** | `GPIO 5`  | DIN PCM5102 #2 | Output Kanal 3 & 4 (Solder PCB - TETAP AKTIF!) |
| **I2S DATA IN dari ADC/BT** | `GPIO 4`  | OUT PCM1808 / Pin 22 ESP32 BT | Input Audio Kanal A & B |
| **I2S MCLK ke ADC** | `GPIO 6`  | SCK PCM1808 | Master Clock (NC saat pakai BT) |
| **Soft-Mute DAC1** | `GPIO 7`  | XMT PCM5102 #1 | High = On, Low = Mute (Solder PCB) |
| **Soft-Mute DAC2** | `GPIO 18` | XMT PCM5102 #2 | High = On, Low = Mute (Solder PCB) |

> [!NOTE]
> **Mode Input Bluetooth Sementara (ESP32 BT Receiver ke ESP32-S3):**
> - **Pin 26 (BCK)** ESP32 BT -> **Pin BCK** Socket ADC PCB (`GPIO 15`)
> - **Pin 25 (LRCK)** ESP32 BT -> **Pin LRCK** Socket ADC PCB (`GPIO 16`)
> - **Pin 22 (DOUT)** ESP32 BT -> **Pin OUT/DATA** Socket ADC PCB (`GPIO 4`)
> - **GND** ESP32 BT -> **GND** PCB (Wajib Terhubung)
> - Dengan skema ini, **DAC2 (Output 3 & 4)** tetap menyala normal dan seluruh 4 kanal aktif!

### Layar TFT ST7735 1.8" (SPI)
| Fungsi | GPIO | Pin Layar | Keterangan |
|---|---|---|---|
| **TFT CS** | `GPIO 10` | CS | Chip Select |
| **TFT DC** | `GPIO 9`  | DC / RS | Data / Command |
| **TFT RST** | `GPIO 14` | RES / RST | Reset |
| **TFT MOSI** | `GPIO 11` | SDA / DIN | SPI MOSI Data |
| **TFT SCLK** | `GPIO 12` | SCL / SCK | SPI Clock (27 MHz) |
| **TFT Backlight** | `GPIO 8` | BLK / LED | PWM LEDC Dimmer (10 - 100%) |

### Kontrol Fisik (Header SZYTF EC11 + Tombol K0)
| Fungsi | GPIO | Pin Fisik | Keterangan |
|---|---|---|---|
| **Encoder A (CLK)** | `GPIO 41` | CLK | Fase A Interrupt |
| **Encoder B (DT)** | `GPIO 42` | DT | Fase B Interrupt |
| **Encoder SW (Push)** | `GPIO 2`  | SW / PUSH | Tombol Putar (Select / Edit / Confirm) |
| **Tombol K0** | `GPIO 1`  | K0 | Push Button (Back / Cancel / Menu) |

---

## 3. Panduan Navigasi & Kontrol Fisik

Antarmuka dirancang untuk operasi cepat di lapangan tanpa PC:

- **Di Layar HOME**:
  - **Putar Encoder**: Memilih kanal yang ingin dipantau/diatur (`In A`, `In B`, `Out 1`, `Out 2`, `Out 3`, `Out 4`).
  - **Tekan Encoder (SW)**: Masuk ke mode edit cepat gain kanal terpilih.
  - **Tahan Encoder (SW) 1 Detik**: Mute / Unmute kanal terpilih.
  - **Tekan Tombol K0**: Membuka Menu Utama (Routing, Crossover, PEQ, Limiter, Delay, Polarity, Preset, Setup).
- **Di Dalam Menu & Parameter**:
  - **Putar Encoder**: Memindahkan item kursor atau mengubah nilai parameter secara *live* (langsung terdengar di audio tanpa klik).
  - **Putar Cepat**: Akselerasi langkah otomatis (10x lipat).
  - **Putar sambil Menahan Tombol K0**: Langkah halus (*fine adjustment* 0.1 dB / 1 Hz).
  - **Tekan Encoder (SW)**: Masuk ke submenu atau konfirmasi perubahan nilai.
  - **Tekan Tombol K0**: Kembali 1 tingkat ke atas atau membatalkan perubahan (mengembalikan nilai semula).
  - **Tahan Tombol K0 1 Detik**: Pintasan langsung kembali ke Layar HOME.
- **Kunci Layar (Screen Lock)**:
  - **Tahan Tombol K0 + Encoder SW bersamaan selama 2 detik**: Mengunci / Membuka kunci layar agar pengaturan tidak sengaja terputar saat pertunjukan/acara berlangsung.

---

## 4. WiFi Remote Dashboard & Web Control

1. Saat perangkat dinyalakan, ESP32-S3 otomatis mengaktifkan WiFi SoftAP bernama:
   **`AudioDSP-xxxx`** (di mana *xxxx* adalah 4 digit unik dari MAC address modul).
2. Hubungkan HP, tablet, atau laptop ke WiFi tersebut (tanpa password default).
3. Buka browser dan akses alamat:
   **`http://192.168.4.1`**
4. Fitur Dashboard Web:
   - **Tombol MUTE ALL Darurat** di header selalu siaga.
   - **Meter Telemetri VU Real-Time 10 Hz**: Memantau level puncak (-60 s.d 0 dBFS), clip warning, dan gain reduction limiter.
   - **Editor PEQ Interaktif**: Kurva respons frekuensi dihitung secara matematis dan divisualisasikan secara langsung.
   - **Manajemen Preset**: Load, Save, dan Export/Import JSON untuk backup konfigurasi.
   - **Sinkronisasi 2-Arah**: Setiap perubahan nilai di knob LCD langsung tercermin di web, begitu juga sebaliknya secara instan.

---

## 5. Struktur Berkas Proyek

```
DLMS NEW/
├── DLMS_NEW/
│   ├── DLMS_NEW.ino         # Main Sketch & FreeRTOS Dual-Core Task Orchestration
│   ├── hal_pins.h           # Definisi Pin Hardware ESP32-S3
│   ├── dsp_types.h          # Struktur Data, Konfigurasi Preset, dan Telemetri Meter
│   ├── biquad.h             # Biquad Direct Form II Transposed & Crossover Cascades
│   ├── i2s_duplex.h         # Header Dual I2S Driver (Master I2S0 + Slave I2S1)
│   ├── i2s_duplex.cpp       # Implementasi Driver Duplex Audio & Soft-Mute XSMT
│   ├── dsp_engine.h         # Header Core 1 32-Sample Float DSP Engine
│   ├── dsp_engine.cpp       # Implementasi Matrix 2x4, PEQ, Crossover, Delay, Limiter
│   ├── storage_manager.h    # Header NVS Flash Storage & Preset Manager
│   ├── storage_manager.cpp  # Implementasi Preset 1..10, CRC32, Auto-Save Debounce 3s
│   ├── rotary_encoder.h     # Header Driver EC11 Interrupt & Debounce Tombol K0
│   ├── rotary_encoder.cpp   # Implementasi Quadrature Decoding & Multi-Button Gestures
│   ├── tft_display.h        # Header ST7735 160x128 Full Sprite Engine
│   ├── tft_display.cpp      # Implementasi UI Dashboard, Kurva Grafik, & Navigasi
│   ├── web_ui.h             # Embedded Cyberpunk Dark Glassmorphism Web App HTML/CSS/JS
│   ├── web_server_dsp.h     # Header Web Server & WiFi SoftAP
│   └── web_server_dsp.cpp   # Implementasi REST JSON API & Captive Portal
└── README.md                # Dokumentasi Lengkap Proyek
```

---

## 6. Cara Kompilasi & Upload Firmware

### Menggunakan Arduino IDE:
1. Buka Arduino IDE.
2. Buka berkas sketch: `DLMS NEW/DLMS_NEW/DLMS_NEW.ino`.
3. Pilih board: **ESP32S3 Dev Module** (`esp32:esp32:esp32s3`).
4. Pastikan pengaturan board:
   - **USB CDC On Boot**: Enabled
   - **Flash Size**: 8MB (atau sesuai modul ESP32-S3 kamu)
   - **PSRAM**: OPI PSRAM (bila modul memiliki PSRAM) / Disabled
5. Sambungkan modul ESP32-S3 via USB dan klik **Upload**.

### Menggunakan Arduino CLI:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 "DLMS_NEW"
arduino-cli upload -p /dev/cu.usbmodem* --fqbn esp32:esp32:esp32s3 "DLMS_NEW"
```

---
*Dikembangkan berdasarkan PRD Audio DSP Speaker Processor (ESP32-S3) — @SHAWIR.*
