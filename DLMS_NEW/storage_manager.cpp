#include "storage_manager.h"
#include <LittleFS.h>
#include "dsp_engine.h"
#include "i2s_duplex.h"

volatile bool g_dspStorageBusy = false;
StorageManager storageManager;
static DspPreset s_factoryPreset;
static bool s_factoryInit = false;
static DspPreset s_saveBuffer;
static DspPreset s_tempPreset;

StorageManager::StorageManager()
    : _currentSlot(1),
      _isDirty(false),
      _lastChangeTime(0)
{
    strncpy(_slotNames[0], "Factory", sizeof(_slotNames[0]));
    for (uint8_t s = 1; s < TOTAL_SLOTS; s++) {
        snprintf(_slotNames[s], sizeof(_slotNames[s]), "Preset %u", s);
    }
}

static inline void getPresetPath(uint8_t slot, char* buf, size_t bufSize) {
    if (slot == StorageManager::FACTORY_SLOT) {
        snprintf(buf, bufSize, "/preset_0.bin");
    } else {
        snprintf(buf, bufSize, "/preset_%u.bin", slot);
    }
}

uint32_t StorageManager::calculateCrc32(const DspPreset& preset) {
    const uint8_t* data = (const uint8_t*)&preset;
    size_t length = sizeof(DspPreset) - sizeof(uint32_t);
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

const DspPreset& StorageManager::getFactoryPreset() {
    if (!s_factoryInit) {
        memset(&s_factoryPreset, 0, sizeof(s_factoryPreset));
        s_factoryPreset.magic = 0x444C4D53;
        s_factoryPreset.version = 1;
        strncpy(s_factoryPreset.name, "Factory", sizeof(s_factoryPreset.name));
        s_factoryPreset.master_mute = false;

        // Input A & B default
        for (int i = 0; i < 2; i++) {
            s_factoryPreset.in[i].gain_db = 0.0f;
            s_factoryPreset.in[i].mute = false;
            for (int b = 0; b < 4; b++) {
                s_factoryPreset.in[i].peq[b].enabled = false;
                s_factoryPreset.in[i].peq[b].type = PEQ_PEAKING;
                s_factoryPreset.in[i].peq[b].freq = (b == 0) ? 100.0f : ((b == 1) ? 500.0f : ((b == 2) ? 2000.0f : 8000.0f));
                s_factoryPreset.in[i].peq[b].gain_db = 0.0f;
                s_factoryPreset.in[i].peq[b].q = 1.0f;
                s_factoryPreset.in[i].peq[b].bypass = false;
            }
        }

        // Output 1..4 (OUT 1, OUT 2, OUT 3, OUT 4)
        const char* names[4] = {"OUT 1", "OUT 2", "OUT 3", "OUT 4"};
        uint8_t srcs[4] = {ROUTING_IN_A, ROUTING_IN_B, ROUTING_IN_A, ROUTING_IN_B};

        for (int i = 0; i < 4; i++) {
            strncpy(s_factoryPreset.out[i].name, names[i], sizeof(s_factoryPreset.out[i].name));
            s_factoryPreset.out[i].source = srcs[i];
            s_factoryPreset.out[i].gain_db = 0.0f;
            s_factoryPreset.out[i].mute = false;
            s_factoryPreset.out[i].polarity_invert = false;

            s_factoryPreset.out[i].hpf.enabled = false;
            s_factoryPreset.out[i].hpf.freq = 40.0f;
            s_factoryPreset.out[i].hpf.type = FILTER_LINKWITZ_RILEY;
            s_factoryPreset.out[i].hpf.slope = SLOPE_24DB;

            s_factoryPreset.out[i].lpf.enabled = false;
            s_factoryPreset.out[i].lpf.freq = 18000.0f;
            s_factoryPreset.out[i].lpf.type = FILTER_LINKWITZ_RILEY;
            s_factoryPreset.out[i].lpf.slope = SLOPE_24DB;

            float peq_f[8] = {63.0f, 160.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 8000.0f, 12500.0f};
            for (int b = 0; b < 8; b++) {
                s_factoryPreset.out[i].peq[b].enabled = false;
                s_factoryPreset.out[i].peq[b].type = PEQ_PEAKING;
                s_factoryPreset.out[i].peq[b].freq = peq_f[b];
                s_factoryPreset.out[i].peq[b].gain_db = 0.0f;
                s_factoryPreset.out[i].peq[b].q = 1.0f;
                s_factoryPreset.out[i].peq[b].bypass = false;
            }

            s_factoryPreset.out[i].delay.delay_ms = 0.0f;
            s_factoryPreset.out[i].delay.unit = DELAY_UNIT_MS;
            s_factoryPreset.out[i].delay.temp_c = 20.0f;

            s_factoryPreset.out[i].limiter.enabled = true;
            s_factoryPreset.out[i].limiter.threshold_db = -0.5f;
            s_factoryPreset.out[i].limiter.attack_ms = 1.0f;
            s_factoryPreset.out[i].limiter.release_ms = 200.0f;
        }

        s_factoryPreset.setup.brightness_pct = 90;
        s_factoryPreset.setup.auto_dim_sec = 60;
        s_factoryPreset.setup.theme = THEME_DARK;
        s_factoryPreset.setup.orientation = 1;
        s_factoryPreset.setup.encoder_reversed = false;
        s_factoryPreset.setup.encoder_accel = true;
        s_factoryPreset.setup.hold_time_ms = 1000;
        s_factoryPreset.setup.auto_lock_screen = false;
        s_factoryPreset.setup.mute_on_boot = false;
        s_factoryPreset.setup.wifi_mode = DSP_WIFI_AP;
        s_factoryPreset.setup.web_access = WEB_ACCESS_FULL;
        s_factoryPreset.setup.out_mode = 0; // Mode 1 Default: Stereo Pair (Out 1-2 & Out 3-4 Linked)
        strncpy(s_factoryPreset.setup.wifi_ssid, "DLMS-DSP-SHAWIR", sizeof(s_factoryPreset.setup.wifi_ssid));
        strncpy(s_factoryPreset.setup.wifi_pass, "0987654321", sizeof(s_factoryPreset.setup.wifi_pass));
        strncpy(s_factoryPreset.setup.web_password, "admin123", sizeof(s_factoryPreset.setup.web_password));

        s_factoryPreset.crc32 = calculateCrc32(s_factoryPreset);
        s_factoryInit = true;
    }
    return s_factoryPreset;
}

bool StorageManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[Storage] LittleFS Mount Gagal!");
    } else {
        Serial.println("[Storage] LittleFS Flash Storage Aktif");
    }

    _currentSlot = 1;
    if (LittleFS.exists("/active_slot.txt")) {
        File fSlot = LittleFS.open("/active_slot.txt", "r");
        if (fSlot) {
            int s = fSlot.parseInt();
            if (s >= 1 && s <= MAX_USER_PRESETS) {
                _currentSlot = (uint8_t)s;
            }
            fSlot.close();
        }
    }

    // Pastikan slot 0 (Factory) selalu tersimpan jika belum ada
    if (!loadPreset(FACTORY_SLOT, s_tempPreset)) {
        savePreset(FACTORY_SLOT, getFactoryPreset());
    }

    // Pastikan slot aktif pengguna tersimpan jika belum ada (agar tidak kereset ke default)
    if (!loadPreset(_currentSlot, s_tempPreset)) {
        s_tempPreset = getFactoryPreset();
        snprintf(s_tempPreset.name, sizeof(s_tempPreset.name), "Preset %u", _currentSlot);
        savePreset(_currentSlot, s_tempPreset);
    }

    // Muat semua nama preset yang ada di Flash ke cache RAM (hanya 1x saat booting!)
    for (uint8_t s = 0; s <= MAX_USER_PRESETS; s++) {
        char path[32];
        getPresetPath(s, path, sizeof(path));
        if (LittleFS.exists(path)) {
            File f = LittleFS.open(path, "r");
            if (f) {
                uint32_t magic = 0;
                uint16_t ver = 0;
                char nameBuf[16];
                memset(nameBuf, 0, sizeof(nameBuf));
                if (f.read((uint8_t*)&magic, sizeof(magic)) == sizeof(magic) &&
                    f.read((uint8_t*)&ver, sizeof(ver)) == sizeof(ver) &&
                    f.read((uint8_t*)nameBuf, sizeof(nameBuf)) == sizeof(nameBuf)) {
                    if (magic == 0x444C4D53 && nameBuf[0] != '\0') {
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        strncpy(_slotNames[s], nameBuf, sizeof(_slotNames[s]) - 1);
                        _slotNames[s][sizeof(_slotNames[s]) - 1] = '\0';
                    }
                }
                f.close();
            }
        }
    }

    return true;
}

