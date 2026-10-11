#include "web_server_dsp.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "web_ui.h"
#include "dsp_engine.h"
#include "storage_manager.h"

WebServerDsp webServerDsp;

static WebServer server(80);
static DNSServer dnsServer;
static const byte DNS_PORT = 53;

static void webServerTask(void* param) {
    while (1) {
        webServerDsp.loop();
        vTaskDelay(pdMS_TO_TICKS(1)); // Ultra low latency untuk respon Web UI responsif & instan
    }
}

WebServerDsp::WebServerDsp()
    : _isRunning(false),
      _wifiMode(DSP_WIFI_AP),
      _isApMode(true),
      _apSsid("DLMS-DSP-SHAWIR"),
      _apPass("0987654321")
{
}

bool WebServerDsp::begin() {
    // 1. Matikan persistensi WiFi agar tidak terganggu data stale NVS
    WiFi.persistent(false);

    // 2. Daftarkan rute Web Server REST API
    setupRoutes();

    // 3. Jalankan Web Task di Core 0 (prioritas 1)
    xTaskCreatePinnedToCore(webServerTask, "web_task", 8192, NULL, 1, NULL, 0);

    // 4. Inisialisasi mode WiFi sesuai konfigurasi preset aktif
    const DspPreset& cfg = dspEngine.getConfig();
    uint8_t initMode = cfg.setup.wifi_mode;
    if (initMode > 2) initMode = DSP_WIFI_AP;
    setWifiMode(initMode);

    return true;
}

void WebServerDsp::loop() {
    if (!_isRunning) return;
    if (_wifiMode == DSP_WIFI_OFF) return;

    if (_wifiMode == DSP_WIFI_AP && _isApMode) {
        dnsServer.processNextRequest();
        server.handleClient();
    } else if (_wifiMode == DSP_WIFI_STA) {
        if (WiFi.status() == WL_CONNECTED) {
            server.handleClient();
        }
    }
}

String WebServerDsp::getIpAddress() const {
    if (_wifiMode == DSP_WIFI_OFF || WiFi.getMode() == WIFI_OFF) return "0.0.0.0";
    if (_wifiMode == DSP_WIFI_STA) {
        return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "STA Mencari...";
    }
    return "192.168.4.1";
}

uint8_t WebServerDsp::getClientCount() const {
    if (_wifiMode == DSP_WIFI_OFF || WiFi.getMode() == WIFI_OFF) return 0;
    if (_wifiMode == DSP_WIFI_STA) return (WiFi.status() == WL_CONNECTED) ? 1 : 0;
    return WiFi.softAPgetStationNum();
}

bool WebServerDsp::isWifiEnabled() const {
    return (_wifiMode != DSP_WIFI_OFF && WiFi.getMode() != WIFI_OFF);
}

void WebServerDsp::setWifiMode(uint8_t mode) {
    _isRunning = false;
    vTaskDelay(pdMS_TO_TICKS(20)); // Tunggu webServerTask menyelesaikan iterasi aktif

    if (mode == DSP_WIFI_OFF) {
        dnsServer.stop();
        server.stop();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        _wifiMode = DSP_WIFI_OFF;
        _isApMode = false;
        Serial.println("[WiFi] Mode OFF: Radio nonaktif");
    } else if (mode == DSP_WIFI_STA) {
        dnsServer.stop();
        server.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _wifiMode = DSP_WIFI_STA;
        _isApMode = false;

        const DspPreset& cfg = dspEngine.getConfig();
        if (strlen(cfg.setup.wifi_ssid) > 0) {
            WiFi.begin(cfg.setup.wifi_ssid, cfg.setup.wifi_pass);
            Serial.printf("[WiFi] Mode Station: Menghubungkan ke %s\n", cfg.setup.wifi_ssid);
        } else {
            Serial.println("[WiFi] Mode Station aktif (SSID belum diset)");
        }
        server.begin();
        _isRunning = true;
    } else { // DSP_WIFI_AP (mode == 1)
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        IPAddress apIP(192, 168, 4, 1);
        IPAddress netMsk(255, 255, 255, 0);
        WiFi.softAPConfig(apIP, apIP, netMsk);

        // Enkripsi WPA2 SoftAP dengan password bawaan 0987654321
        WiFi.softAP(_apSsid.c_str(), _apPass.c_str(), 1, 0, 4);
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(DNS_PORT, "*", apIP);
        server.begin();

        _wifiMode = DSP_WIFI_AP;
        _isApMode = true;
        _isRunning = true;
        Serial.printf("[WiFi] Mode SoftAP Aktif: SSID=\"%s\", Pass=\"%s\", IP=http://192.168.4.1\n", _apSsid.c_str(), _apPass.c_str());
    }
}

