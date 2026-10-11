#include "tft_display.h"
#include <SPI.h>
#include <WiFi.h>
#include <math.h>
#include "dsp_engine.h"
#include "storage_manager.h"
#include "web_server_dsp.h"

TftDisplay tftDisplay;

// Hardware SPI ST7735: 27 MHz fast hardware SPI
static Adafruit_ST7735 tft(&SPI, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

// 160x128 16-Bit RGB565 Full Framebuffer Sprite (40 KB di SRAM)
static GFXcanvas16 canvas(160, 128);

// ============================================================================
// PALET WARNA MODERN CYBER DASHBOARD (RGB565)
// Disesuaikan persis dengan referensi visual gambar UI LCD
// ============================================================================
#define COLOR_BG          0x0842 // Deep Navy-Black Background (#070b12)
#define COLOR_CARD_BG     0x0CE4 // Dark Card Background (#0d1522)
#define COLOR_CARD_BORDER 0x1967 // Subtle Dark Border Frame (#1a2638)
#define COLOR_CYAN        0x06BF // Vibrant Electric Cyan (#00d2ff)
#define COLOR_CYAN_DIM    0x0333 // Muted Tech Cyan
#define COLOR_WHITE       0xFFFF // Crisp Pure White (#ffffff)
#define COLOR_GRAY        0x63B2 // Slate Silver Text (#64748b)
#define COLOR_DARK_GRAY   0x31E7 // Very Muted Frame (#334155)

// Aksen Warna Kanal Sesuai Gambar
#define COLOR_CH_INA      0x9CD3 // Slate Silver (#94a3b8)
#define COLOR_CH_INB      0x9CD3 // Slate Silver (#94a3b8)
#define COLOR_CH_SUB      0xFB6D // Coral Pink/Red (#f87171)
#define COLOR_CH_LOW      0xFE60 // Amber/Yellow (#fbbf24)
#define COLOR_CH_MID      0x36A9 // Neon Emerald Green (#34d399)
#define COLOR_CH_HIGH     0x3DFE // Sky Cyan/Blue (#38bdf8)

// Warna Meter & Badges
#define COLOR_METER_BG    0x1105 // Dark Unlit Bar (#121927)
#define COLOR_METER_GRN   0x164A // Bright Neon Green (#10b981)
#define COLOR_METER_YEL   0xFE60 // Warning Yellow (#f59e0b)
#define COLOR_METER_RED   0xF800 // Peak Clip Crimson (#ef4444)
#define COLOR_MUTE_BADGE  0xF9A7 // Coral MUTE Pill (#ef4444)
#define COLOR_LIM_BADGE   0xFE60 // Yellow LIM Indicator
#define COLOR_ALERT_RED   0xF800 // Alert Red Indicator


TftDisplay::TftDisplay()
    : _currentScreen(SCREEN_HOME),
      _isInitialized(false),
      _inEditMode(false),
      _screenLocked(false),
      _k0HoldStartTime(0),
      _k0HoldHandled(false),
      _lockProgressSec(0),
      _brightnessPct(90),
      _lastActivityTime(0),
      _isDimmed(false),
      _toastEndTime(0),
      _homeSelectedCh(2), // Default: OUT 1
      _menuIndex(0),      // Default: Mode Out
      _subItemIndex(0),
      _modeOptionSel(0),  // 0 = Mode 1 (Pair), 1 = Mode 2 (Individual)
      _peqTarget(2),      // Default: OUT 1
      _peqBand(0),        // Band 1
      _peqEditField(0),   // Default: Channel
      _xoverCh(0),        // Default: OUT 1
      _xoverEditField(0), // Default: Tab
      _routingRow(2),     // Default: OUT 1
      _routingEditCol(0),
      _presetSlotSel(1),
      _presetActionActive(false),
      _presetActionChoice(0),
      _resetConfirmActive(false),
      _resetConfirmTime(0),
      _originalFloatVal(0.0f),
      _originalIntVal(0)
{
    _toastMsg[0] = '\0';
    for (int i = 0; i < 6; i++) {
        _peakHoldDb[i] = -60.0f;
        _peakHoldTime[i] = 0;
    }
}

void TftDisplay::begin() {
    // 1. Hardware Reset ST7735 agar keluar dari status sleep/freeze
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, LOW);
    delay(20);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(50);

    // 2. Inisialisasi SPI Hardware eksplisit dengan pin konfigurasi
    SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);

    // 3. Inisialisasi Layar ST7735
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1); // Landscape 160 x 128
    tft.fillScreen(COLOR_BG);

    // 4. Inisialisasi Backlight (nyalakan penuh dulu, lalu aktifkan PWM)
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    ledcAttach(TFT_BL_PIN, 5000, 8);
    ledcWrite(TFT_BL_PIN, (uint32_t)(_brightnessPct * 255 / 100));
#else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL_PIN, 0);
    ledcWrite(0, (uint32_t)(_brightnessPct * 255 / 100));
#endif

    _isInitialized = true;
    _lastActivityTime = millis();
}

void TftDisplay::setBrightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    if (pct < 10) pct = 10;
    _brightnessPct = pct;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    ledcWrite(TFT_BL_PIN, (uint32_t)(pct * 255 / 100));
#else
    ledcWrite(0, (uint32_t)(pct * 255 / 100));
#endif
}

void TftDisplay::resetUserActivityTimer() {
    _lastActivityTime = millis();
    if (_isDimmed) {
        _isDimmed = false;
        setBrightness(_brightnessPct);
    }
}

void TftDisplay::showToast(const char* message, uint16_t duration_ms) {
    if (!message) return;
    strncpy(_toastMsg, message, sizeof(_toastMsg) - 1);
    _toastMsg[sizeof(_toastMsg) - 1] = '\0';
    _toastEndTime = millis() + duration_ms;
}

void TftDisplay::setScreen(ScreenMode mode) {
    _currentScreen = mode;
    _inEditMode = false;
    _subItemIndex = 0;
    resetUserActivityTimer();
}

void TftDisplay::toggleScreenLock() {
    _screenLocked = !_screenLocked;
    showToast(_screenLocked ? "Layar Terkunci" : "Kunci Terbuka", 1500);
}

// ============================================================================
// HELPER DRAWING: ICONS & BARS
// ============================================================================

void TftDisplay::drawArrowBack(int16_t x, int16_t y, uint16_t col) {
    // Gambar panah kembali '<' kecil
    canvas.drawLine(x + 3, y, x, y + 3, col);
    canvas.drawLine(x, y + 3, x + 3, y + 6, col);
    canvas.drawLine(x + 4, y, x + 1, y + 3, col);
    canvas.drawLine(x + 1, y + 3, x + 4, y + 6, col);
}

void TftDisplay::drawWifiIcon(int16_t x, int16_t y, uint16_t col) {
    // Gambar ikon WiFi 3 baris lengkung
    canvas.drawPixel(x + 3, y + 5, col);
    canvas.drawFastHLine(x + 2, y + 3, 3, col);
    canvas.drawFastHLine(x + 1, y + 1, 5, col);
}

void TftDisplay::drawMenuIcon(int16_t x, int16_t y, uint8_t idx, uint16_t col) {
    switch (idx) {
        case 0: // Mode Out (Pair icon ⚇)
            canvas.drawCircle(x + 2, y + 4, 2, col);
            canvas.drawCircle(x + 6, y + 4, 2, col);
            canvas.drawFastHLine(x + 4, y + 4, 1, col);
            break;
        case 1: // ⇄ Routing
            canvas.drawFastHLine(x, y + 2, 7, col);
            canvas.drawPixel(x + 6, y + 1, col);
            canvas.drawPixel(x + 6, y + 3, col);
            canvas.drawFastHLine(x + 2, y + 6, 7, col);
            canvas.drawPixel(x + 2, y + 5, col);
            canvas.drawPixel(x + 2, y + 7, col);
            break;
        case 2: // ∿ Crossover Curve
            canvas.drawPixel(x, y + 7, col);
            canvas.drawPixel(x + 1, y + 6, col);
            canvas.drawPixel(x + 2, y + 3, col);
            canvas.drawPixel(x + 3, y + 1, col);
            canvas.drawPixel(x + 4, y + 1, col);
            canvas.drawPixel(x + 5, y + 3, col);
            canvas.drawPixel(x + 6, y + 6, col);
            canvas.drawPixel(x + 7, y + 7, col);
            break;
        case 3: // ☷ PEQ Sliders
            canvas.drawFastVLine(x + 1, y, 8, col);
            canvas.drawFastVLine(x + 4, y, 8, col);
            canvas.drawFastVLine(x + 7, y, 8, col);
            canvas.drawFastHLine(x, y + 2, 3, col);
            canvas.drawFastHLine(x + 3, y + 5, 3, col);
            canvas.drawFastHLine(x + 6, y + 3, 3, col);
            break;
        case 4: // ⎍ Limiter
            canvas.drawFastHLine(x, y + 4, 3, col);
            canvas.drawFastVLine(x + 2, y + 2, 3, col);
            canvas.drawFastHLine(x + 2, y + 2, 4, col);
            canvas.drawFastVLine(x + 6, y + 2, 5, col);
            canvas.drawFastHLine(x + 6, y + 7, 3, col);
            break;
        case 5: // ◷ Delay Clock
            canvas.drawCircle(x + 4, y + 4, 4, col);
            canvas.drawFastVLine(x + 4, y + 2, 3, col);
            canvas.drawFastHLine(x + 4, y + 4, 3, col);
            break;
        case 6: // ⊘ Polarity Phase
            canvas.drawCircle(x + 4, y + 4, 4, col);
            canvas.drawLine(x + 2, y + 6, x + 6, y + 2, col);
            break;
        case 7: // 🔖 Preset Bookmark
            canvas.drawRect(x + 1, y, 6, 8, col);
            canvas.drawPixel(x + 4, y + 5, COLOR_BG);
            break;
        case 8: // ☼ Setup Sun/Gear
            canvas.drawCircle(x + 4, y + 4, 2, col);
            canvas.drawFastVLine(x + 4, y, 2, col);
            canvas.drawFastVLine(x + 4, y + 7, 2, col);
            canvas.drawFastHLine(x, y + 4, 2, col);
            canvas.drawFastHLine(x + 7, y + 4, 2, col);
            break;
    }
}

void TftDisplay::renderHeaderBar(const char* title, const char* rightTag, uint16_t titleCol, bool showBackArrow) {
    if (showBackArrow) {
        drawArrowBack(4, 3, COLOR_CYAN);
    }
    canvas.setTextSize(1);
    canvas.setTextColor(titleCol);
    canvas.setCursor(showBackArrow ? 14 : 4, 2);
    canvas.print(title);

    if (rightTag && strlen(rightTag) > 0) {
        canvas.setTextColor(COLOR_GRAY);
        int16_t tagLen = strlen(rightTag);
        canvas.setCursor(156 - tagLen * 6, 2);
        canvas.print(rightTag);
    }
    canvas.drawFastHLine(0, 11, 160, COLOR_CARD_BORDER);
}

void TftDisplay::renderFooterBar(const char* hint1, const char* hint2, const char* hint3) {
    canvas.drawFastHLine(0, 116, 160, COLOR_CARD_BORDER);
    canvas.setTextSize(1);

    if (hint1 && strlen(hint1) > 0) {
        canvas.setTextColor(COLOR_CYAN_DIM);
        canvas.setCursor(2, 119);
        canvas.print(hint1);
    }
    if (hint2 && strlen(hint2) > 0) {
        canvas.setTextColor(COLOR_GRAY);
        int16_t len2 = strlen(hint2);
        int16_t x2 = 80 - (len2 * 6) / 2;
        if (x2 < 50) x2 = 50;
        canvas.setCursor(x2, 119);
        canvas.print(hint2);
    }
    if (hint3 && strlen(hint3) > 0) {
        canvas.setTextColor(COLOR_DARK_GRAY);
        int16_t len3 = strlen(hint3);
        int16_t x3 = 158 - (len3 * 6);
        if (x3 < 112) x3 = 112;
        canvas.setCursor(x3, 119);
        canvas.print(hint3);
    }
}