void StorageManager::sanitizePreset(DspPreset& preset) {
    if (preset.magic != 0x444C4D53) {
        preset = getFactoryPreset();
        return;
    }
    preset.version = 1;
    preset.name[sizeof(preset.name) - 1] = '\0';
    if (strlen(preset.name) == 0) {
        strncpy(preset.name, "Preset", sizeof(preset.name));
    }

    // Input sanitize
    for (int i = 0; i < 2; i++) {
        if (isnan(preset.in[i].gain_db) || isinf(preset.in[i].gain_db)) preset.in[i].gain_db = 0.0f;
        if (preset.in[i].gain_db < -60.0f) preset.in[i].gain_db = -60.0f;
        if (preset.in[i].gain_db > 12.0f) preset.in[i].gain_db = 12.0f;

        for (int b = 0; b < 4; b++) {
            PeqBandConfig& peq = preset.in[i].peq[b];
            if (peq.type > 3) peq.type = PEQ_PEAKING;
            if (isnan(peq.freq) || isinf(peq.freq) || peq.freq < 20.0f) peq.freq = 100.0f * (float)(b + 1);
            if (peq.freq > 20000.0f) peq.freq = 20000.0f;
            if (isnan(peq.gain_db) || isinf(peq.gain_db)) peq.gain_db = 0.0f;
            if (peq.gain_db < -15.0f) peq.gain_db = -15.0f;
            if (peq.gain_db > 15.0f) peq.gain_db = 15.0f;
            if (isnan(peq.q) || isinf(peq.q) || peq.q < 0.1f) peq.q = 1.0f;
            if (peq.q > 20.0f) peq.q = 20.0f;
        }
    }

    // Output sanitize
    for (int i = 0; i < 4; i++) {
        preset.out[i].name[sizeof(preset.out[i].name) - 1] = '\0';
        if (preset.out[i].source > 3) preset.out[i].source = (i % 2);
        if (isnan(preset.out[i].gain_db) || isinf(preset.out[i].gain_db)) preset.out[i].gain_db = 0.0f;
        if (preset.out[i].gain_db < -60.0f) preset.out[i].gain_db = -60.0f;
        if (preset.out[i].gain_db > 12.0f) preset.out[i].gain_db = 12.0f;

        // HPF
        if (preset.out[i].hpf.type > 2) preset.out[i].hpf.type = FILTER_LINKWITZ_RILEY;
        uint8_t hs = preset.out[i].hpf.slope;
        if (hs != 6 && hs != 12 && hs != 18 && hs != 24 && hs != 36 && hs != 48) preset.out[i].hpf.slope = 24;
        if (isnan(preset.out[i].hpf.freq) || isinf(preset.out[i].hpf.freq) || preset.out[i].hpf.freq < 20.0f) preset.out[i].hpf.freq = 40.0f;
        if (preset.out[i].hpf.freq > 20000.0f) preset.out[i].hpf.freq = 20000.0f;

        // LPF
        if (preset.out[i].lpf.type > 2) preset.out[i].lpf.type = FILTER_LINKWITZ_RILEY;
        uint8_t ls = preset.out[i].lpf.slope;
        if (ls != 6 && ls != 12 && ls != 18 && ls != 24 && ls != 36 && ls != 48) preset.out[i].lpf.slope = 24;
        if (isnan(preset.out[i].lpf.freq) || isinf(preset.out[i].lpf.freq) || preset.out[i].lpf.freq < 20.0f) preset.out[i].lpf.freq = 18000.0f;
        if (preset.out[i].lpf.freq > 20000.0f) preset.out[i].lpf.freq = 20000.0f;

        // PEQ
        for (int b = 0; b < 8; b++) {
            PeqBandConfig& peq = preset.out[i].peq[b];
            if (peq.type > 3) peq.type = PEQ_PEAKING;
            if (isnan(peq.freq) || isinf(peq.freq) || peq.freq < 20.0f) peq.freq = 1000.0f;
            if (peq.freq > 20000.0f) peq.freq = 20000.0f;
            if (isnan(peq.gain_db) || isinf(peq.gain_db)) peq.gain_db = 0.0f;
            if (peq.gain_db < -15.0f) peq.gain_db = -15.0f;
            if (peq.gain_db > 15.0f) peq.gain_db = 15.0f;
            if (isnan(peq.q) || isinf(peq.q) || peq.q < 0.1f) peq.q = 1.0f;
            if (peq.q > 20.0f) peq.q = 20.0f;
        }

        // Delay
        if (isnan(preset.out[i].delay.delay_ms) || isinf(preset.out[i].delay.delay_ms) || preset.out[i].delay.delay_ms < 0.0f) {
            preset.out[i].delay.delay_ms = 0.0f;
        }
        if (preset.out[i].delay.delay_ms > AUDIO_MAX_DELAY_MS) preset.out[i].delay.delay_ms = AUDIO_MAX_DELAY_MS;
        if (preset.out[i].delay.unit > 2) preset.out[i].delay.unit = DELAY_UNIT_MS;
        if (isnan(preset.out[i].delay.temp_c) || isinf(preset.out[i].delay.temp_c) || preset.out[i].delay.temp_c < 0.0f || preset.out[i].delay.temp_c > 50.0f) {
            preset.out[i].delay.temp_c = 20.0f;
        }

        // Limiter
        if (isnan(preset.out[i].limiter.threshold_db) || isinf(preset.out[i].limiter.threshold_db)) preset.out[i].limiter.threshold_db = 0.0f;
        if (preset.out[i].limiter.threshold_db < -40.0f) preset.out[i].limiter.threshold_db = -40.0f;
        if (preset.out[i].limiter.threshold_db > 0.0f) preset.out[i].limiter.threshold_db = 0.0f;
        if (isnan(preset.out[i].limiter.attack_ms) || isinf(preset.out[i].limiter.attack_ms) || preset.out[i].limiter.attack_ms < 0.1f) {
            preset.out[i].limiter.attack_ms = 1.0f;
        }
        if (preset.out[i].limiter.attack_ms > 100.0f) preset.out[i].limiter.attack_ms = 100.0f;
        if (isnan(preset.out[i].limiter.release_ms) || isinf(preset.out[i].limiter.release_ms) || preset.out[i].limiter.release_ms < 10.0f) {
            preset.out[i].limiter.release_ms = 200.0f;
        }
        if (preset.out[i].limiter.release_ms > 2000.0f) preset.out[i].limiter.release_ms = 2000.0f;
    }

    // Setup sanitize
    if (preset.setup.brightness_pct < 10 || preset.setup.brightness_pct > 100) preset.setup.brightness_pct = 90;
    if (preset.setup.wifi_mode > 2) preset.setup.wifi_mode = DSP_WIFI_AP;
    if (preset.setup.out_mode > 1) preset.setup.out_mode = 0;
    preset.setup.wifi_ssid[sizeof(preset.setup.wifi_ssid) - 1] = '\0';
    preset.setup.wifi_pass[sizeof(preset.setup.wifi_pass) - 1] = '\0';
    preset.setup.web_password[sizeof(preset.setup.web_password) - 1] = '\0';
}

