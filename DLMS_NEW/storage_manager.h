#pragma once
#include <Arduino.h>
#include "dsp_types.h"

// ============================================================================
// STORAGE & PRESETS MANAGER (NVS / FLASH LITTLEFS)
// 10 Slot Preset Pengguna + 1 Slot Factory (Read-Only)
// Auto-Save 3 Detik Debounce, CRC32 Integritas Data, Mute Aman 20ms
// ============================================================================

class StorageManager {
public:
    static const uint8_t FACTORY_SLOT = 0;
    static const uint8_t MAX_USER_PRESETS = 10;
    static const uint8_t TOTAL_SLOTS = 11; // 0 = Factory, 1..10 = User

    StorageManager();

    bool begin();

    // Preset Operations
    bool loadPreset(uint8_t slot, DspPreset& preset);
    bool savePreset(uint8_t slot, const DspPreset& preset);
    bool renamePreset(uint8_t slot, const char* new_name);
    bool copyPreset(uint8_t src_slot, uint8_t dst_slot);
    bool deletePreset(uint8_t slot); // Reset to default
    void factoryReset();

    // Slot aktif saat ini
    uint8_t getCurrentSlot() const { return _currentSlot; }
    void setCurrentSlot(uint8_t slot) { _currentSlot = slot; }

    // Ambil daftar nama slot untuk menu UI
    void getSlotName(uint8_t slot, char* out_name, size_t max_len);

    // Auto-Save Debounce (Triggered saat parameter diubah lewat UI/Web)
    void markConfigChanged();
    void tickAutoSave(); // Dipanggil secara berkala di Core 0
    void saveCurrentImmediately(); // Simpan langsung ke Flash tanpa menunggu debounce

    // Crossover Quick Presets (2-Way, 3-Way, 4-Way LR4)
    void applyQuickCrossover(DspPreset& cfg, uint8_t type_2_3_4);

    // Default factory configuration generator
    static const DspPreset& getFactoryPreset();

    // Validasi dan sanitasi nilai-nilai float preset (mencegah NaN / overflow / crash)
    static void sanitizePreset(DspPreset& preset);

    // CRC32 calculation
    static uint32_t calculateCrc32(const DspPreset& preset);

private:
    uint8_t _currentSlot;
    bool _isDirty;
    uint32_t _lastChangeTime;
    char _slotNames[TOTAL_SLOTS][16];
    static const uint32_t AUTO_SAVE_DELAY_MS = 2000; // 2 detik debounce
};

extern volatile bool g_dspStorageBusy;
extern StorageManager storageManager;
