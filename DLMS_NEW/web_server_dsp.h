#pragma once
#include <Arduino.h>

// ============================================================================
// WEB SERVER & WIFI REMOTE CONTROL (ESP32-S3 CORE 0 TASK)
// SoftAP "AudioDSP-xxxx" + Captive Portal + REST JSON API
// Sesuai PRD Bagian 7 (Kontrol WiFi & Web)
// ============================================================================

class WebServerDsp {
public:
    WebServerDsp();

    bool begin();
    void loop();

    String getIpAddress() const;
    String getSsid() const { return _apSsid; }
    String getPassword() const { return _apPass; }
    uint8_t getClientCount() const;
    void setWifiMode(uint8_t mode);
    bool isWifiEnabled() const;
    uint8_t getWifiMode() const { return _wifiMode; }

private:
    volatile bool _isRunning;
    volatile uint8_t _wifiMode;
    bool _isApMode;
    String _apSsid;
    String _apPass;
    void setupRoutes();
};

extern WebServerDsp webServerDsp;