bool StorageManager::loadPreset(uint8_t slot, DspPreset& preset) {
    if (slot > MAX_USER_PRESETS) return false;

    char path[32];
    getPresetPath(slot, path, sizeof(path));

    if (!LittleFS.exists(path)) return false;

    // Lindungi pemrosesan audio saat membaca flash LittleFS
    g_dspStorageBusy = true;
    i2sDuplex.setHardwareMute(true, true);
    vTaskDelay(pdMS_TO_TICKS(5));

    File f = LittleFS.open(path, "r");
    if (!f) {
        i2sDuplex.flushDmaBuffers();
        g_dspStorageBusy = false;
        return false;
    }

    size_t len = f.read((uint8_t*)&preset, sizeof(DspPreset));
    f.close();

    if (len != sizeof(DspPreset) || preset.magic != 0x444C4D53) {
        i2sDuplex.flushDmaBuffers();
        g_dspStorageBusy = false;
        return false;
    }

    sanitizePreset(preset);
    i2sDuplex.flushDmaBuffers();
    g_dspStorageBusy = false;
    return true;
}

bool StorageManager::savePreset(uint8_t slot, const DspPreset& preset) {
    if (slot > MAX_USER_PRESETS) return false;

    char path[32];
    getPresetPath(slot, path, sizeof(path));

    if (slot == FACTORY_SLOT && LittleFS.exists(path)) {
        // Slot 0 Read-Only setelah dibuat pertama kali
        return false;
    }

    memset(&s_saveBuffer, 0, sizeof(DspPreset));
    s_saveBuffer = preset;
    sanitizePreset(s_saveBuffer);
    s_saveBuffer.magic = 0x444C4D53;
    s_saveBuffer.version = 1;
    s_saveBuffer.crc32 = calculateCrc32(s_saveBuffer);

    // KUNCI UTAMA: Lindungi audio selama penulisan Flash SPI (Cache CPU dinonaktifkan)
    g_dspStorageBusy = true;
    i2sDuplex.setHardwareMute(true, true);
    vTaskDelay(pdMS_TO_TICKS(5));

    File f = LittleFS.open(path, "w");
    if (!f) {
        i2sDuplex.flushDmaBuffers();
        g_dspStorageBusy = false;
        return false;
    }

    size_t written = f.write((const uint8_t*)&s_saveBuffer, sizeof(DspPreset));
    f.flush();
    f.close();

    bool success = (written == sizeof(DspPreset));
    if (success) {
        if (slot < TOTAL_SLOTS) {
            strncpy(_slotNames[slot], preset.name, sizeof(_slotNames[slot]) - 1);
            _slotNames[slot][sizeof(_slotNames[slot]) - 1] = '\0';
        }
        if (slot > 0 && _currentSlot != slot) {
            _currentSlot = slot;
            File fSlot = LittleFS.open("/active_slot.txt", "w");
            if (fSlot) {
                fSlot.print(_currentSlot);
                fSlot.close();
            }
        }
    }

    // Flush antrean I2S DMA yang sempat terhambat selama penulisan flash
    i2sDuplex.flushDmaBuffers();
    g_dspStorageBusy = false;
    return success;
}