void WebServerDsp::setupRoutes() {
    // 1. Halaman Utama Web UI
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    // 2. High-speed Real-time VU Meter Telemetry API (10 Hz)
    server.on("/api/meter", HTTP_GET, []() {
        JsonDocument doc;
        VuMeterData m = dspEngine.getMeterData();

        doc["cpu"] = round(m.cpu_usage_pct);
        doc["uptime"] = m.uptime_sec;
        doc["ver"] = dspEngine.getConfigVersion();

        JsonArray inP = doc["in_peak"].to<JsonArray>();
        inP.add(round(m.in_peak_db[0] * 10.0f) / 10.0f);
        inP.add(round(m.in_peak_db[1] * 10.0f) / 10.0f);

        JsonArray outP = doc["out_peak"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            outP.add(round(m.out_peak_db[i] * 10.0f) / 10.0f);
        }

        JsonArray grArr = doc["limiter_gr"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            grArr.add(round(m.limiter_gr_db[i] * 10.0f) / 10.0f);
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // 3. Ambil Full Konfigurasi DspPreset
    server.on("/api/config", HTTP_GET, []() {
        JsonDocument doc;
        const DspPreset& cfg = dspEngine.getConfig();

        doc["name"] = cfg.name;
        doc["current_slot"] = storageManager.getCurrentSlot();
        doc["master_mute"] = cfg.master_mute;

        // Setup & Mode
        JsonObject setupObj = doc["setup"].to<JsonObject>();
        setupObj["out_mode"] = cfg.setup.out_mode;
        setupObj["wifi_mode"] = cfg.setup.wifi_mode;
        setupObj["wifi_ssid"] = cfg.setup.wifi_ssid;
        setupObj["web_access"] = cfg.setup.web_access;
        setupObj["brightness_pct"] = cfg.setup.brightness_pct;
        setupObj["theme"] = cfg.setup.theme;

        // Inputs
        JsonArray inArr = doc["in"].to<JsonArray>();
        for (int i = 0; i < 2; i++) {
            JsonObject inObj = inArr.add<JsonObject>();
            inObj["name"] = (i == 0) ? "IN A" : "IN B";
            inObj["gain_db"] = cfg.in[i].gain_db;
            inObj["mute"] = cfg.in[i].mute;
            JsonArray peqArr = inObj["peq"].to<JsonArray>();
            for (int b = 0; b < 4; b++) {
                JsonObject p = peqArr.add<JsonObject>();
                p["enabled"] = cfg.in[i].peq[b].enabled;
                p["type"] = cfg.in[i].peq[b].type;
                p["freq"] = cfg.in[i].peq[b].freq;
                p["gain_db"] = cfg.in[i].peq[b].gain_db;
                p["q"] = cfg.in[i].peq[b].q;
                p["bypass"] = cfg.in[i].peq[b].bypass;
            }
        }

        // Outputs
        JsonArray outArr = doc["out"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            JsonObject outObj = outArr.add<JsonObject>();
            outObj["name"] = cfg.out[i].name;
            outObj["source"] = cfg.out[i].source;
            outObj["gain_db"] = cfg.out[i].gain_db;
            outObj["mute"] = cfg.out[i].mute;
            outObj["polarity_invert"] = cfg.out[i].polarity_invert;

            JsonObject hpf = outObj["hpf"].to<JsonObject>();
            hpf["enabled"] = cfg.out[i].hpf.enabled;
            hpf["freq"] = cfg.out[i].hpf.freq;
            hpf["type"] = cfg.out[i].hpf.type;
            hpf["slope"] = cfg.out[i].hpf.slope;

            JsonObject lpf = outObj["lpf"].to<JsonObject>();
            lpf["enabled"] = cfg.out[i].lpf.enabled;
            lpf["freq"] = cfg.out[i].lpf.freq;
            lpf["type"] = cfg.out[i].lpf.type;
            lpf["slope"] = cfg.out[i].lpf.slope;

            JsonArray peqArr = outObj["peq"].to<JsonArray>();
            for (int b = 0; b < 8; b++) {
                JsonObject p = peqArr.add<JsonObject>();
                p["enabled"] = cfg.out[i].peq[b].enabled;
                p["type"] = cfg.out[i].peq[b].type;
                p["freq"] = cfg.out[i].peq[b].freq;
                p["gain_db"] = cfg.out[i].peq[b].gain_db;
                p["q"] = cfg.out[i].peq[b].q;
                p["bypass"] = cfg.out[i].peq[b].bypass;
            }

            JsonObject lim = outObj["limiter"].to<JsonObject>();
            lim["enabled"] = cfg.out[i].limiter.enabled;
            lim["threshold_db"] = cfg.out[i].limiter.threshold_db;
            lim["attack_ms"] = cfg.out[i].limiter.attack_ms;
            lim["release_ms"] = cfg.out[i].limiter.release_ms;

            JsonObject dly = outObj["delay"].to<JsonObject>();
            dly["delay_ms"] = cfg.out[i].delay.delay_ms;
            dly["unit"] = cfg.out[i].delay.unit;
            dly["temp_c"] = cfg.out[i].delay.temp_c;
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // 4. Update Master Mute
    server.on("/api/master_mute", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        bool m = doc["mute"] | false;
        dspEngine.setMasterMute(m);
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 5. Update Channel (In A/B & Out 1..4: Gain, Mute, Polarity, Routing, Name)
    server.on("/api/channel", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        String type = doc["type"] | "out";
        uint8_t ch = doc["channel"] | 0;

        if (type == "in" && ch < 2) {
            if (doc.containsKey("gain_db")) dspEngine.setInputGain(ch, doc["gain_db"]);
            if (doc.containsKey("mute")) dspEngine.setInputMute(ch, doc["mute"]);
        } else if (type == "out" && ch < 4) {
            if (doc.containsKey("gain_db")) dspEngine.setOutputGain(ch, doc["gain_db"]);
            if (doc.containsKey("mute")) dspEngine.setOutputMute(ch, doc["mute"]);
            if (doc.containsKey("polarity_invert")) dspEngine.setOutputPolarity(ch, doc["polarity_invert"]);
            if (doc.containsKey("source")) dspEngine.setOutputSource(ch, doc["source"]);
        }
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 6. Update Output Mode (Mode 1: Stereo Pair 1-2 & 3-4 Linked vs Mode 2: 4-Way Individual)
    server.on("/api/mode", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t m = doc["out_mode"] | 0;
        DspPreset cfg = dspEngine.getConfig();
        cfg.setup.out_mode = (m > 1) ? 1 : m;
        dspEngine.setConfig(cfg);
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 7. Update PEQ Band (Input 0..1 atau Output 0..3)
    server.on("/api/peq", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t target = doc["target"] | 0;
        uint8_t band = doc["band"] | 0;
        JsonObject p = doc["config"];

        bool en = p["enabled"] | true;
        uint8_t type = p["type"] | 0;
        float freq = p["freq"] | 1000.0f;
        float gain = p["gain_db"] | 0.0f;
        float q = p["q"] | 1.0f;
        bool byp = p["bypass"] | false;

        if (target < 2) {
            dspEngine.setInputPeq(target, band, en, type, freq, gain, q, byp);
        } else {
            dspEngine.setOutputPeq(target - 2, band, en, type, freq, gain, q, byp);
        }
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 8. Update Crossover HPF / LPF (Lengkap On/Off, Freq, Tipe, Slope)
    server.on("/api/xover", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t ch = doc["channel"] | 0;
        String typeStr = doc["type"] | "hpf";
        JsonObject c = doc["config"];

        bool en = c["enabled"] | false;
        float freq = c["freq"] | 100.0f;
        uint8_t ft = c["type"] | 1;
        uint8_t slope = c["slope"] | 24;

        if (typeStr == "hpf") {
            dspEngine.setOutputHpf(ch, en, freq, ft, slope);
        } else {
            dspEngine.setOutputLpf(ch, en, freq, ft, slope);
        }
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 9. Quick Crossover (2-Way, 3-Way, 4-Way)
    server.on("/api/quick_xover", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t type = doc["type"] | 2;
        static DspPreset cfg;
        cfg = dspEngine.getConfig();
        storageManager.applyQuickCrossover(cfg, type);
        dspEngine.setConfig(cfg);
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 10. Update Limiter (Lengkap On/Off, Threshold, Attack, Release)
    server.on("/api/limiter", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t ch = doc["channel"] | 0;
        bool en = doc["enabled"] | true;
        float th = doc["threshold_db"] | 0.0f;
        float att = doc["attack_ms"] | 1.0f;
        float rel = doc["release_ms"] | 200.0f;
        dspEngine.setOutputLimiter(ch, en, th, att, rel);
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 11. Update Delay (Lengkap ms, unit, suhu udara)
    server.on("/api/delay", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t ch = doc["channel"] | 0;
        float d = doc["delay_ms"] | 0.0f;
        uint8_t unit = doc["unit"] | 0;
        float temp = doc["temp_c"] | 20.0f;
        dspEngine.setOutputDelay(ch, d, unit, temp);
        storageManager.markConfigChanged();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 12. Daftar Lengkap Slot Preset (Slot 0..10)
    server.on("/api/presets", HTTP_GET, []() {
        JsonDocument doc;
        doc["current_slot"] = storageManager.getCurrentSlot();
        JsonArray arr = doc["presets"].to<JsonArray>();
        char nameBuf[32];
        for (uint8_t s = 0; s <= 10; s++) {
            JsonObject p = arr.add<JsonObject>();
            p["slot"] = s;
            storageManager.getSlotName(s, nameBuf, sizeof(nameBuf));
            p["name"] = nameBuf;
            p["is_current"] = (s == storageManager.getCurrentSlot());
            p["is_factory"] = (s == 0);
        }
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // 13. Preset Load & Save & Rename
    server.on("/api/preset/load", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t slot = doc["slot"] | 1;
        static DspPreset p;
        dspEngine.setMasterMute(true);
        delay(25);
        bool ok = storageManager.loadPreset(slot, p);
        if (ok) {
            dspEngine.setConfig(p);
            storageManager.setCurrentSlot(slot);
        }
        delay(25);
        dspEngine.setMasterMute(false);
        if (ok) {
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(404, "application/json", "{\"status\":\"error\",\"msg\":\"Gagal memuat preset\"}");
        }
    });

    server.on("/api/preset/save", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t slot = doc["slot"] | 1;
        dspEngine.setMasterMute(true);
        delay(25);
        bool ok = storageManager.savePreset(slot, dspEngine.getConfig());
        delay(25);
        dspEngine.setMasterMute(false);
        if (ok) {
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(500, "application/json", "{\"status\":\"error\",\"msg\":\"Gagal menyimpan preset\"}");
        }
    });

    server.on("/api/preset/rename", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t slot = doc["slot"] | 1;
        const char* name = doc["name"] | "Preset";
        if (storageManager.renamePreset(slot, name)) {
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(500, "application/json", "{\"status\":\"error\"}");
        }
    });

    // 14. Setup & WiFi Remote Configuration
    server.on("/api/setup", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        DspPreset cfg = dspEngine.getConfig();

        if (doc.containsKey("wifi_ssid")) {
            strncpy(cfg.setup.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.setup.wifi_ssid) - 1);
        }
        if (doc.containsKey("wifi_pass")) {
            strncpy(cfg.setup.wifi_pass, doc["wifi_pass"], sizeof(cfg.setup.wifi_pass) - 1);
        }
        if (doc.containsKey("web_access")) {
            cfg.setup.web_access = doc["web_access"];
        }
        uint8_t newWifiMode = cfg.setup.wifi_mode;
        bool wifiModeChanged = false;
        if (doc.containsKey("wifi_mode")) {
            newWifiMode = doc["wifi_mode"];
            if (newWifiMode != cfg.setup.wifi_mode) {
                cfg.setup.wifi_mode = newWifiMode;
                wifiModeChanged = true;
            }
        }

        dspEngine.setConfig(cfg);
        storageManager.markConfigChanged();
        storageManager.saveCurrentImmediately();

        server.send(200, "application/json", "{\"status\":\"ok\"}");

        if (wifiModeChanged) {
            webServerDsp.setWifiMode(newWifiMode);
        }
    });

    // 15. Factory Reset
    server.on("/api/factory_reset", HTTP_POST, []() {
        storageManager.factoryReset();
        dspEngine.setConfig(StorageManager::getFactoryPreset());
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // Captive Portal Endpoints
    server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/canonical.html", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/connecttest.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft Connect Test"); });
    server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });

    server.onNotFound([]() {
        server.send_P(200, "text/html", INDEX_HTML);
    });
}