// ============================================================================
// 1. LAYAR UTAMA (HOME) — PERSIS GAMBAR 1
// ============================================================================
void TftDisplay::renderHomeScreen() {
    const DspPreset& cfg = dspEngine.getConfig();
    VuMeterData m = dspEngine.getMeterData();
    uint32_t now = millis();

    // 1. Header Bar: "● 01 Konser Hall" + WiFi + "CPU 38%" atau Badge "LOCKED" jika terkunci
    if (_screenLocked) {
        canvas.fillRoundRect(2, 1, 38, 9, 2, 0xF9A7); // Amber/Red badge
        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_BG);
        canvas.setCursor(4, 2);
        canvas.print("LOCKED");
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(44, 2);
        canvas.print(cfg.name);
    } else {
        canvas.fillCircle(5, 6, 2, COLOR_METER_GRN); // Green status dot
        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_CYAN);
        canvas.setCursor(10, 2);
        canvas.printf("%s", (cfg.setup.out_mode == 0 ? "M1" : "M2"));
        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(24, 2);
        canvas.printf("%02d", storageManager.getCurrentSlot());
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(38, 2);
        canvas.print(cfg.name);
    }

    drawWifiIcon(122, 3, COLOR_CYAN);
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(132, 2);
    canvas.printf("%2d%%", (int)m.cpu_usage_pct);
    canvas.drawFastHLine(0, 11, 160, COLOR_CARD_BORDER);

    // 2. Enam Kolom Kanal VU Meter
    const char* chNames[6] = {"In A", "In B", "O 1", "O 2", "O 3", "O 4"};
    uint16_t chColors[6]   = {COLOR_CH_INA, COLOR_CH_INB, COLOR_CH_SUB, COLOR_CH_LOW, COLOR_CH_MID, COLOR_CH_HIGH};
    int16_t colX[6]        = {4, 30, 56, 82, 108, 134};
    int16_t barW = 12;
    int16_t barH = 64;
    int16_t barY = 23;

    for (int i = 0; i < 6; i++) {
        int16_t cx = colX[i];
        bool isSel = (_homeSelectedCh == i);

        // Ambil nilai dB & status kanal
        float db = 0.0f;
        float gain = 0.0f;
        bool mute = false;
        bool lim = false;

        if (i < 2) {
            db = m.in_peak_db[i];
            gain = cfg.in[i].gain_db;
            mute = cfg.in[i].mute;
        } else {
            int out_idx = i - 2;
            db = m.out_peak_db[out_idx];
            gain = cfg.out[out_idx].gain_db;
            mute = cfg.out[out_idx].mute || cfg.master_mute;
            lim = m.limiter_active[out_idx];
        }

        // Peak Hold logic (1 detik hold decay)
        if (db >= _peakHoldDb[i]) {
            _peakHoldDb[i] = db;
            _peakHoldTime[i] = now;
        } else if (now - _peakHoldTime[i] > 1000) {
            _peakHoldDb[i] -= 1.5f;
            if (_peakHoldDb[i] < -60.0f) _peakHoldDb[i] = -60.0f;
        }

        // Highlight Cyan Rounded Frame jika kanal ini sedang dipilih
        if (isSel) {
            canvas.drawRoundRect(cx - 2, 13, 26, 101, 4, COLOR_CYAN);
        }

        // Badge di atas meter: LIM (kuning) atau MUTE (merah)
        if (mute) {
            canvas.setTextSize(1);
            canvas.setTextColor(COLOR_MUTE_BADGE);
            canvas.setCursor(cx + 1, 15);
            canvas.print("MUTE");
        } else if (lim) {
            canvas.setTextSize(1);
            canvas.setTextColor(COLOR_LIM_BADGE);
            canvas.setCursor(cx + 3, 15);
            canvas.print("LIM");
        }

        // Wadah VU Meter Bar (rounded pill vertikal)
        int16_t bx = cx + 4;
        canvas.fillRoundRect(bx, barY, barW, barH, 3, COLOR_METER_BG);

        // Isi bar meter jika tidak dimute
        if (!mute) {
            float pct = (db + 60.0f) / 60.0f;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 1.0f) pct = 1.0f;

            int16_t fillH = (int16_t)(barH * pct);
            if (fillH > 0) {
                int16_t fillY = barY + barH - fillH;
                // Gradien dua warna: hijau neon di bawah, kuning di atas jika sinyal tinggi
                if (fillH <= barH - 12) {
                    canvas.fillRoundRect(bx, fillY, barW, fillH, 2, COLOR_METER_GRN);
                } else {
                    canvas.fillRoundRect(bx, fillY + 12, barW, fillH - 12, 2, COLOR_METER_GRN);
                    canvas.fillRect(bx, fillY, barW, 12, COLOR_METER_YEL);
                }
            }

            // Garis putih penahan puncak (Peak Hold Bar)
            float holdPct = (_peakHoldDb[i] + 60.0f) / 60.0f;
            if (holdPct > 0.05f) {
                int16_t holdY = barY + barH - (int16_t)(barH * holdPct);
                if (holdY < barY) holdY = barY;
                canvas.drawFastHLine(bx + 1, holdY, barW - 2, COLOR_WHITE);
            }
        }

        // Label Nama Kanal (In A, In B, O 1, O 2, O 3, O 4)
        canvas.setTextSize(1);
        canvas.setTextColor(chColors[i]);
        int16_t nameLen = strlen(chNames[i]);
        int16_t nameX = cx + 11 - (nameLen * 6) / 2;
        canvas.setCursor(nameX, 90);
        canvas.print(chNames[i]);

        // Nilai Gain Kanal (Tengah presisi)
        canvas.setTextColor(isSel && _inEditMode ? COLOR_CYAN : COLOR_GRAY);
        char gBuf[10];
        if (gain == 0.0f) snprintf(gBuf, sizeof(gBuf), " 0.0");
        else snprintf(gBuf, sizeof(gBuf), "%+.1f", gain);
        int16_t gLen = strlen(gBuf);
        int16_t gX = cx + 11 - (gLen * 6) / 2;
        canvas.setCursor(gX, 102);
        canvas.print(gBuf);
    }

    // 3. Footer Bar
    if (_screenLocked) {
        renderFooterBar("[KUNCI]", "Tahan K0 4s Buka", "");
    } else {
        renderFooterBar("O:Kanal", "SW:Gain", "K0:Menu");
    }
}

// ============================================================================
// 2. LAYAR ROUTING & GAIN — PERSIS GAMBAR 2
// ============================================================================
void TftDisplay::renderRoutingScreen() {
    renderHeaderBar("Routing dan Gain", "dB", COLOR_WHITE, true);
    const DspPreset& cfg = dspEngine.getConfig();

    const char* rowNames[6]  = {"In A", "In B", "OUT 1", "OUT 2", "OUT 3", "OUT 4"};
    uint16_t rowColors[6]    = {COLOR_CH_INA, COLOR_CH_INB, COLOR_CH_SUB, COLOR_CH_LOW, COLOR_CH_MID, COLOR_CH_HIGH};
    const char* srcLabels[4] = {"A", "B", "A+B", "Off"};

    int16_t yStart = 14;
    int16_t rowH = 16;

    for (int r = 0; r < 6; r++) {
        int16_t y = yStart + r * 17;
        bool isSel = (_routingRow == r);

        // Kotak baris: Cyan frame jika terpilih, background gelap jika tidak
        if (isSel) {
            canvas.drawRoundRect(2, y, 156, rowH, 4, COLOR_CYAN);
            canvas.fillRoundRect(3, y + 1, 154, rowH - 2, 3, COLOR_CARD_BG);
        } else {
            canvas.fillRoundRect(2, y, 156, rowH, 3, COLOR_CARD_BG);
        }

        // 1. Nama Kanal (warna khas)
        canvas.setTextSize(1);
        canvas.setTextColor(rowColors[r]);
        canvas.setCursor(6, y + 4);
        canvas.print(rowNames[r]);

        // 2. Pill Kotak Sumber (ADC L, ADC R, A+B, A, B, Off)
        canvas.fillRoundRect(34, y + 2, 24, 12, 3, 0x0821);
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(37, y + 4);
        if (r == 0) canvas.print("ADCL");
        else if (r == 1) canvas.print("ADCR");
        else canvas.print(srcLabels[cfg.out[r - 2].source]);

        // 3. Horizontal Slider Bar Gain
        float gain = (r < 2) ? cfg.in[r].gain_db : cfg.out[r - 2].gain_db;
        bool isOff = (r >= 2 && cfg.out[r - 2].source == ROUTING_OFF);
        int16_t slX = 62;
        int16_t slW = 46;
        canvas.fillRoundRect(slX, y + 6, slW, 4, 2, COLOR_DARK_GRAY);

        if (!isOff) {
            float gPct = (gain + 60.0f) / 72.0f; // -60 s.d +12 dB
            if (gPct < 0.0f) gPct = 0.0f;
            if (gPct > 1.0f) gPct = 1.0f;
            int16_t fillW = (int16_t)(slW * gPct);
            if (fillW > 0) canvas.fillRoundRect(slX, y + 6, fillW, 4, 2, rowColors[r]);
        }

        // 4. Teks Nilai Gain
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(112, y + 4);
        if (isOff) {
            canvas.print("Off");
        } else {
            if (gain == 0.0f) canvas.print(" 0.0");
            else canvas.printf("%+.1f", gain);
        }

        // 5. Pill Tombol Mute [ M ]
        bool isMuted = (r < 2) ? cfg.in[r].mute : (cfg.out[r - 2].mute || cfg.master_mute);
        if (isMuted) {
            canvas.fillRoundRect(140, y + 3, 14, 10, 2, COLOR_MUTE_BADGE);
            canvas.setTextColor(COLOR_WHITE);
        } else {
            canvas.fillRoundRect(140, y + 3, 14, 10, 2, 0x1883);
            canvas.setTextColor(COLOR_DARK_GRAY);
        }
        canvas.setCursor(144, y + 4);
        canvas.print("M");
    }

    renderFooterBar(_inEditMode ? "O:Ubah" : "O:Pilih", _inEditMode ? "SW:Simpan" : "SW:Edit", _inEditMode ? "K0:Batal" : "K0:Back");
}

// ============================================================================
// 3. LAYAR MENU UTAMA — GRID 2 KOLOM DENGAN SCROLLING 9 ITEM
// ============================================================================
void TftDisplay::renderMainMenu() {
    char indexTag[8];
    snprintf(indexTag, sizeof(indexTag), "%d/9", _menuIndex + 1);
    drawWifiIcon(130, 3, COLOR_CYAN);
    renderHeaderBar("Menu", indexTag, COLOR_WHITE, true);

    const char* titles[9]    = {"Mode Out", "Routing", "Crossover", "PEQ", "Limiter", "Delay", "Polarity", "Preset", "Setup"};
    const char* subTitles[9] = {"Mode 1 / 2", "Patch & gain", "HPF dan LPF", "Parametrik EQ",
                                "Proteksi", "ms/meter/ft", "0 / 180 deg", "10 Slot NVS", "Layar & sys"};

    int16_t cardW = 76;
    int16_t cardH = 23;
    int16_t yStart = 14;

    // Scrolling row offset agar 9 item dapat diakses lancar
    int16_t scrollRow = (_menuIndex >= 6) ? (_menuIndex / 2) - 2 : 0;

    for (int i = 0; i < 9; i++) {
        int16_t col = (i % 2); // 0 = kiri, 1 = kanan
        int16_t row = (i / 2) - scrollRow;
        if (row < 0 || row >= 4) continue;

        int16_t x = 2 + col * 80;
        int16_t y = yStart + row * 25;
        bool isSel = (_menuIndex == i);

        if (isSel) {
            canvas.drawRoundRect(x, y, cardW, cardH, 4, COLOR_CYAN);
            canvas.fillRoundRect(x + 1, y + 1, cardW - 2, cardH - 2, 3, COLOR_CARD_BG);
        } else {
            canvas.fillRoundRect(x, y, cardW, cardH, 3, COLOR_CARD_BG);
            canvas.drawRoundRect(x, y, cardW, cardH, 3, COLOR_CARD_BORDER);
        }

        drawMenuIcon(x + 5, y + 5, i, isSel ? COLOR_CYAN : COLOR_WHITE);

        canvas.setTextSize(1);
        canvas.setTextColor(isSel ? COLOR_CYAN : COLOR_WHITE);
        canvas.setCursor(x + 17, y + 3);
        canvas.print(titles[i]);

        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(x + 17, y + 13);
        char shortSub[12];
        strncpy(shortSub, subTitles[i], 10);
        shortSub[10] = '\0';
        canvas.print(shortSub);
    }

    renderFooterBar("O:Pilih", "SW:Buka", "K0:Home");
}

// ============================================================================
// 3B. LAYAR PILIHAN MODE OUTPUT (MODE 1: PAIR VS MODE 2: INDIVIDUAL)
// ============================================================================
void TftDisplay::renderModeSelectScreen() {
    renderHeaderBar("Mode Output", "Pair/4Way", COLOR_WHITE, true);
    const DspPreset& cfg = dspEngine.getConfig();
    uint8_t currentMode = cfg.setup.out_mode;

    // Card 1: Mode 1 - Stereo Pair
    int16_t y1 = 16;
    bool isSel1 = (_modeOptionSel == 0);
    bool isAct1 = (currentMode == 0);

    canvas.fillRoundRect(3, y1, 154, 46, 4, isSel1 ? COLOR_CARD_BG : 0x08A3);
    canvas.drawRoundRect(3, y1, 154, 46, 4, isSel1 ? COLOR_CYAN : COLOR_CARD_BORDER);

    canvas.setTextSize(1);
    canvas.setTextColor(isSel1 ? COLOR_CYAN : COLOR_WHITE);
    canvas.setCursor(6, y1 + 4);
    canvas.print("[1] MODE 1: PAIR");
    if (isAct1) {
        canvas.setTextColor(COLOR_METER_GRN);
        canvas.setCursor(114, y1 + 4);
        canvas.print("<AKTIF>");
    }

    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(6, y1 + 16);
    canvas.print("OUT 1-2 & 3-4 Terhubung");
    canvas.setCursor(6, y1 + 26);
    canvas.print("Settingan per pasang");
    canvas.setTextColor(COLOR_CYAN_DIM);
    canvas.setCursor(6, y1 + 36);
    canvas.print("Stereo Sub + Mid/High");

    // Card 2: Mode 2 - Individual 4-Way
    int16_t y2 = 66;
    bool isSel2 = (_modeOptionSel == 1);
    bool isAct2 = (currentMode == 1);

    canvas.fillRoundRect(3, y2, 154, 46, 4, isSel2 ? COLOR_CARD_BG : 0x08A3);
    canvas.drawRoundRect(3, y2, 154, 46, 4, isSel2 ? COLOR_CYAN : COLOR_CARD_BORDER);

    canvas.setTextColor(isSel2 ? COLOR_CYAN : COLOR_WHITE);
    canvas.setCursor(6, y2 + 4);
    canvas.print("[2] MODE 2: 4-WAY");
    if (isAct2) {
        canvas.setTextColor(COLOR_METER_GRN);
        canvas.setCursor(114, y2 + 4);
        canvas.print("<AKTIF>");
    }

    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(6, y2 + 16);
    canvas.print("OUT 1..4 Bebas Mandiri");
    canvas.setCursor(6, y2 + 26);
    canvas.print("Tiap output terpisah");
    canvas.setTextColor(COLOR_CYAN_DIM);
    canvas.setCursor(6, y2 + 36);
    canvas.print("3-Way / 4-Way Sistem");

    renderFooterBar("O:Pilih", "SW:Pilih", "K0:Back");
}