bool StorageManager::renamePreset(uint8_t slot, const char* new_name) {
    if (slot == FACTORY_SLOT || slot > MAX_USER_PRESETS || !new_name) return false;
    if (!loadPreset(slot, s_tempPreset)) return false;

    strncpy(s_tempPreset.name, new_name, sizeof(s_tempPreset.name) - 1);
    s_tempPreset.name[sizeof(s_tempPreset.name) - 1] = '\0';
    if (slot < TOTAL_SLOTS) {
        strncpy(_slotNames[slot], new_name, sizeof(_slotNames[slot]) - 1);
        _slotNames[slot][sizeof(_slotNames[slot]) - 1] = '\0';
    }
    return savePreset(slot, s_tempPreset);
}

bool StorageManager::copyPreset(uint8_t src_slot, uint8_t dst_slot) {
    if (dst_slot == FACTORY_SLOT || dst_slot > MAX_USER_PRESETS || src_slot > MAX_USER_PRESETS) return false;
    if (!loadPreset(src_slot, s_tempPreset)) return false;

    snprintf(s_tempPreset.name, sizeof(s_tempPreset.name), "Copy-%u", src_slot);
    if (dst_slot < TOTAL_SLOTS) {
        strncpy(_slotNames[dst_slot], s_tempPreset.name, sizeof(_slotNames[dst_slot]) - 1);
        _slotNames[dst_slot][sizeof(_slotNames[dst_slot]) - 1] = '\0';
    }
    return savePreset(dst_slot, s_tempPreset);
}