// ============================================================================
// 4. LAYAR CROSSOVER LENGKAP — HPF & LPF INDIVIDUAL DENGAN TYPE & SLOPE
// ============================================================================
void TftDisplay::renderCrossoverScreen() {
    const DspPreset& cfg = dspEngine.getConfig();
    bool isPairMode = (cfg.setup.out_mode == 0);
    uint8_t ch = _xoverCh;
    if (isPairMode && (ch == 1 || ch == 3)) ch -= 1; // Normalize ke channel awal pair (0 atau 2)

    char outTag[12];
    if (isPairMode) {
        snprintf(outTag, sizeof(outTag), "[M1] OUT %s", (ch < 2) ? "1-2" : "3-4");
    } else {
        snprintf(outTag, sizeof(outTag), "[M2] OUT %d", ch + 1);
    }
    renderHeaderBar("Crossover", outTag, COLOR_WHITE, true);

    // 1. Channel Tabs Bar
    if (isPairMode) {
        // Mode 1: 2 Tab Pasangan (OUT 1-2 dan OUT 3-4)
        const char* pairTabs[2] = {"OUT 1-2", "OUT 3-4"};
        int16_t pTabX[2]        = {20, 92};
        int16_t pTabW[2]        = {48, 48};

        for (int t = 0; t < 2; t++) {
            bool isTabSel = ((ch < 2 && t == 0) || (ch >= 2 && t == 1));
            int16_t tx = pTabX[t];

            if (isTabSel && _xoverEditField == 0) {
                canvas.fillRoundRect(tx - 3, 13, pTabW[t] + 6, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
                canvas.setTextColor(COLOR_BG);
            } else if (isTabSel) {
                canvas.drawRoundRect(tx - 3, 13, pTabW[t] + 6, 11, 2, COLOR_CYAN);
                canvas.setTextColor(COLOR_WHITE);
            } else {
                canvas.setTextColor(COLOR_GRAY);
            }
            canvas.setTextSize(1);
            canvas.setCursor(tx, 15);
            canvas.print(pairTabs[t]);
        }
    } else {
        // Mode 2: 4 Tab Individual (OUT 1, OUT 2, OUT 3, OUT 4)
        int16_t indTabX[4] = {4, 43, 82, 121};
        int16_t indTabW = 35;

        for (int t = 0; t < 4; t++) {
            bool isTabSel = (ch == t);
            int16_t tx = indTabX[t];

            if (isTabSel && _xoverEditField == 0) {
                canvas.fillRoundRect(tx, 13, indTabW, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
                canvas.setTextColor(COLOR_BG);
            } else if (isTabSel) {
                canvas.drawRoundRect(tx, 13, indTabW, 11, 2, COLOR_CYAN);
                canvas.setTextColor(COLOR_WHITE);
            } else {
                canvas.setTextColor(COLOR_GRAY);
            }
            canvas.setTextSize(1);
            canvas.setCursor(tx + 2, 15);
            canvas.printf("OUT %d", t + 1);
        }
    }

    // 2. Grid Kurva Respons Frekuensi (y = 25..63, h = 39)
    int16_t gx = 2, gy = 25, gw = 156, gh = 39;
    canvas.fillRoundRect(gx, gy, gw, gh, 3, COLOR_METER_BG);
    canvas.drawRoundRect(gx, gy, gw, gh, 3, COLOR_CARD_BORDER);

    // Grid halus
    for (int px = 28; px < gw; px += 26) {
        canvas.drawFastVLine(gx + px, gy + 1, gh - 2, 0x08A3);
    }
    for (int py = 10; py < gh; py += 10) {
        canvas.drawFastHLine(gx + 1, gy + py, gw - 2, 0x08A3);
    }

    // Evaluasi dan Gambar Kurva
    float fs = (float)dspEngine.getSampleRate();
    if (fs < 40000.0f) fs = 48000.0f;
    CrossoverCascade hpf, lpf;
    hpf.configure(true, cfg.out[ch].hpf.enabled, cfg.out[ch].hpf.freq, fs, cfg.out[ch].hpf.type, cfg.out[ch].hpf.slope);
    lpf.configure(false, cfg.out[ch].lpf.enabled, cfg.out[ch].lpf.freq, fs, cfg.out[ch].lpf.type, cfg.out[ch].lpf.slope);

    int16_t prevY = -1;
    for (int16_t px = 0; px < gw - 2; px++) {
        float f = 20.0f * powf(1000.0f, (float)px / (float)(gw - 2));
        float mag = hpf.getMagnitudeDb(f, fs) + lpf.getMagnitudeDb(f, fs);
        if (isnan(mag) || isinf(mag)) mag = -40.0f;
        if (mag < -40.0f) mag = -40.0f;
        if (mag > 6.0f)  mag = 6.0f;

        int16_t py = gy + gh - 4 - (int16_t)((mag + 40.0f) / 46.0f * (gh - 8));
        if (py < gy + 1) py = gy + 1;
        if (py > gy + gh - 2) py = gy + gh - 2;

        if (prevY >= 0) {
            canvas.drawLine(gx + 1 + px - 1, prevY, gx + 1 + px, py, COLOR_CH_MID);
        }
        prevY = py;
    }

    // Titik lingkaran cutoff
    if (cfg.out[ch].hpf.enabled) {
        float hpf_f = cfg.out[ch].hpf.freq;
        if (hpf_f < 20.0f) hpf_f = 20.0f;
        float hpf_x_ratio = log10f(hpf_f / 20.0f) / 3.0f;
        if (!isnan(hpf_x_ratio) && hpf_x_ratio >= 0.0f && hpf_x_ratio <= 1.0f) {
            int16_t cx = gx + 1 + (int16_t)(hpf_x_ratio * (gw - 2));
            if (cx >= gx + 1 && cx <= gx + gw - 2) {
                canvas.drawCircle(cx, gy + gh / 2 - 2, 3, COLOR_CYAN);
            }
        }
    }
    if (cfg.out[ch].lpf.enabled) {
        float lpf_f = cfg.out[ch].lpf.freq;
        if (lpf_f < 20.0f) lpf_f = 20.0f;
        float lpf_x_ratio = log10f(lpf_f / 20.0f) / 3.0f;
        if (!isnan(lpf_x_ratio) && lpf_x_ratio >= 0.0f && lpf_x_ratio <= 1.0f) {
            int16_t cx = gx + 1 + (int16_t)(lpf_x_ratio * (gw - 2));
            if (cx >= gx + 1 && cx <= gx + gw - 2) {
                canvas.drawCircle(cx, gy + gh / 2 - 2, 3, COLOR_CH_MID);
            }
        }
    }

    // 3. Dua Kotak Parameter di Bawah: HPF (Kiri) dan LPF (Kanan)
    int16_t cardY = 65;
    int16_t cardH = 50;
    const char* typeNames[3] = {"Butt", "Link-R", "Bessel"};

    // --- KARTU HPF (KIRI) ---
    canvas.fillRoundRect(2, cardY, 76, cardH, 3, COLOR_CARD_BG);
    canvas.drawRoundRect(2, cardY, 76, cardH, 3, COLOR_CARD_BORDER);

    // Baris 1: HPF Title + Switch ON/OFF (Field 1)
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(5, cardY + 3);
    canvas.print("HPF");

    int16_t hpfSwX = 42, hpfSwY = cardY + 2, hpfSwW = 33, hpfSwH = 10;
    bool hpfOn = cfg.out[ch].hpf.enabled;
    if (_xoverEditField == 1) {
        canvas.drawRoundRect(hpfSwX, hpfSwY, hpfSwW, hpfSwH, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.fillRoundRect(hpfSwX + 1, hpfSwY + 1, hpfSwW - 2, hpfSwH - 2, 1, hpfOn ? 0x0B85 : 0x2124);
    canvas.setTextColor(hpfOn ? COLOR_METER_GRN : COLOR_GRAY);
    canvas.setCursor(hpfSwX + (hpfOn ? 9 : 6), hpfSwY + 1);
    canvas.print(hpfOn ? "ON" : "OFF");

    // Baris 2: Freq (Field 2)
    int16_t hpfFreqY = cardY + 13;
    if (_xoverEditField == 2) {
        canvas.drawRoundRect(4, hpfFreqY, 72, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(6, hpfFreqY + 2);
    canvas.print("F:");
    canvas.setTextColor((_xoverEditField == 2 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(20, hpfFreqY + 2);
    if (cfg.out[ch].hpf.freq >= 1000.0f) canvas.printf("%.2fkHz", cfg.out[ch].hpf.freq / 1000.0f);
    else canvas.printf("%d Hz", (int)cfg.out[ch].hpf.freq);

    // Baris 3: Tipe Filter (Field 3)
    int16_t hpfTypeY = cardY + 25;
    if (_xoverEditField == 3) {
        canvas.drawRoundRect(4, hpfTypeY, 72, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(6, hpfTypeY + 2);
    canvas.print("T:");
    canvas.setTextColor((_xoverEditField == 3 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(20, hpfTypeY + 2);
    canvas.print(typeNames[cfg.out[ch].hpf.type % 3]);

    // Baris 4: Slope Filter (Field 4)
    int16_t hpfSlopeY = cardY + 37;
    if (_xoverEditField == 4) {
        canvas.drawRoundRect(4, hpfSlopeY, 72, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(6, hpfSlopeY + 2);
    canvas.print("S:");
    canvas.setTextColor((_xoverEditField == 4 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(20, hpfSlopeY + 2);
    canvas.printf("%d dB/okt", cfg.out[ch].hpf.slope);

    // --- KARTU LPF (KANAN) ---
    canvas.fillRoundRect(82, cardY, 76, cardH, 3, COLOR_CARD_BG);
    canvas.drawRoundRect(82, cardY, 76, cardH, 3, COLOR_CARD_BORDER);

    // Baris 1: LPF Title + Switch ON/OFF (Field 5)
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(85, cardY + 3);
    canvas.print("LPF");

    int16_t lpfSwX = 122, lpfSwY = cardY + 2, lpfSwW = 33, lpfSwH = 10;
    bool lpfOn = cfg.out[ch].lpf.enabled;
    if (_xoverEditField == 5) {
        canvas.drawRoundRect(lpfSwX, lpfSwY, lpfSwW, lpfSwH, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.fillRoundRect(lpfSwX + 1, lpfSwY + 1, lpfSwW - 2, lpfSwH - 2, 1, lpfOn ? 0x0B85 : 0x2124);
    canvas.setTextColor(lpfOn ? COLOR_METER_GRN : COLOR_GRAY);
    canvas.setCursor(lpfSwX + (lpfOn ? 9 : 6), lpfSwY + 1);
    canvas.print(lpfOn ? "ON" : "OFF");

    // Baris 2: Freq (Field 6)
    int16_t lpfFreqY = cardY + 13;
    if (_xoverEditField == 6) {
        canvas.drawRoundRect(84, lpfFreqY, 72, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(86, lpfFreqY + 2);
    canvas.print("F:");
    canvas.setTextColor((_xoverEditField == 6 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(100, lpfFreqY + 2);
    if (cfg.out[ch].lpf.freq >= 1000.0f) canvas.printf("%.2fkHz", cfg.out[ch].lpf.freq / 1000.0f);
    else canvas.printf("%d Hz", (int)cfg.out[ch].lpf.freq);

    // Baris 3: Tipe Filter (Field 7)
    int16_t lpfTypeY = cardY + 25;
    if (_xoverEditField == 7) {
        canvas.drawRoundRect(84, lpfTypeY, 72, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(86, lpfTypeY + 2);
    canvas.print("T:");
    canvas.setTextColor((_xoverEditField == 7 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(100, lpfTypeY + 2);
    canvas.print(typeNames[cfg.out[ch].lpf.type % 3]);

    // Baris 4: Slope Filter (Field 8)
    int16_t lpfSlopeY = cardY + 37;
    if (_xoverEditField == 8) {
        canvas.drawRoundRect(84, lpfSlopeY, 72, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(86, lpfSlopeY + 2);
    canvas.print("S:");
    canvas.setTextColor((_xoverEditField == 8 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(100, lpfSlopeY + 2);
    canvas.printf("%d dB/okt", cfg.out[ch].lpf.slope);

    renderFooterBar(_inEditMode ? "O:Ubah" : "O:Pilih", _inEditMode ? "SW:Simpan" : "SW:Edit", _inEditMode ? "K0:Batal" : "K0:Back");
}

// ============================================================================
// 5. LAYAR PEQ LENGKAP — INPUT A/B & OUTPUT 1..4 DENGAN SEMUA PARAMETER
// ============================================================================
void TftDisplay::renderPeqScreen() {
    const DspPreset& cfg = dspEngine.getConfig();
    bool isPairMode = (cfg.setup.out_mode == 0);
    int maxTargets = isPairMode ? 4 : 6;
    if (_peqTarget < 0) _peqTarget = 0;
    if (_peqTarget >= maxTargets) _peqTarget = maxTargets - 1;

    bool isInput = (_peqTarget < 2);
    uint8_t in_ch = _peqTarget;
    uint8_t out_ch = isPairMode ? ((_peqTarget - 2) * 2) : (_peqTarget - 2);

    int numBands = isInput ? 4 : 8;
    if (_peqBand < 0) _peqBand = 0;
    if (_peqBand >= numBands) _peqBand = numBands - 1;
    uint8_t b = _peqBand;

    const PeqBandConfig& actP = isInput ? cfg.in[in_ch].peq[b] : cfg.out[out_ch].peq[b];

    // Header: "< PEQ: " + Channel Name + "Bnd X/Y"
    char bandTag[12];
    snprintf(bandTag, sizeof(bandTag), "Bnd %d/%d", b + 1, numBands);

    drawArrowBack(4, 3, COLOR_CYAN);
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(14, 2);
    canvas.print("PEQ: ");

    char chName[12];
    if (isInput) {
        snprintf(chName, sizeof(chName), "%s", in_ch == 0 ? "IN A" : "IN B");
        canvas.setTextColor(COLOR_CYAN);
    } else {
        if (isPairMode) {
            snprintf(chName, sizeof(chName), "%s", out_ch == 0 ? "OUT 1-2" : "OUT 3-4");
        } else {
            snprintf(chName, sizeof(chName), "OUT %d", out_ch + 1);
        }
        canvas.setTextColor(COLOR_CH_MID);
    }
    canvas.print(chName);

    canvas.setTextColor(actP.bypass ? COLOR_ALERT_RED : COLOR_GRAY);
    canvas.setCursor(156 - strlen(bandTag) * 6, 2);
    canvas.print(bandTag);
    canvas.drawFastHLine(0, 11, 160, COLOR_CARD_BORDER);

    // 1. Channel Tabs Bar (y = 13..23, h = 11) - Field 0
    if (isPairMode) {
        const char* pairTabs[4] = {"IN A", "IN B", "OUT 1-2", "OUT 3-4"};
        int16_t tabW = 37;
        for (int t = 0; t < 4; t++) {
            int16_t tx = 2 + t * 39;
            bool isTabSel = (_peqTarget == t);
            if (isTabSel && _peqEditField == 0) {
                canvas.fillRoundRect(tx, 13, tabW, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
                canvas.setTextColor(COLOR_BG);
            } else if (isTabSel) {
                canvas.drawRoundRect(tx, 13, tabW, 11, 2, COLOR_CYAN);
                canvas.setTextColor(COLOR_WHITE);
            } else {
                canvas.fillRoundRect(tx, 13, tabW, 11, 2, COLOR_CARD_BG);
                canvas.setTextColor(COLOR_GRAY);
            }
            canvas.setCursor(tx + 2, 15);
            canvas.print(pairTabs[t]);
        }
    } else {
        const char* indTabs[6] = {"IN A", "IN B", "O 1", "O 2", "O 3", "O 4"};
        int16_t tabW = 24;
        for (int t = 0; t < 6; t++) {
            int16_t tx = 2 + t * 26;
            bool isTabSel = (_peqTarget == t);
            if (isTabSel && _peqEditField == 0) {
                canvas.fillRoundRect(tx, 13, tabW, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
                canvas.setTextColor(COLOR_BG);
            } else if (isTabSel) {
                canvas.drawRoundRect(tx, 13, tabW, 11, 2, COLOR_CYAN);
                canvas.setTextColor(COLOR_WHITE);
            } else {
                canvas.fillRoundRect(tx, 13, tabW, 11, 2, COLOR_CARD_BG);
                canvas.setTextColor(COLOR_GRAY);
            }
            canvas.setCursor(tx + 2, 15);
            canvas.print(indTabs[t]);
        }
    }

    // 2. Grid Kurva Respons Frekuensi PEQ (y = 25..55, h = 31)
    int16_t gx = 2, gy = 25, gw = 156, gh = 31;
    canvas.fillRoundRect(gx, gy, gw, gh, 2, COLOR_METER_BG);
    canvas.drawRoundRect(gx, gy, gw, gh, 2, COLOR_CARD_BORDER);

    int16_t zeroY = gy + gh / 2;
    for (int x = gx + 2; x < gx + gw - 2; x += 4) {
        canvas.drawFastHLine(x, zeroY, 2, 0x18C4);
    }

    float fs = (float)dspEngine.getSampleRate();
    if (fs < 40000.0f) fs = 48000.0f;

    // Periksa apakah ada band aktif yang tidak flat \u2014 jika tidak ada, hanya gambar garis datar
    bool hasCurve = false;
    for (int band = 0; band < numBands && !hasCurve; band++) {
        const PeqBandConfig& p = isInput ? cfg.in[in_ch].peq[band] : cfg.out[out_ch].peq[band];
        if (p.enabled && !p.bypass && fabsf(p.gain_db) > 0.05f) hasCurve = true;
    }

    if (!hasCurve) {
        // Flat response \u2014 hanya satu garis horizontal, 0 CPU
        canvas.drawFastHLine(gx + 1, zeroY, gw - 2, COLOR_CYAN);
    } else {
        // Sample setiap STEP pixel, interpolasi linear antar titik (154 -> ~52 evaluasi)
        const int STEP = 3;
        Biquad bqs[8];
        for (int band = 0; band < numBands; band++) {
            const PeqBandConfig& p = isInput ? cfg.in[in_ch].peq[band] : cfg.out[out_ch].peq[band];
            if (p.enabled && !p.bypass) {
                if (p.type == PEQ_LOW_SHELF) bqs[band].setLowShelf(p.freq, fs, p.gain_db, p.q);
                else if (p.type == PEQ_HIGH_SHELF) bqs[band].setHighShelf(p.freq, fs, p.gain_db, p.q);
                else if (p.type == PEQ_NOTCH) bqs[band].setNotch(p.freq, fs, p.q);
                else bqs[band].setPeaking(p.freq, fs, p.gain_db, p.q);
            } else {
                bqs[band].setPassthrough();
            }
        }

        // Hitung respons pada titik-titik sample saja, lalu drawLine antar titik
        // Gunakan log-space freq dengan incremental multiplier (no powf per pixel)
        float log_f_mul = powf(1000.0f, (float)STEP / (float)(gw - 2));
        float f = 20.0f;

        int16_t prevX = gx + 1, prevY_c = zeroY;
        for (int16_t px = 0; px < gw - 2; px += STEP) {
            float totalDb = 0.0f;
            for (int band = 0; band < numBands; band++) {
                const PeqBandConfig& p = isInput ? cfg.in[in_ch].peq[band] : cfg.out[out_ch].peq[band];
                if (p.enabled && !p.bypass && fabsf(p.gain_db) > 0.05f) {
                    totalDb += bqs[band].getMagnitudeDb(f, fs);
                }
            }

            if (isnan(totalDb) || isinf(totalDb)) totalDb = 0.0f;
            if (totalDb < -15.0f) totalDb = -15.0f;
            if (totalDb >  15.0f) totalDb =  15.0f;

            int16_t curX = gx + 1 + px;
            int16_t curY_c = zeroY - (int16_t)(totalDb / 18.0f * (gh / 2 - 2));
            if (curY_c < gy + 1) curY_c = gy + 1;
            if (curY_c > gy + gh - 2) curY_c = gy + gh - 2;

            if (px > 0) {
                canvas.drawLine(prevX, prevY_c, curX, curY_c, COLOR_CYAN);
            }
            prevX = curX;
            prevY_c = curY_c;
            f *= log_f_mul;
        }
    }


    // Node Titik Band pada kurva
    for (int band = 0; band < numBands; band++) {
        const PeqBandConfig& p = isInput ? cfg.in[in_ch].peq[band] : cfg.out[out_ch].peq[band];
        float pf = p.freq;
        if (pf < 20.0f) pf = 20.0f;
        float x_ratio = log10f(pf / 20.0f) / 3.0f;
        if (!isnan(x_ratio) && x_ratio >= 0.0f && x_ratio <= 1.0f) {
            int16_t nx = gx + 1 + (int16_t)(x_ratio * (gw - 2));
            float g = p.gain_db;
            if (isnan(g) || isinf(g)) g = 0.0f;
            if (g < -15.0f) g = -15.0f;
            if (g > 15.0f) g = 15.0f;
            int16_t ny = zeroY - (int16_t)(g / 18.0f * (gh / 2 - 2));
            if (ny < gy + 2) ny = gy + 2;
            if (ny > gy + gh - 3) ny = gy + gh - 3;

            if (band == b) {
                canvas.fillCircle(nx, ny, 4, COLOR_CYAN);
                canvas.setTextColor(COLOR_BG);
                canvas.setCursor(nx - 2, ny - 3);
                canvas.print(band + 1);
            } else {
                canvas.fillCircle(nx, ny, 3, 0x1927);
                canvas.drawCircle(nx, ny, 3, COLOR_CYAN_DIM);
                canvas.setTextColor(COLOR_GRAY);
                canvas.setCursor(nx - 2, ny - 3);
                canvas.print(band + 1);
            }
        }
    }

    // 3. Band Tabs Row (y = 57..67, h = 11) - Field 1
    int16_t bTabW = isInput ? 37 : 18;
    int16_t bTabSpacing = isInput ? 39 : 20;
    for (int band = 0; band < numBands; band++) {
        int16_t tx = 2 + band * bTabSpacing;
        bool isThisBand = (band == b);
        if (isThisBand && _peqEditField == 1) {
            canvas.fillRoundRect(tx, 57, bTabW, 11, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
            canvas.setTextColor(COLOR_BG);
        } else if (isThisBand) {
            canvas.fillRoundRect(tx, 57, bTabW, 11, 2, COLOR_CYAN);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.fillRoundRect(tx, 57, bTabW, 11, 2, COLOR_CARD_BG);
            canvas.setTextColor(COLOR_GRAY);
        }
        canvas.setCursor(tx + (isInput ? 12 : 6), 59);
        canvas.print(band + 1);
    }

    // 4. Parameter Cards: Freq, Gain, Q (y = 69..94, h = 25)
    // Card 1: Freq (Field 2)
    canvas.fillRoundRect(2, 69, 48, 25, 3, COLOR_CARD_BG);
    if (_peqEditField == 2) canvas.drawRoundRect(2, 69, 48, 25, 3, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    else canvas.drawRoundRect(2, 69, 48, 25, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(5, 71); canvas.print("Freq");
    canvas.setTextColor((_peqEditField == 2 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(5, 82);
    if (actP.freq >= 1000.0f) canvas.printf("%.2fk", actP.freq / 1000.0f);
    else canvas.printf("%dHz", (int)actP.freq);

    // Card 2: Gain (Field 3)
    canvas.fillRoundRect(53, 69, 54, 25, 3, COLOR_CARD_BG);
    if (_peqEditField == 3) canvas.drawRoundRect(53, 69, 54, 25, 3, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    else canvas.drawRoundRect(53, 69, 54, 25, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(56, 71); canvas.print("Gain");
    canvas.setTextColor((_peqEditField == 3 && _inEditMode) ? COLOR_METER_GRN : COLOR_CYAN);
    canvas.setCursor(56, 82);
    canvas.printf("%+.1fdB", actP.gain_db);

    // Card 3: Q Factor (Field 4)
    canvas.fillRoundRect(110, 69, 48, 25, 3, COLOR_CARD_BG);
    if (_peqEditField == 4) canvas.drawRoundRect(110, 69, 48, 25, 3, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    else canvas.drawRoundRect(110, 69, 48, 25, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(113, 71); canvas.print("Q");
    canvas.setTextColor((_peqEditField == 4 && _inEditMode) ? COLOR_METER_GRN : COLOR_WHITE);
    canvas.setCursor(113, 82);
    canvas.printf("%.1f", actP.q);

    // 5. Tipe Filter Pills (Field 5) & Bypass Toggle Button (Field 6) (y = 96..112, h = 16)
    // Tipe Filter
    const char* typePills[4] = {"Pk", "Lo-S", "Hi-S", "Ntch"};
    int16_t pillX[4]         = {2, 28, 56, 84};
    int16_t pillW[4]         = {25, 27, 27, 26};

    if (_peqEditField == 5) {
        canvas.drawRoundRect(1, 95, 111, 18, 2, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }

    for (int pt = 0; pt < 4; pt++) {
        bool isTypeSel = (actP.type == pt);
        if (isTypeSel) {
            canvas.fillRoundRect(pillX[pt], 97, pillW[pt], 14, 2, COLOR_CYAN);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.fillRoundRect(pillX[pt], 97, pillW[pt], 14, 2, COLOR_CARD_BG);
            canvas.setTextColor(COLOR_GRAY);
        }
        canvas.setCursor(pillX[pt] + 2, 100);
        canvas.print(typePills[pt]);
    }

    // Bypass Button (Field 6)
    int16_t bypX = 114, bypY = 96, bypW = 44, bypH = 16;
    if (_peqEditField == 6) {
        canvas.drawRoundRect(bypX, bypY, bypW, bypH, 3, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    }
    canvas.fillRoundRect(bypX + 1, bypY + 1, bypW - 2, bypH - 2, 2, actP.bypass ? 0x6000 : 0x0280);
    canvas.setTextColor(actP.bypass ? COLOR_ALERT_RED : COLOR_METER_GRN);
    canvas.setCursor(bypX + 5, bypY + 4);
    canvas.print(actP.bypass ? "BYPASS" : "ACTIVE");

    renderFooterBar(_inEditMode ? "O:Ubah" : "O:Pilih", _inEditMode ? "SW:Simpan" : "SW:Edit", _inEditMode ? "K0:Batal" : "K0:Back");
}

// ============================================================================
// 6. LAYAR LAINNYA (LIMITER, DELAY, POLARITY, PRESET, SETUP)
// ============================================================================
void TftDisplay::renderLimiterScreen() {
    renderHeaderBar("Limiter", "Proteksi", COLOR_WHITE, true);
    const DspPreset& cfg = dspEngine.getConfig();
    VuMeterData m = dspEngine.getMeterData();
    bool isPairMode = (cfg.setup.out_mode == 0);

    int16_t yStart = 16;
    for (int i = 0; i < 4; i++) {
        int16_t y = yStart + i * 24;
        bool isSel = (_subItemIndex == i);

        if (isSel) canvas.drawRoundRect(2, y, 156, 22, 3, COLOR_CYAN);
        else canvas.fillRoundRect(2, y, 156, 22, 3, COLOR_CARD_BG);

        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_CH_MID);
        canvas.setCursor(6, y + 3);
        if (isPairMode) {
            canvas.printf("OUT %s", (i < 2) ? "1-2" : "3-4");
        } else {
            canvas.printf("OUT %d", i + 1);
        }

        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(76, y + 3);
        canvas.printf("Thr:%+.1fdB", cfg.out[i].limiter.threshold_db);

        // Meter Gain Reduction
        canvas.fillRect(6, y + 13, 144, 4, COLOR_DARK_GRAY);
        float gr = m.limiter_gr_db[i];
        if (gr < 0.0f) {
            int16_t gw = (int16_t)((-gr / 30.0f) * 144);
            if (gw > 144) gw = 144;
            canvas.fillRect(6, y + 13, gw, 4, COLOR_LIM_BADGE);
        }
    }
    renderFooterBar(_inEditMode ? "O:Ubah" : "O:Pilih", _inEditMode ? "SW:Simpan" : "SW:Edit", _inEditMode ? "K0:Batal" : "K0:Back");
}

void TftDisplay::renderDelayScreen() {
    renderHeaderBar("Delay", "Time Align", COLOR_WHITE, true);
    const DspPreset& cfg = dspEngine.getConfig();
    bool isPairMode = (cfg.setup.out_mode == 0);

    int16_t yStart = 16;
    for (int i = 0; i < 4; i++) {
        int16_t y = yStart + i * 24;
        bool isSel = (_subItemIndex == i);

        if (isSel) canvas.drawRoundRect(2, y, 156, 22, 3, COLOR_CYAN);
        else canvas.fillRoundRect(2, y, 156, 22, 3, COLOR_CARD_BG);

        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_CH_MID);
        canvas.setCursor(6, y + 3);
        if (isPairMode) {
            canvas.printf("OUT %s", (i < 2) ? "1-2" : "3-4");
        } else {
            canvas.printf("OUT %d", i + 1);
        }

        float c = 331.3f + 0.606f * cfg.out[i].delay.temp_c;
        float dist_m = (cfg.out[i].delay.delay_ms * 0.001f) * c;

        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(6, y + 12);
        canvas.printf("%.2f ms (%.2fm)", cfg.out[i].delay.delay_ms, dist_m);
    }
    renderFooterBar(_inEditMode ? "O:Ubah" : "O:Pilih", _inEditMode ? "SW:Simpan" : "SW:Edit", _inEditMode ? "K0:Batal" : "K0:Back");
}

void TftDisplay::renderPolarityScreen() {
    renderHeaderBar("Polarity Phase", "0 / 180", COLOR_WHITE, true);
    const DspPreset& cfg = dspEngine.getConfig();
    bool isPairMode = (cfg.setup.out_mode == 0);

    int16_t yStart = 16;
    for (int i = 0; i < 4; i++) {
        int16_t y = yStart + i * 24;
        bool isSel = (_subItemIndex == i);

        if (isSel) canvas.drawRoundRect(2, y, 156, 22, 3, COLOR_CYAN);
        else canvas.fillRoundRect(2, y, 156, 22, 3, COLOR_CARD_BG);

        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_CH_MID);
        canvas.setCursor(6, y + 6);
        if (isPairMode) {
            canvas.printf("OUT %d (Pair %s)", i + 1, (i < 2) ? "1-2" : "3-4");
        } else {
            canvas.printf("OUT %d (%s)", i + 1, cfg.out[i].name);
        }

        bool inv = cfg.out[i].polarity_invert;
        canvas.setTextColor(inv ? COLOR_CH_SUB : COLOR_CH_MID);
        canvas.setCursor(106, y + 6);
        canvas.printf("%s", inv ? "INV 180" : "NORM 0");
    }
    renderFooterBar("O:Kanal", "SW:Balik", "K0:Back");
}

void TftDisplay::renderPresetScreen() {
    renderHeaderBar("Preset", "10 Slot", COLOR_WHITE, true);

    // Tentukan jendela scroll 5 item (topSlot = 1..6)
    int topSlot = _presetSlotSel - 2;
    if (topSlot < 1) topSlot = 1;
    if (topSlot > 6) topSlot = 6;

    int16_t yStart = 15;
    for (int i = 0; i < 5; i++) {
        uint8_t slot = topSlot + i;
        int16_t y = yStart + i * 19;
        bool isSel = (_presetSlotSel == slot);
        bool isCurrentActive = (slot == storageManager.getCurrentSlot());

        char slotName[16];
        storageManager.getSlotName(slot, slotName, sizeof(slotName));

        // Background item
        canvas.fillRoundRect(2, y, 149, 17, 3, isSel ? 0x0185 : COLOR_CARD_BG);
        if (isSel) {
            canvas.drawRoundRect(2, y, 149, 17, 3, COLOR_CYAN);
        } else {
            canvas.drawRoundRect(2, y, 149, 17, 3, COLOR_CARD_BORDER);
        }

        // Teks Slot & Nama
        canvas.setTextSize(1);
        canvas.setTextColor(isSel ? COLOR_CYAN : COLOR_WHITE);
        canvas.setCursor(6, y + 5);
        canvas.printf("%02d: %s", slot, slotName);

        // Badge Status [AKTIF]
        if (isCurrentActive) {
            canvas.fillRoundRect(106, y + 3, 42, 11, 2, 0x0280);
            canvas.setTextColor(COLOR_METER_GRN);
            canvas.setCursor(110, y + 5);
            canvas.print("AKTIF");
        }
    }

    // Scrollbar di sebelah kanan (x = 154..157, y = 15..110)
    canvas.drawFastVLine(155, 15, 95, COLOR_DARK_GRAY);
    int16_t thumbY = 15 + (int16_t)((float)(_presetSlotSel - 1) / 9.0f * (95 - 20));
    canvas.fillRoundRect(154, thumbY, 3, 20, 1, COLOR_CYAN);

    // Modal dialog konfirmasi LOAD / SIMPAN / BATAL jika _presetActionActive
    if (_presetActionActive) {
        int16_t mx = 8, my = 30, mw = 144, mh = 66;
        canvas.fillRoundRect(mx, my, mw, mh, 4, 0x0842);
        canvas.drawRoundRect(mx, my, mw, mh, 4, COLOR_CYAN);

        char slotName[16];
        storageManager.getSlotName(_presetSlotSel, slotName, sizeof(slotName));

        // Judul Modal
        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(mx + 8, my + 6);
        canvas.printf("Pilih Aksi Slot %02d", _presetSlotSel);
        canvas.drawFastHLine(mx + 4, my + 17, mw - 8, COLOR_CARD_BORDER);

        // 3 Tombol Aksi: [ LOAD ] [ SIMPAN ] [ BATAL ]
        const char* actLabels[3] = {"LOAD", "SIMPAN", "BATAL"};
        int16_t actX[3] = {mx + 8, mx + 50, mx + 98};
        int16_t actW[3] = {38, 44, 38};

        for (int a = 0; a < 3; a++) {
            bool isActSel = (_presetActionChoice == a);
            if (isActSel) {
                canvas.fillRoundRect(actX[a], my + 24, actW[a], 18, 3, (a == 1) ? COLOR_METER_GRN : COLOR_CYAN);
                canvas.setTextColor(COLOR_BG);
            } else {
                canvas.fillRoundRect(actX[a], my + 24, actW[a], 18, 3, COLOR_CARD_BG);
                canvas.drawRoundRect(actX[a], my + 24, actW[a], 18, 3, COLOR_CARD_BORDER);
                canvas.setTextColor(COLOR_WHITE);
            }
            canvas.setCursor(actX[a] + 4, my + 29);
            canvas.print(actLabels[a]);
        }

        // Keterangan di bawah tombol
        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(mx + 10, my + 49);
        if (_presetActionChoice == 0) canvas.print("Muat ke DSP aktif");
        else if (_presetActionChoice == 1) canvas.print("Simpan DSP ke slot");
        else canvas.print("Batal / kembali");
    }

    renderFooterBar(_presetActionActive ? "O:Aksi" : "O:Slot", _presetActionActive ? "SW:Pilih" : "SW:Aksi", _presetActionActive ? "K0:Batal" : "K0:Menu");
}

void TftDisplay::renderSetupScreen() {
    renderHeaderBar("Setup", "WiFi dan Web", COLOR_WHITE, true);
    const DspPreset& cfg = dspEngine.getConfig();

    // 1. Box Status WiFi (y = 13..41, h = 28)
    canvas.fillRoundRect(2, 13, 156, 28, 3, COLOR_CARD_BG);
    canvas.drawRoundRect(2, 13, 156, 28, 3, 0x03EF);

    drawWifiIcon(6, 17, (cfg.setup.wifi_mode != DSP_WIFI_OFF) ? COLOR_CYAN : COLOR_GRAY);

    canvas.setTextSize(1);
    if (cfg.setup.wifi_mode == DSP_WIFI_OFF) {
        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(26, 16); canvas.print("WiFi Nonaktif");
        canvas.setCursor(26, 27); canvas.print("Radio Off");
    } else if (cfg.setup.wifi_mode == DSP_WIFI_STA) {
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(26, 15);
        bool staConn = (webServerDsp.getWifiMode() == DSP_WIFI_STA && WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED);
        canvas.print(staConn ? "STA Terhubung" : "STA Mencari...");
        canvas.setTextColor(COLOR_CYAN);
        canvas.setCursor(26, 23);
        canvas.print(staConn ? WiFi.SSID().c_str() : (strlen(cfg.setup.wifi_ssid) > 0 ? cfg.setup.wifi_ssid : "Belum diatur"));
        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(26, 31);
        canvas.print(webServerDsp.getIpAddress().c_str());

        if (staConn) {
            canvas.setTextColor(COLOR_METER_GRN);
            canvas.setCursor(118, 16); canvas.printf("%ddBm", WiFi.RSSI());
        }
    } else { // DSP_WIFI_AP
        canvas.setTextColor(COLOR_CYAN);
        canvas.setCursor(26, 15); canvas.printf("AP: %s", webServerDsp.getSsid().c_str());
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(26, 23); canvas.printf("Pass: %s", webServerDsp.getPassword().c_str());
        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(26, 31); canvas.printf("IP: %s", webServerDsp.getIpAddress().c_str());

        // Klien count
        canvas.setTextColor(COLOR_METER_GRN);
        canvas.setCursor(144, 16); canvas.printf("%d", webServerDsp.getClientCount());
        canvas.setTextColor(COLOR_GRAY);
        canvas.setCursor(126, 26); canvas.print("klien");
    }

    // 2. Card: Mode WiFi (y = 43..60, h = 17) - Row 0
    canvas.fillRoundRect(2, 43, 156, 17, 3, COLOR_CARD_BG);
    if (_subItemIndex == 0) canvas.drawRoundRect(2, 43, 156, 17, 3, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    else canvas.drawRoundRect(2, 43, 156, 17, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(6, 48); canvas.print("Mode WiFi");

    const char* wifiModes[3] = {"Off", "AP", "Station"};
    int16_t mX[3] = {76, 102, 126};
    int16_t mW[3] = {24, 22, 30};
    for (int m = 0; m < 3; m++) {
        bool isModeActive = (cfg.setup.wifi_mode == m);
        if (isModeActive) {
            canvas.fillRoundRect(mX[m], 45, mW[m], 13, 2, COLOR_CYAN);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.fillRoundRect(mX[m], 45, mW[m], 13, 2, 0x1105);
            canvas.setTextColor(COLOR_GRAY);
        }
        canvas.setCursor(mX[m] + 2, 48);
        canvas.print(wifiModes[m]);
    }

    // 3. Card: Akses web (y = 62..77, h = 15) - Row 1
    canvas.fillRoundRect(2, 62, 156, 15, 3, COLOR_CARD_BG);
    if (_subItemIndex == 1) canvas.drawRoundRect(2, 62, 156, 15, 3, _inEditMode ? COLOR_METER_GRN : COLOR_CYAN);
    else canvas.drawRoundRect(2, 62, 156, 15, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(6, 65); canvas.print("Akses web");
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(110, 65);
    canvas.print(cfg.setup.web_access == 0 ? "Penuh >" : "Lihat >");

    // 4. Card: Password web (y = 79..94, h = 15) - Row 2
    canvas.fillRoundRect(2, 79, 156, 15, 3, COLOR_CARD_BG);
    if (_subItemIndex == 2) canvas.drawRoundRect(2, 79, 156, 15, 3, COLOR_CYAN);
    else canvas.drawRoundRect(2, 79, 156, 15, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(6, 82); canvas.print("Password web");
    canvas.setTextColor(COLOR_GRAY);
    canvas.setCursor(102, 82); canvas.print("admin123 >");

    // 5. Card: Reset Sistem (y = 96..112, h = 16) - Row 3
    canvas.fillRoundRect(2, 96, 156, 16, 3, COLOR_CARD_BG);
    if (_subItemIndex == 3) canvas.drawRoundRect(2, 96, 156, 16, 3, _resetConfirmActive ? COLOR_ALERT_RED : COLOR_CYAN);
    else canvas.drawRoundRect(2, 96, 156, 16, 3, COLOR_CARD_BORDER);

    canvas.setTextColor(_resetConfirmActive ? COLOR_ALERT_RED : COLOR_WHITE);
    canvas.setCursor(6, 100); canvas.print("Reset Sistem");
    canvas.setTextColor(COLOR_ALERT_RED);
    canvas.setCursor(85, 100);
    canvas.print(_resetConfirmActive ? "KLIK LAGI >" : "Tahan 2 dtk >");

    renderFooterBar(_inEditMode ? "O:Ubah" : "O:Pilih", _inEditMode ? "SW:Simpan" : "SW:Edit", _inEditMode ? "K0:Batal" : "K0:Menu");
}

void TftDisplay::renderToast() {
    if (millis() < _toastEndTime && strlen(_toastMsg) > 0) {
        int16_t tw = 130;
        int16_t th = 20;
        int16_t tx = (160 - tw) / 2;
        int16_t ty = (128 - th) / 2;

        canvas.fillRoundRect(tx, ty, tw, th, 4, 0x0123);
        canvas.drawRoundRect(tx, ty, tw, th, 4, COLOR_CYAN);

        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(tx + 8, ty + 6);
        canvas.print(_toastMsg);
    }
}

// ============================================================================
// BOOT SCREEN FUTURISTIK PERSIS GAMBAR REFERENSI UI MENU LCD AUDIO DSP
// ============================================================================
void TftDisplay::drawBootScreen(uint8_t progress, const char* statusMsg) {
    if (!_isInitialized) return;
    if (progress > 100) progress = 100;

    canvas.fillScreen(COLOR_BG);

    // 1. Outer Border Futuristik dengan Chamfer Kanan-Atas & Aksen Garis
    // Garis bingkai luar (Cyan 2px)
    canvas.drawFastHLine(3, 3, 140, COLOR_CYAN);
    canvas.drawFastHLine(4, 4, 138, COLOR_CYAN);
    
    // Diagonal Chamfer di sudut kanan atas
    canvas.drawLine(143, 3, 156, 16, COLOR_CYAN);
    canvas.drawLine(142, 4, 155, 17, COLOR_CYAN);
    
    // Segitiga Chamfer Solid di sudut kanan atas (seperti badge sci-fi di gambar)
    canvas.fillTriangle(145, 3, 156, 3, 156, 14, COLOR_CYAN);

    // Garis kanan, bawah, kiri
    canvas.drawFastVLine(156, 15, 110, COLOR_CYAN);
    canvas.drawFastVLine(155, 16, 108, COLOR_CYAN);
    canvas.drawFastHLine(3, 124, 154, COLOR_CYAN);
    canvas.drawFastHLine(4, 123, 152, COLOR_CYAN);
    canvas.drawFastVLine(3, 3, 122, COLOR_CYAN);
    canvas.drawFastVLine(4, 4, 120, COLOR_CYAN);

    // 3 Garis miring /// di sudut kiri bawah
    for (int i = 0; i < 3; i++) {
        int xOffset = i * 4;
        canvas.drawLine(10 + xOffset, 120, 18 + xOffset, 112, COLOR_CYAN);
        canvas.drawLine(11 + xOffset, 120, 19 + xOffset, 112, COLOR_CYAN);
    }

    // 2. Header Bar: 3 Garis Miring + LOADING + Persentase
    // 3 strip miring di sebelah kiri tulisan LOADING
    for (int i = 0; i < 3; i++) {
        int sx = 18 + i * 5;
        canvas.drawLine(sx, 16, sx + 3, 10, COLOR_CYAN);
        canvas.drawLine(sx + 1, 16, sx + 4, 10, COLOR_CYAN);
    }

    // Tulisan LOADING (Cyan tebal)
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(36, 10);
    canvas.print("LOADING");

    // Persentase (Putih tebal di kanan)
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%u%%", progress);
    int16_t pctLen = strlen(pctBuf);
    int16_t pctX = 142 - (pctLen * 6);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(pctX, 10);
    canvas.print(pctBuf);
    canvas.setCursor(pctX + 1, 10);
    canvas.print(pctBuf);

    // 3. Futuristic Progress Bar (Parallelogram dengan Striped Hazard Pattern)
    int16_t barY = 20;
    int16_t barH = 7;
    int16_t barX1 = 19;
    int16_t barX2 = 141;

    // Bingkai miring (Parallelogram)
    canvas.drawLine(barX1, barY + barH, barX1 + 3, barY, COLOR_CYAN);
    canvas.drawLine(barX1 + 3, barY, barX2, barY, COLOR_CYAN);
    canvas.drawLine(barX2, barY, barX2 - 3, barY + barH, COLOR_CYAN);
    canvas.drawLine(barX2 - 3, barY + barH, barX1, barY + barH, COLOR_CYAN);

    // Latar belakang bar gelap
    for (int py = 1; py < barH; py++) {
        int sx = barX1 + 1 + (3 * (barH - py)) / barH;
        int ex = barX2 - 1 - (3 * py) / barH;
        if (ex > sx) canvas.drawFastHLine(sx, barY + py, ex - sx, 0x0010);
    }

    // Isi Bar berpola garis diagonal (Hazard Striped Cyan & Light Blue)
    int16_t totalInnerW = barX2 - barX1 - 5;
    int16_t fillW = (int16_t)((progress * totalInnerW) / 100);
    if (fillW > totalInnerW) fillW = totalInnerW;

    for (int px = 0; px < fillW; px++) {
        for (int py = 1; py < barH; py++) {
            int sx = barX1 + 2 + px + (3 * (barH - py)) / barH;
            if (sx < barX2 - 2) {
                uint16_t col = (((px + py) / 4) % 2 == 0) ? COLOR_CYAN : 0x57FF;
                canvas.drawPixel(sx, barY + py, col);
            }
        }
    }

    // 4. Center Graphic: Speaker Cabinet dengan Tweeter, Woofer & Gelombang Suara
    // Garis perspektif lantai / horizon maya di background
    canvas.drawFastHLine(10, 52, 140, 0x0925);
    canvas.drawLine(10, 52, 22, 75, 0x0925);
    canvas.drawLine(150, 52, 138, 75, 0x0925);

    // Titik partikel cyber di sekitar speaker
    canvas.fillCircle(18, 48, 2, COLOR_CYAN);
    canvas.fillCircle(14, 62, 1, COLOR_CYAN);
    canvas.fillCircle(27, 54, 1, COLOR_CYAN);
    canvas.fillCircle(142, 47, 2, COLOR_CYAN);
    canvas.fillCircle(150, 57, 1, COLOR_CYAN);
    canvas.fillCircle(137, 70, 2, COLOR_CYAN);
    canvas.fillCircle(128, 85, 1, COLOR_CYAN);
    canvas.fillCircle(28, 88, 1, COLOR_CYAN);

    // Kotak Speaker (Rounded Box)
    int16_t spkX = 66, spkY = 34, spkW = 28, spkH = 37;
    canvas.fillRoundRect(spkX, spkY, spkW, spkH, 4, 0x0821);
    canvas.drawRoundRect(spkX, spkY, spkW, spkH, 4, COLOR_CYAN);
    canvas.drawRoundRect(spkX + 1, spkY + 1, spkW - 2, spkH - 2, 3, COLOR_CYAN);

    // Tweeter Atas
    canvas.drawCircle(80, 43, 4, COLOR_CYAN);
    canvas.fillCircle(80, 43, 2, 0x03FF);
    canvas.drawPixel(80, 43, COLOR_WHITE);

    // Woofer Bawah (Lingkaran konsentris)
    canvas.drawCircle(80, 58, 9, COLOR_CYAN);
    canvas.drawCircle(80, 58, 6, 0x03FF);
    canvas.fillCircle(80, 58, 3, 0x0110);
    canvas.drawCircle(80, 58, 3, COLOR_CYAN);
    canvas.drawPixel(78, 56, COLOR_WHITE); // Highlight kilauan

    // Gelombang Suara Melengkung (Arcs di kiri & kanan)
    // Kiri Inner Arc
    canvas.drawLine(59, 48, 57, 54, COLOR_CYAN);
    canvas.drawLine(57, 54, 59, 60, COLOR_CYAN);
    canvas.drawLine(60, 48, 58, 54, COLOR_CYAN);
    canvas.drawLine(58, 54, 60, 60, COLOR_CYAN);

    // Kiri Outer Arc
    canvas.drawLine(54, 45, 51, 54, COLOR_CYAN);
    canvas.drawLine(51, 54, 54, 63, COLOR_CYAN);
    canvas.drawLine(55, 45, 52, 54, COLOR_CYAN);
    canvas.drawLine(52, 54, 55, 63, COLOR_CYAN);

    // Kanan Inner Arc
    canvas.drawLine(101, 48, 103, 54, COLOR_CYAN);
    canvas.drawLine(103, 54, 101, 60, COLOR_CYAN);
    canvas.drawLine(100, 48, 102, 54, COLOR_CYAN);
    canvas.drawLine(102, 54, 100, 60, COLOR_CYAN);

    // Kanan Outer Arc
    canvas.drawLine(106, 45, 109, 54, COLOR_CYAN);
    canvas.drawLine(109, 54, 106, 63, COLOR_CYAN);
    canvas.drawLine(105, 45, 108, 54, COLOR_CYAN);
    canvas.drawLine(108, 54, 105, 63, COLOR_CYAN);

    // 5. Typography Utama: SHAWIR DLMS (Besar, Bold, 3D Drop-Shadow)
    const char* titleText = "SHAWIR DLMS";
    canvas.setTextSize(2);
    // 3D Drop Shadow Biru Tua
    canvas.setTextColor(0x018F);
    canvas.setCursor(16, 79);
    canvas.print(titleText);
    canvas.setCursor(15, 78);
    canvas.print(titleText);
    // Teks Utama Electric Cyan
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(14, 77);
    canvas.print(titleText);

    // 6. Angled Banner Ribbon: SOFGAM SS
    int16_t rY = 96;
    int16_t rH = 13;
    int16_t rW = 106;

    // Drop shadow ribbon gelap di bawahnya
    for (int ly = 0; ly < 2; ly++) {
        canvas.drawFastHLine(26, rY + rH + ly, rW, 0x014B);
    }

    // Parallelogram Cyan Solid
    for (int ly = 0; ly <= rH; ly++) {
        int sx = 28 - (ly * 4) / rH;
        canvas.drawFastHLine(sx, rY + ly, rW, COLOR_CYAN);
    }

    // Teks SOFGAM SS di dalam banner (Warna Gelap, Bold)
    const char* subBannerText = "SOFGAM SS";
    int16_t subLen = strlen(subBannerText);
    int16_t subX = 80 - (subLen * 6) / 2;
    canvas.setTextSize(1);
    canvas.setTextColor(0x0821); // Gelap kontras
    canvas.setCursor(subX, 99);
    canvas.print(subBannerText);
    canvas.setCursor(subX + 1, 99);
    canvas.print(subBannerText);

    // 7. Status Console Teks di Baris Bawah
    if (statusMsg && strlen(statusMsg) > 0) {
        char statBuf[24];
        snprintf(statBuf, sizeof(statBuf), "> %s", statusMsg);
        canvas.setTextSize(1);
        canvas.setTextColor(COLOR_WHITE);
        canvas.setCursor(34, 114);
        canvas.print(statBuf);
    }

    // Push seluruh frame sprite ke LCD ST7735 secara instan (Flicker-Free)
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 160, 128);
}

void TftDisplay::update() {
    if (!_isInitialized) return;

    canvas.fillScreen(COLOR_BG);

    switch (_currentScreen) {
        case SCREEN_BOOT:        drawBootScreen(100, "Sistem siap!"); return;
        case SCREEN_HOME:        renderHomeScreen(); break;
        case SCREEN_MAIN_MENU:   renderMainMenu(); break;
        case SCREEN_MODE_SELECT: renderModeSelectScreen(); break;
        case SCREEN_ROUTING:     renderRoutingScreen(); break;
        case SCREEN_CROSSOVER:   renderCrossoverScreen(); break;
        case SCREEN_PEQ:         renderPeqScreen(); break;
        case SCREEN_LIMITER:     renderLimiterScreen(); break;
        case SCREEN_DELAY:       renderDelayScreen(); break;
        case SCREEN_POLARITY:    renderPolarityScreen(); break;
        case SCREEN_PRESET:      renderPresetScreen(); break;
        case SCREEN_SETUP:       renderSetupScreen(); break;
        default:                 renderHomeScreen(); break;
    }

    renderToast();
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 160, 128);
}

// ============================================================================
// LOGIKA NAVIGASI ROTARY ENCODER & K0
// ============================================================================
void TftDisplay::handleNavigation(int16_t delta, ButtonEvent swEv, ButtonEvent k0Ev, bool k0Held, bool swHeld) {
    if (delta != 0 || swEv != BTN_NONE || k0Ev != BTN_NONE || k0Held || swHeld) {
        resetUserActivityTimer();
    }

    // --- LOGIKA KUNCI / BUKA LAYAR VIA TOMBOL FISIK K0 ---
    // Di Home Screen: Tahan K0 3 detik untuk KUNCI layar
    // Saat Layar Terkunci: Tahan K0 4 detik untuk BUKA kunci
    if (k0Held) {
        if (_k0HoldStartTime == 0) {
            _k0HoldStartTime = millis();
            _k0HoldHandled = false;
            _lockProgressSec = 0;
        }

        uint32_t holdTime = millis() - _k0HoldStartTime;

        if (!_screenLocked) {
            // Penguncian diaktifkan dari layar HOME
            if (_currentScreen == SCREEN_HOME) {
                if (holdTime >= 1000 && holdTime < 2000 && _lockProgressSec != 1) {
                    _lockProgressSec = 1;
                    showToast("Kunci: Tahan 2s...", 600);
                } else if (holdTime >= 2000 && holdTime < 3000 && _lockProgressSec != 2) {
                    _lockProgressSec = 2;
                    showToast("Kunci: Tahan 1s...", 600);
                } else if (holdTime >= 3000 && !_k0HoldHandled) {
                    _screenLocked = true;
                    _k0HoldHandled = true;
                    _inEditMode = false;
                    showToast("Layar Terkunci!", 2000);
                }
            }
        } else {
            // Membuka kunci layar: butuh tahan K0 selama 4 detik penuh
            if (holdTime >= 1000 && holdTime < 2000 && _lockProgressSec != 1) {
                _lockProgressSec = 1;
                showToast("Buka: Tahan 3s...", 600);
            } else if (holdTime >= 2000 && holdTime < 3000 && _lockProgressSec != 2) {
                _lockProgressSec = 2;
                showToast("Buka: Tahan 2s...", 600);
            } else if (holdTime >= 3000 && holdTime < 4000 && _lockProgressSec != 3) {
                _lockProgressSec = 3;
                showToast("Buka: Tahan 1s...", 600);
            } else if (holdTime >= 4000 && !_k0HoldHandled) {
                _screenLocked = false;
                _k0HoldHandled = true;
                showToast("Kunci Terbuka!", 2000);
            }
        }
    } else {
        // Tombol K0 dilepas
        _k0HoldStartTime = 0;
        _lockProgressSec = 0;
        if (_k0HoldHandled) {
            // Buang sisa event klik atau long press K0 setelah aksi hold lock/unlock selesai
            k0Ev = BTN_NONE;
            _k0HoldHandled = false;
        }
    }

    // Jika layar terkunci, blokir seluruh input navigasi & setting!
    if (_screenLocked) {
        if (!_k0HoldHandled && (delta != 0 || swEv == BTN_CLICK || swEv == BTN_LONG_PRESS_1S || k0Ev == BTN_CLICK)) {
            showToast("Terkunci! Tahan K0 4s", 1200);
        }
        return;
    }

    // Jika di sub-menu (bukan Home), K0 long-press 1s kembali langsung ke Home
    if (k0Ev == BTN_LONG_PRESS_1S && _currentScreen != SCREEN_HOME) {
        setScreen(SCREEN_HOME);
        return;
    }

    const DspPreset& cfg = dspEngine.getConfig();

    switch (_currentScreen) {
        case SCREEN_HOME: {
            if (_inEditMode) {
                float step = k0Held ? 0.1f : 0.5f;
                if (_homeSelectedCh < 2) {
                    float new_gain = cfg.in[_homeSelectedCh].gain_db + delta * step;
                    dspEngine.setInputGain(_homeSelectedCh, new_gain);
                } else {
                    int out_idx = _homeSelectedCh - 2;
                    float new_gain = cfg.out[out_idx].gain_db + delta * step;
                    dspEngine.setOutputGain(out_idx, new_gain);
                }
                storageManager.markConfigChanged();

                if (swEv == BTN_CLICK || k0Ev == BTN_CLICK) {
                    _inEditMode = false;
                }
            } else {
                if (delta != 0) {
                    _homeSelectedCh += delta;
                    if (_homeSelectedCh < 0) _homeSelectedCh = 0;
                    if (_homeSelectedCh > 5) _homeSelectedCh = 5;
                }

                if (swEv == BTN_CLICK) {
                    _inEditMode = true;
                    _originalFloatVal = (_homeSelectedCh < 2) ? cfg.in[_homeSelectedCh].gain_db : cfg.out[_homeSelectedCh - 2].gain_db;
                } else if (swEv == BTN_LONG_PRESS_1S) {
                    // PUSH LAMA = MUTE KANAL YANG SEDANG DIPILIH!
                    if (_homeSelectedCh < 2) {
                        bool nextMute = !cfg.in[_homeSelectedCh].mute;
                        dspEngine.setInputMute(_homeSelectedCh, nextMute);
                        showToast(nextMute ? "In A/B MUTED" : "In A/B UNMUTED", 1200);
                    } else {
                        int out_idx = _homeSelectedCh - 2;
                        bool nextMute = !cfg.out[out_idx].mute;
                        dspEngine.setOutputMute(out_idx, nextMute);
                        showToast(nextMute ? "Out MUTED" : "Out UNMUTED", 1200);
                    }
                    storageManager.markConfigChanged();
                } else if (k0Ev == BTN_CLICK) {
                    setScreen(SCREEN_MAIN_MENU);
                }
            }
            break;
        }

        case SCREEN_MAIN_MENU: {
            if (delta != 0) {
                _menuIndex += delta;
                if (_menuIndex < 0) _menuIndex = 0;
                if (_menuIndex > 8) _menuIndex = 8;
            }

            if (swEv == BTN_CLICK) {
                switch (_menuIndex) {
                    case 0: setScreen(SCREEN_MODE_SELECT); break;
                    case 1: setScreen(SCREEN_ROUTING); break;
                    case 2: setScreen(SCREEN_CROSSOVER); break;
                    case 3: setScreen(SCREEN_PEQ); break;
                    case 4: setScreen(SCREEN_LIMITER); break;
                    case 5: setScreen(SCREEN_DELAY); break;
                    case 6: setScreen(SCREEN_POLARITY); break;
                    case 7: setScreen(SCREEN_PRESET); break;
                    case 8: setScreen(SCREEN_SETUP); break;
                }
            } else if (k0Ev == BTN_CLICK) {
                setScreen(SCREEN_HOME);
            }
            break;
        }

        case SCREEN_MODE_SELECT: {
            if (delta != 0) {
                _modeOptionSel += delta;
                if (_modeOptionSel < 0) _modeOptionSel = 0;
                if (_modeOptionSel > 1) _modeOptionSel = 1;
            }

            if (swEv == BTN_CLICK) {
                static DspPreset newCfg;
                newCfg = cfg;
                newCfg.setup.out_mode = (uint8_t)_modeOptionSel;
                dspEngine.setConfig(newCfg);
                storageManager.markConfigChanged();
                if (_modeOptionSel == 0) {
                    showToast("Mode 1: Stereo Pair Aktif!");
                } else {
                    showToast("Mode 2: 4-Way Indiv Aktif!");
                }
            } else if (k0Ev == BTN_CLICK) {
                setScreen(SCREEN_MAIN_MENU);
            }
            break;
        }

        case SCREEN_ROUTING: {
            if (_inEditMode) {
                if (_routingRow >= 2) {
                    int out_idx = _routingRow - 2;
                    int src = (int)cfg.out[out_idx].source + delta;
                    if (src < 0) src = 0; if (src > 3) src = 3;
                    dspEngine.setOutputSource(out_idx, (uint8_t)src);
                    storageManager.markConfigChanged();
                }
                if (swEv == BTN_CLICK) {
                    _inEditMode = false;
                } else if (k0Ev == BTN_CLICK) {
                    _inEditMode = false;
                }
            } else {
                if (delta != 0) {
                    _routingRow += delta;
                    if (_routingRow < 0) _routingRow = 0;
                    if (_routingRow > 5) _routingRow = 5;
                }
                if (swEv == BTN_CLICK) {
                    if (_routingRow >= 2) _inEditMode = true;
                } else if (k0Ev == BTN_CLICK) {
                    setScreen(SCREEN_MAIN_MENU);
                }
            }
            break;
        }

        case SCREEN_CROSSOVER: {
            bool isPairMode = (cfg.setup.out_mode == 0);
            uint8_t ch = isPairMode ? (_xoverCh < 2 ? 0 : 2) : _xoverCh;
            if (ch > 3) ch = 3;
            const uint8_t slopeVals[6] = {6, 12, 18, 24, 36, 48};

            if (_inEditMode) {
                if (delta != 0) {
                    if (_xoverEditField == 0) {
                        // Channel tab
                        if (isPairMode) {
                            _xoverCh = (_xoverCh < 2) ? 2 : 0;
                        } else {
                            _xoverCh += delta;
                            if (_xoverCh < 0) _xoverCh = 0;
                            if (_xoverCh > 3) _xoverCh = 3;
                        }
                    } else if (_xoverEditField == 1) {
                        // HPF On/Off
                        bool newEn = (delta > 0);
                        dspEngine.setOutputHpf(ch, newEn, cfg.out[ch].hpf.freq, cfg.out[ch].hpf.type, cfg.out[ch].hpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 2) {
                        // HPF Freq
                        float step = (cfg.out[ch].hpf.freq >= 1000.0f) ? (k0Held ? 10.0f : 100.0f) : (k0Held ? 1.0f : 10.0f);
                        float f = cfg.out[ch].hpf.freq + delta * step;
                        if (f < 20.0f) f = 20.0f; if (f > 20000.0f) f = 20000.0f;
                        dspEngine.setOutputHpf(ch, cfg.out[ch].hpf.enabled, f, cfg.out[ch].hpf.type, cfg.out[ch].hpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 3) {
                        // HPF Type (0: Butt, 1: Link-R, 2: Bessel)
                        int t = (int)cfg.out[ch].hpf.type + delta;
                        if (t < 0) t = 0; if (t > 2) t = 2;
                        dspEngine.setOutputHpf(ch, cfg.out[ch].hpf.enabled, cfg.out[ch].hpf.freq, (uint8_t)t, cfg.out[ch].hpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 4) {
                        // HPF Slope (6, 12, 18, 24, 36, 48)
                        int sIdx = 0;
                        for (int si = 0; si < 6; si++) {
                            if (slopeVals[si] == cfg.out[ch].hpf.slope) { sIdx = si; break; }
                        }
                        sIdx += delta;
                        if (sIdx < 0) sIdx = 0;
                        if (sIdx > 5) sIdx = 5;
                        dspEngine.setOutputHpf(ch, cfg.out[ch].hpf.enabled, cfg.out[ch].hpf.freq, cfg.out[ch].hpf.type, slopeVals[sIdx]);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 5) {
                        // LPF On/Off
                        bool newEn = (delta > 0);
                        dspEngine.setOutputLpf(ch, newEn, cfg.out[ch].lpf.freq, cfg.out[ch].lpf.type, cfg.out[ch].lpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 6) {
                        // LPF Freq
                        float step = (cfg.out[ch].lpf.freq >= 1000.0f) ? (k0Held ? 10.0f : 100.0f) : (k0Held ? 1.0f : 10.0f);
                        float f = cfg.out[ch].lpf.freq + delta * step;
                        if (f < 20.0f) f = 20.0f; if (f > 20000.0f) f = 20000.0f;
                        dspEngine.setOutputLpf(ch, cfg.out[ch].lpf.enabled, f, cfg.out[ch].lpf.type, cfg.out[ch].lpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 7) {
                        // LPF Type (0: Butt, 1: Link-R, 2: Bessel)
                        int t = (int)cfg.out[ch].lpf.type + delta;
                        if (t < 0) t = 0; if (t > 2) t = 2;
                        dspEngine.setOutputLpf(ch, cfg.out[ch].lpf.enabled, cfg.out[ch].lpf.freq, (uint8_t)t, cfg.out[ch].lpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 8) {
                        // LPF Slope (6, 12, 18, 24, 36, 48)
                        int sIdx = 0;
                        for (int si = 0; si < 6; si++) {
                            if (slopeVals[si] == cfg.out[ch].lpf.slope) { sIdx = si; break; }
                        }
                        sIdx += delta;
                        if (sIdx < 0) sIdx = 0;
                        if (sIdx > 5) sIdx = 5;
                        dspEngine.setOutputLpf(ch, cfg.out[ch].lpf.enabled, cfg.out[ch].lpf.freq, cfg.out[ch].lpf.type, slopeVals[sIdx]);
                        storageManager.markConfigChanged();
                    }
                }

                if (swEv == BTN_CLICK) {
                    _inEditMode = false;
                    storageManager.markConfigChanged();
                } else if (k0Ev == BTN_CLICK) {
                    _inEditMode = false;
                    storageManager.markConfigChanged();
                }
            } else {
                if (delta != 0) {
                    _xoverEditField += delta;
                    if (_xoverEditField < 0) _xoverEditField = 0;
                    if (_xoverEditField > 8) _xoverEditField = 8;
                }

                if (swEv == BTN_CLICK) {
                    if (_xoverEditField == 1) {
                        // Langsung toggle On/Off HPF
                        bool newEn = !cfg.out[ch].hpf.enabled;
                        dspEngine.setOutputHpf(ch, newEn, cfg.out[ch].hpf.freq, cfg.out[ch].hpf.type, cfg.out[ch].hpf.slope);
                        storageManager.markConfigChanged();
                    } else if (_xoverEditField == 5) {
                        // Langsung toggle On/Off LPF
                        bool newEn = !cfg.out[ch].lpf.enabled;
                        dspEngine.setOutputLpf(ch, newEn, cfg.out[ch].lpf.freq, cfg.out[ch].lpf.type, cfg.out[ch].lpf.slope);
                        storageManager.markConfigChanged();
                    } else {
                        _inEditMode = true;
                    }
                } else if (k0Ev == BTN_CLICK) {
                    storageManager.markConfigChanged();
                    setScreen(SCREEN_MAIN_MENU);
                }
            }
            break;
        }

        case SCREEN_PEQ: {
            bool isPairMode = (cfg.setup.out_mode == 0);
            int maxTargets = isPairMode ? 4 : 6;
            if (_peqTarget < 0) _peqTarget = 0;
            if (_peqTarget >= maxTargets) _peqTarget = maxTargets - 1;

            bool isInput = (_peqTarget < 2);
            uint8_t in_ch = _peqTarget;
            uint8_t out_ch = isPairMode ? ((_peqTarget - 2) * 2) : (_peqTarget - 2);

            int numBands = isInput ? 4 : 8;
            if (_peqBand < 0) _peqBand = 0;
            if (_peqBand >= numBands) _peqBand = numBands - 1;
            uint8_t b = _peqBand;

            PeqBandConfig p = isInput ? cfg.in[in_ch].peq[b] : cfg.out[out_ch].peq[b];

            if (_inEditMode) {
                if (delta != 0) {
                    if (_peqEditField == 0) {
                        // Ubah Channel Target
                        _peqTarget += delta;
                        if (_peqTarget < 0) _peqTarget = 0;
                        if (_peqTarget >= maxTargets) _peqTarget = maxTargets - 1;
                        int maxB = (_peqTarget < 2) ? 4 : 8;
                        if (_peqBand >= maxB) _peqBand = maxB - 1;
                    } else if (_peqEditField == 1) {
                        // Ubah Band
                        _peqBand += delta;
                        if (_peqBand < 0) _peqBand = 0;
                        if (_peqBand >= numBands) _peqBand = numBands - 1;
                    } else if (_peqEditField == 2) {
                        // Freq
                        float step = (p.freq >= 1000.0f) ? (k0Held ? 10.0f : 100.0f) : (k0Held ? 1.0f : 10.0f);
                        p.freq += delta * step;
                        if (p.freq < 20.0f) p.freq = 20.0f; if (p.freq > 20000.0f) p.freq = 20000.0f;
                        p.enabled = true;
                        if (isInput) dspEngine.setInputPeq(in_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        else dspEngine.setOutputPeq(out_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        storageManager.markConfigChanged();
                    } else if (_peqEditField == 3) {
                        // Gain
                        p.gain_db += delta * (k0Held ? 0.1f : 0.5f);
                        if (p.gain_db < -15.0f) p.gain_db = -15.0f; if (p.gain_db > 15.0f) p.gain_db = 15.0f;
                        p.enabled = true;
                        if (isInput) dspEngine.setInputPeq(in_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        else dspEngine.setOutputPeq(out_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        storageManager.markConfigChanged();
                    } else if (_peqEditField == 4) {
                        // Q
                        p.q += delta * (k0Held ? 0.05f : 0.1f);
                        if (p.q < 0.1f) p.q = 0.1f; if (p.q > 20.0f) p.q = 20.0f;
                        p.enabled = true;
                        if (isInput) dspEngine.setInputPeq(in_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        else dspEngine.setOutputPeq(out_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        storageManager.markConfigChanged();
                    } else if (_peqEditField == 5) {
                        // Type
                        int t_new = (int)p.type + delta;
                        if (t_new < 0) t_new = 0; if (t_new > 3) t_new = 3;
                        p.type = (uint8_t)t_new;
                        p.enabled = true;
                        if (isInput) dspEngine.setInputPeq(in_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        else dspEngine.setOutputPeq(out_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        storageManager.markConfigChanged();
                    } else if (_peqEditField == 6) {
                        // Bypass
                        p.bypass = (delta > 0);
                        if (isInput) dspEngine.setInputPeq(in_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        else dspEngine.setOutputPeq(out_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, p.bypass);
                        storageManager.markConfigChanged();
                    }
                }

                if (swEv == BTN_CLICK) {
                    _inEditMode = false;
                    storageManager.markConfigChanged();
                } else if (k0Ev == BTN_CLICK) {
                    _inEditMode = false;
                    storageManager.markConfigChanged();
                }
            } else {
                if (delta != 0) {
                    _peqEditField += delta;
                    if (_peqEditField < 0) _peqEditField = 0;
                    if (_peqEditField > 6) _peqEditField = 6;
                }

                if (swEv == BTN_CLICK) {
                    if (_peqEditField == 6) {
                        // Langsung toggle Bypass
                        bool newBypass = !p.bypass;
                        if (isInput) dspEngine.setInputPeq(in_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, newBypass);
                        else dspEngine.setOutputPeq(out_ch, b, p.enabled, p.type, p.freq, p.gain_db, p.q, newBypass);
                        storageManager.markConfigChanged();
                    } else {
                        _inEditMode = true;
                    }
                } else if (k0Ev == BTN_CLICK) {
                    storageManager.markConfigChanged();
                    setScreen(SCREEN_MAIN_MENU);
                }
            }
            break;
        }

        case SCREEN_LIMITER: {
            if (delta != 0) {
                int ch = _subItemIndex;
                float th = cfg.out[ch].limiter.threshold_db + delta * (k0Held ? 0.1f : 0.5f);
                if (th < -40.0f) th = -40.0f; if (th > 0.0f) th = 0.0f;
                dspEngine.setOutputLimiter(ch, true, th, cfg.out[ch].limiter.attack_ms, cfg.out[ch].limiter.release_ms);
                storageManager.markConfigChanged();
            }
            if (swEv == BTN_CLICK) {
                _subItemIndex = (_subItemIndex + 1) % 4;
            } else if (k0Ev == BTN_CLICK) {
                storageManager.markConfigChanged();
                setScreen(SCREEN_MAIN_MENU);
            }
            break;
        }

        case SCREEN_DELAY: {
            if (delta != 0) {
                int ch = _subItemIndex;
                float d = cfg.out[ch].delay.delay_ms + delta * (k0Held ? 0.05f : 0.5f);
                if (d < 0.0f) d = 0.0f; if (d > 100.0f) d = 100.0f;
                dspEngine.setOutputDelay(ch, d, cfg.out[ch].delay.unit, cfg.out[ch].delay.temp_c);
                storageManager.markConfigChanged();
            }
            if (swEv == BTN_CLICK) {
                _subItemIndex = (_subItemIndex + 1) % 4;
            } else if (k0Ev == BTN_CLICK) {
                storageManager.markConfigChanged();
                setScreen(SCREEN_MAIN_MENU);
            }
            break;
        }

        case SCREEN_POLARITY: {
            if (delta != 0) {
                _subItemIndex = (_subItemIndex + delta) % 4;
                if (_subItemIndex < 0) _subItemIndex = 3;
            }
            if (swEv == BTN_CLICK) {
                int ch = _subItemIndex;
                dspEngine.setOutputPolarity(ch, !cfg.out[ch].polarity_invert);
                storageManager.markConfigChanged();
            } else if (k0Ev == BTN_CLICK) {
                storageManager.markConfigChanged();
                setScreen(SCREEN_MAIN_MENU);
            }
            break;
        }

        case SCREEN_PRESET: {
            if (_presetActionActive) {
                if (delta != 0) {
                    _presetActionChoice += delta;
                    if (_presetActionChoice < 0) _presetActionChoice = 0;
                    if (_presetActionChoice > 2) _presetActionChoice = 2;
                }
                if (swEv == BTN_CLICK) {
                    if (_presetActionChoice == 0) {
                        // 1. Opsi LOAD Preset ke DSP
                        static DspPreset loaded;
                        dspEngine.setMasterMute(true);
                        delay(25);
                        bool ok = storageManager.loadPreset(_presetSlotSel, loaded);
                        if (ok) {
                            dspEngine.setConfig(loaded);
                            storageManager.setCurrentSlot(_presetSlotSel);
                        }
                        delay(25);
                        dspEngine.setMasterMute(false);
                        if (ok) {
                            showToast("Preset Dimuat!");
                        } else {
                            showToast("Gagal Memuat!");
                        }
                        _presetActionActive = false;
                    } else if (_presetActionChoice == 1) {
                        // 2. Opsi SIMPAN DSP Aktif ke Slot Pilihan
                        dspEngine.setMasterMute(true);
                        delay(25);
                        bool ok = storageManager.savePreset(_presetSlotSel, dspEngine.getConfig());
                        delay(25);
                        dspEngine.setMasterMute(false);
                        if (ok) {
                            storageManager.setCurrentSlot(_presetSlotSel);
                            showToast("Tersimpan ke Slot!");
                        } else {
                            showToast("Gagal Menyimpan!");
                        }
                        _presetActionActive = false;
                    } else {
                        // 3. Opsi BATAL
                        _presetActionActive = false;
                    }
                } else if (k0Ev == BTN_CLICK) {
                    _presetActionActive = false;
                }
            } else {
                if (delta != 0) {
                    _presetSlotSel += delta;
                    if (_presetSlotSel < 1) _presetSlotSel = 1;
                    if (_presetSlotSel > 10) _presetSlotSel = 10;
                }
                if (swEv == BTN_CLICK) {
                    _presetActionActive = true;
                    _presetActionChoice = 0; // Default ke LOAD
                } else if (k0Ev == BTN_CLICK) {
                    setScreen(SCREEN_MAIN_MENU);
                }
            }
            break;
        }

        case SCREEN_SETUP: {
            if (_inEditMode) {
                if (delta != 0) {
                    if (_subItemIndex == 0) {
                        // Ubah Mode WiFi (0: Off, 1: AP, 2: Station) — HANYA ubah di memori saat scrolling!
                        static DspPreset newCfg;
                        newCfg = cfg;
                        int m = (int)newCfg.setup.wifi_mode + delta;
                        if (m < 0) m = 0; if (m > 2) m = 2;
                        newCfg.setup.wifi_mode = (uint8_t)m;
                        dspEngine.setConfig(newCfg);
                        storageManager.markConfigChanged();
                    } else if (_subItemIndex == 1) {
                        // Ubah Akses Web (0: Penuh, 1: Read-Only)
                        static DspPreset newCfg;
                        newCfg = cfg;
                        newCfg.setup.web_access = (delta > 0);
                        dspEngine.setConfig(newCfg);
                        storageManager.markConfigChanged();
                    }
                }
                if (swEv == BTN_CLICK) {
                    _inEditMode = false;
                    // Terapkan mode WiFi hanya saat user menekan konfirmasi!
                    if (_subItemIndex == 0) {
                        webServerDsp.setWifiMode(cfg.setup.wifi_mode);
                    }
                    storageManager.markConfigChanged();
                    showToast("Pengaturan Disimpan!");
                } else if (k0Ev == BTN_CLICK) {
                    _inEditMode = false;
                }
            } else {
                if (delta != 0) {
                    _subItemIndex += delta;
                    if (_subItemIndex < 0) _subItemIndex = 0;
                    if (_subItemIndex > 3) _subItemIndex = 3;
                    _resetConfirmActive = false;
                }

                if (swEv == BTN_CLICK) {
                    if (_subItemIndex == 0 || _subItemIndex == 1) {
                        _inEditMode = true;
                    } else if (_subItemIndex == 2) {
                        showToast("Pass: admin123", 2000);
                    } else if (_subItemIndex == 3) {
                        if (!_resetConfirmActive) {
                            _resetConfirmActive = true;
                            _resetConfirmTime = millis();
                            showToast("Klik lagi / Tahan utk Reset!", 2500);
                        } else {
                            // Eksekusi Reset Sistem & DSP
                            _resetConfirmActive = false;
                            storageManager.factoryReset();
                            dspEngine.setConfig(StorageManager::getFactoryPreset());
                            webServerDsp.setWifiMode(DSP_WIFI_AP);
                            showToast("Factory Reset Selesai!");
                        }
                    }
                } else if (swEv == BTN_LONG_PRESS_1S && _subItemIndex == 3) {
                    // Eksekusi Reset via Tahan Tombol 1-2 detik
                    _resetConfirmActive = false;
                    storageManager.factoryReset();
                    dspEngine.setConfig(StorageManager::getFactoryPreset());
                    webServerDsp.setWifiMode(DSP_WIFI_AP);
                    showToast("Factory Reset Selesai!");
                } else if (k0Ev == BTN_CLICK) {
                    _resetConfirmActive = false;
                    setScreen(SCREEN_MAIN_MENU);
                }
            }
            break;
        }

        default:
            break;
    }
}