bool StorageManager::deletePreset(uint8_t slot) {
    if (slot == FACTORY_SLOT || slot > MAX_USER_PRESETS) return false;
    s_tempPreset = getFactoryPreset();
    snprintf(s_tempPreset.name, sizeof(s_tempPreset.name), "Preset %u", slot);
    if (slot < TOTAL_SLOTS) {
        strncpy(_slotNames[slot], s_tempPreset.name, sizeof(_slotNames[slot]) - 1);
        _slotNames[slot][sizeof(_slotNames[slot]) - 1] = '\0';
    }
    return savePreset(slot, s_tempPreset);
}

void StorageManager::factoryReset() {
    for (uint8_t s = 1; s <= MAX_USER_PRESETS; s++) {
        deletePreset(s);
    }
    _currentSlot = 1;
    File fSlot = LittleFS.open("/active_slot.txt", "w");
    if (fSlot) {
        fSlot.print(_currentSlot);
        fSlot.close();
    }
}

void StorageManager::getSlotName(uint8_t slot, char* out_name, size_t max_len) {
    if (!out_name || max_len == 0) return;
    if (slot >= TOTAL_SLOTS) {
        snprintf(out_name, max_len, "Preset %u", slot);
        return;
    }
    strncpy(out_name, _slotNames[slot], max_len - 1);
    out_name[max_len - 1] = '\0';
}

void StorageManager::markConfigChanged() {
    _isDirty = true;
    _lastChangeTime = millis();
}

void StorageManager::tickAutoSave() {
    if (_isDirty && (millis() - _lastChangeTime >= AUTO_SAVE_DELAY_MS)) {
        _isDirty = false;
        savePreset(_currentSlot, dspEngine.getConfig());
    }
}

void StorageManager::saveCurrentImmediately() {
    _isDirty = false;
    savePreset(_currentSlot, dspEngine.getConfig());
}

void StorageManager::applyQuickCrossover(DspPreset& cfg, uint8_t type_2_3_4) {
    if (type_2_3_4 == 2) {
        // 2-Way: Out 1/2 Sub (LPF 100Hz LR4), Out 3/4 Mid/High (HPF 100Hz LR4)
        for (int i = 0; i < 2; i++) {
            strncpy(cfg.out[i].name, (i == 0) ? "Sub L" : "Sub R", sizeof(cfg.out[i].name));
            cfg.out[i].hpf.enabled = true; cfg.out[i].hpf.freq = 30.0f; cfg.out[i].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[i].hpf.slope = SLOPE_24DB;
            cfg.out[i].lpf.enabled = true; cfg.out[i].lpf.freq = 100.0f; cfg.out[i].lpf.type = FILTER_LINKWITZ_RILEY; cfg.out[i].lpf.slope = SLOPE_24DB;
        }
        for (int i = 2; i < 4; i++) {
            strncpy(cfg.out[i].name, (i == 2) ? "Top L" : "Top R", sizeof(cfg.out[i].name));
            cfg.out[i].hpf.enabled = true; cfg.out[i].hpf.freq = 100.0f; cfg.out[i].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[i].hpf.slope = SLOPE_24DB;
            cfg.out[i].lpf.enabled = false;
        }
    } else if (type_2_3_4 == 3) {
        // 3-Way: Out 1 Sub (LPF 90Hz), Out 2 & 3 Low/Mid (HPF 90Hz & LPF 2500Hz), Out 4 High (HPF 2500Hz)
        strncpy(cfg.out[0].name, "Sub", sizeof(cfg.out[0].name));
        cfg.out[0].hpf.enabled = true; cfg.out[0].hpf.freq = 30.0f; cfg.out[0].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[0].hpf.slope = SLOPE_24DB;
        cfg.out[0].lpf.enabled = true; cfg.out[0].lpf.freq = 90.0f; cfg.out[0].lpf.type = FILTER_LINKWITZ_RILEY; cfg.out[0].lpf.slope = SLOPE_24DB;

        for (int i = 1; i <= 2; i++) {
            strncpy(cfg.out[i].name, (i == 1) ? "Mid L" : "Mid R", sizeof(cfg.out[i].name));
            cfg.out[i].hpf.enabled = true; cfg.out[i].hpf.freq = 90.0f; cfg.out[i].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[i].hpf.slope = SLOPE_24DB;
            cfg.out[i].lpf.enabled = true; cfg.out[i].lpf.freq = 2500.0f; cfg.out[i].lpf.type = FILTER_LINKWITZ_RILEY; cfg.out[i].lpf.slope = SLOPE_24DB;
        }

        strncpy(cfg.out[3].name, "High", sizeof(cfg.out[3].name));
        cfg.out[3].hpf.enabled = true; cfg.out[3].hpf.freq = 2500.0f; cfg.out[3].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[3].hpf.slope = SLOPE_24DB;
        cfg.out[3].lpf.enabled = false;
    } else if (type_2_3_4 == 4) {
        // 4-Way: Out 1 Sub (80Hz), Out 2 Low (80-350Hz), Out 3 Mid (350-3500Hz), Out 4 High (3500Hz)
        strncpy(cfg.out[0].name, "Sub", sizeof(cfg.out[0].name));
        cfg.out[0].hpf.enabled = true; cfg.out[0].hpf.freq = 30.0f; cfg.out[0].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[0].hpf.slope = SLOPE_24DB;
        cfg.out[0].lpf.enabled = true; cfg.out[0].lpf.freq = 80.0f; cfg.out[0].lpf.type = FILTER_LINKWITZ_RILEY; cfg.out[0].lpf.slope = SLOPE_24DB;

        strncpy(cfg.out[1].name, "Low", sizeof(cfg.out[1].name));
        cfg.out[1].hpf.enabled = true; cfg.out[1].hpf.freq = 80.0f; cfg.out[1].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[1].hpf.slope = SLOPE_24DB;
        cfg.out[1].lpf.enabled = true; cfg.out[1].lpf.freq = 350.0f; cfg.out[1].lpf.type = FILTER_LINKWITZ_RILEY; cfg.out[1].lpf.slope = SLOPE_24DB;

        strncpy(cfg.out[2].name, "Mid", sizeof(cfg.out[2].name));
        cfg.out[2].hpf.enabled = true; cfg.out[2].hpf.freq = 350.0f; cfg.out[2].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[2].hpf.slope = SLOPE_24DB;
        cfg.out[2].lpf.enabled = true; cfg.out[2].lpf.freq = 3500.0f; cfg.out[2].lpf.type = FILTER_LINKWITZ_RILEY; cfg.out[2].lpf.slope = SLOPE_24DB;

        strncpy(cfg.out[3].name, "High", sizeof(cfg.out[3].name));
        cfg.out[3].hpf.enabled = true; cfg.out[3].hpf.freq = 3500.0f; cfg.out[3].hpf.type = FILTER_LINKWITZ_RILEY; cfg.out[3].hpf.slope = SLOPE_24DB;
        cfg.out[3].lpf.enabled = false;
    }
}
