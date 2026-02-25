// =============================================================================
// modem73_gui.cc  –  Windows GUI (Dear ImGui + GLFW + OpenGL3)
// Compile with: -DMODEM73_GUI_MODE -DWITH_UI
// =============================================================================
#include "windows_socket_compat.hh"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <random>
#include <windows.h>    // Registry enumeration for COM ports
#include <commdlg.h>   // GetOpenFileNameA / GetSaveFileNameA

// modem73 backend
#include "miniaudio_audio.hh"
#include "kiss_tnc.hh"    // TNCConfig, PTTType
#include "tnc_ui.hh"      // TNCUIState, MODULATION_OPTIONS, CODE_RATE_OPTIONS, PTT_TYPE_OPTIONS

// ─── Globals required by kiss_tnc.cc ────────────────────────────────────────
std::atomic<bool>  g_running{false};
TNCConfig          g_config;
bool               g_verbose = false;
bool               g_use_ui  = true;   // enable callbacks in TNC
TNCUIState*        g_ui_state = nullptr;

void ui_log(const std::string& msg) {
    if (g_ui_state) g_ui_state->add_log(msg);
}

// ─── Forward: run_tnc defined in kiss_tnc.cc ────────────────────────────────
void run_tnc(TNCConfig& cfg, TNCUIState& ui_state, std::atomic<bool>& running);

// ─── GUI state ───────────────────────────────────────────────────────────────
static TNCUIState        g_ui;
static std::thread       g_tnc_thread;
static std::atomic<bool> g_tnc_running{false};

// Text edit buffers (kept in sync with g_ui string fields)
static char s_callsign[16] = "N0CALL";
static char s_comport[32]  = "COM3";
static char s_righost[64]  = "localhost";

// Utils
static int  g_test_size_override = 0;  // 0 = use g_ui.random_data_size

// Auto-threshold calibration
static bool  g_calibrating   = false;
static float g_calib_max     = -100.f;
static std::chrono::steady_clock::time_point g_calib_start;

// COM port list
static std::vector<std::string> g_com_ports;
static int g_com_port_idx = 0;

// ─────────────────────────────────────────────────────────────────────────────

// Enumerate available Windows COM ports via registry
static void refresh_com_ports() {
    g_com_ports.clear();
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char valName[256], valData[256];
        DWORD idx = 0;
        while (true) {
            DWORD nameLen = sizeof(valName), dataLen = sizeof(valData), type;
            LONG r = RegEnumValueA(hKey, idx++, valName, &nameLen,
                                   nullptr, &type, (LPBYTE)valData, &dataLen);
            if (r != ERROR_SUCCESS) break;
            if (type == REG_SZ) g_com_ports.push_back(valData);
        }
        RegCloseKey(hKey);
    }
    // Natural sort: COM1, COM2, ..., COM10, COM11, ...
    std::sort(g_com_ports.begin(), g_com_ports.end(),
        [](const std::string& a, const std::string& b) {
            // Extract number after "COM"
            auto num = [](const std::string& s) -> int {
                size_t p = s.find_first_of("0123456789");
                return p != std::string::npos ? std::stoi(s.substr(p)) : 0;
            };
            return num(a) < num(b);
        });

    if (g_com_ports.empty())
        g_com_ports.push_back("COM3");  // fallback

    // Find index matching current selection
    g_com_port_idx = 0;
    for (int i = 0; i < (int)g_com_ports.size(); i++) {
        if (g_com_ports[i] == s_comport) { g_com_port_idx = i; break; }
    }
}

static void sync_buffers_from_ui() {
    strncpy(s_callsign, g_ui.callsign.c_str(),   sizeof(s_callsign)-1);
    strncpy(s_comport,  g_ui.com_port.c_str(),   sizeof(s_comport)-1);
    strncpy(s_righost,  g_ui.rigctl_host.c_str(),sizeof(s_righost)-1);
}

static void apply_settings() {
    // Sync text buffers back to g_ui
    g_ui.callsign   = s_callsign;
    g_ui.com_port   = s_comport;
    g_ui.rigctl_host = s_righost;
    g_ui.update_modem_info();
    if (g_tnc_running && g_ui.on_settings_changed)
        g_ui.on_settings_changed(g_ui);
    // Persist immediately so next launch restores current settings
    g_ui.save_settings();
}

static void refresh_audio_devs() {
    g_ui.available_input_devices.clear();
    g_ui.input_device_descriptions.clear();
    g_ui.available_output_devices.clear();
    g_ui.output_device_descriptions.clear();

    for (auto& p : MiniAudio::list_capture_devices()) {
        g_ui.available_input_devices.push_back(p.first);
        g_ui.input_device_descriptions.push_back(p.second);
    }
    if (g_ui.available_input_devices.empty()) {
        g_ui.available_input_devices.push_back("default");
        g_ui.input_device_descriptions.push_back("default");
    }

    for (auto& p : MiniAudio::list_playback_devices()) {
        g_ui.available_output_devices.push_back(p.first);
        g_ui.output_device_descriptions.push_back(p.second);
    }
    if (g_ui.available_output_devices.empty()) {
        g_ui.available_output_devices.push_back("default");
        g_ui.output_device_descriptions.push_back("default");
    }

    g_ui.audio_input_index  = std::min(g_ui.audio_input_index,
                                        (int)g_ui.available_input_devices.size()-1);
    g_ui.audio_output_index = std::min(g_ui.audio_output_index,
                                        (int)g_ui.available_output_devices.size()-1);
    if (g_ui.audio_input_index  < 0) g_ui.audio_input_index  = 0;
    if (g_ui.audio_output_index < 0) g_ui.audio_output_index = 0;
}

static void start_tnc() {
    if (g_tnc_running) return;

    // Build TNCConfig from g_ui
    g_ui.callsign    = s_callsign;
    g_ui.com_port    = s_comport;
    g_ui.rigctl_host = s_righost;

    g_config = TNCConfig{};
    g_config.callsign          = g_ui.callsign;
    g_config.port              = g_ui.port;
    g_config.bind_address      = "0.0.0.0";
    g_config.center_freq       = g_ui.center_freq;
    g_config.modulation        = MODULATION_OPTIONS[g_ui.modulation_index];
    g_config.code_rate         = CODE_RATE_OPTIONS[g_ui.code_rate_index];
    g_config.short_frame       = g_ui.short_frame;
    g_config.csma_enabled      = g_ui.csma_enabled;
    g_config.carrier_threshold_db = g_ui.carrier_threshold_db;
    g_config.p_persistence     = g_ui.p_persistence;
    g_config.slot_time_ms      = g_ui.slot_time_ms;
    g_config.fragmentation_enabled = g_ui.fragmentation_enabled;
    g_config.tx_blanking_enabled   = g_ui.tx_blanking_enabled;

    if (!g_ui.available_input_devices.empty())
        g_config.audio_input_device  = g_ui.available_input_devices[g_ui.audio_input_index];
    if (!g_ui.available_output_devices.empty())
        g_config.audio_output_device = g_ui.available_output_devices[g_ui.audio_output_index];

    g_config.ptt_type      = static_cast<PTTType>(g_ui.ptt_type_index);
    g_config.rigctl_host   = g_ui.rigctl_host;
    g_config.rigctl_port   = g_ui.rigctl_port;
    g_config.vox_tone_freq = g_ui.vox_tone_freq;
    g_config.vox_lead_ms   = g_ui.vox_lead_ms;
    g_config.vox_tail_ms   = g_ui.vox_tail_ms;
    g_config.com_port      = g_ui.com_port;
    g_config.com_ptt_line  = g_ui.com_ptt_line;
    g_config.com_invert_dtr= g_ui.com_invert_dtr;
    g_config.com_invert_rts= g_ui.com_invert_rts;

    g_ui.update_modem_info();

    g_running     = true;
    g_ui_state    = &g_ui;
    g_tnc_running = true;

    g_tnc_thread = std::thread([](){
        run_tnc(g_config, g_ui, g_running);
        g_tnc_running = false;
    });
}

static void stop_tnc() {
    if (!g_tnc_running) return;
    g_running = false;
    if (g_tnc_thread.joinable()) g_tnc_thread.join();
    g_tnc_running = false;
}

// ─── Drawing helpers ─────────────────────────────────────────────────────────

static void draw_level_bar(float db, float min_db, float max_db, float thresh_db) {
    float t = (db - min_db) / (max_db - min_db);
    t = std::max(0.0f, std::min(1.0f, t));
    float tt = (thresh_db - min_db) / (max_db - min_db);

    bool over = db > thresh_db;
    ImVec4 col = over            ? ImVec4(1.f,.3f,.1f,1.f)
               : t > 0.66f      ? ImVec4(.9f,.8f,.1f,1.f)
                                 : ImVec4(.2f,.9f,.3f,1.f);

    float w = ImGui::GetContentRegionAvail().x - 62.f;
    float h = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, {p.x+w, p.y+h}, IM_COL32(25,25,25,255));
    if (t > 0)
        dl->AddRectFilled(p, {p.x+w*t, p.y+h},
            IM_COL32((int)(col.x*255),(int)(col.y*255),(int)(col.z*255),220));
    // Threshold marker
    float tx = p.x + w * std::max(0.f, std::min(1.f, tt));
    dl->AddLine({tx, p.y}, {tx, p.y+h}, IM_COL32(255,200,50,200), 2);
    dl->AddRect(p, {p.x+w, p.y+h}, IM_COL32(70,70,70,200));
    ImGui::Dummy({w, h});
    ImGui::SameLine();
    ImGui::Text("%+.0f dB", db);
}

static void draw_level_history() {
    const int  N   = TNCUIState::LEVEL_HISTORY_SIZE;
    static float buf[TNCUIState::LEVEL_HISTORY_SIZE];
    {
        std::lock_guard<std::mutex> lk(g_ui.level_mutex);
        const int pos = g_ui.level_history_pos;
        for (int i = 0; i < N; i++)
            buf[i] = g_ui.level_history[(pos + i) % N];
    }
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(.2f,.9f,.3f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.05f,.05f,.05f,1.f));
    ImGui::PlotLines("##lh", buf, N, 0, nullptr, -80.f, 0.f,
                     {ImGui::GetContentRegionAvail().x, 48.f});
    ImGui::PopStyleColor(2);
}

static void draw_constellation() {
    if (!g_ui.constellation_valid) {
        ImGui::TextDisabled("(waiting for signal...)");
        return;
    }
    std::lock_guard<std::mutex> lk(g_ui.constellation_mutex);
    const int N  = TNCUIState::CONSTELLATION_SIZE;
    const int SZ = 300;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, {p0.x+SZ, p0.y+SZ}, IM_COL32(6,6,16,255));
    float mid = SZ * 0.5f;
    dl->AddLine({p0.x+mid, p0.y}, {p0.x+mid, p0.y+SZ}, IM_COL32(30,30,60,200));
    dl->AddLine({p0.x, p0.y+mid}, {p0.x+SZ, p0.y+mid}, IM_COL32(30,30,60,200));

    // Scale matching tnc_ui.hh update_constellation()
    float scale;
    switch (g_ui.constellation_mod_bits) {
        case 1:  scale = 1.5f; break;
        case 2:  scale = 1.3f; break;
        case 3:  scale = 1.5f; break;
        case 4:  scale = 1.7f; break;
        case 6:  scale = 2.0f; break;
        case 8:  scale = 2.3f; break;
        case 10: scale = 2.5f; break;
        case 12: scale = 2.5f; break;
        default: scale = 1.5f; break;
    }

    // Render raw IQ points at exact float positions — no grid quantisation
    for (int i = 0; i < N; ++i) {
        float re = g_ui.constellation_points[i].real();
        float im = g_ui.constellation_points[i].imag();
        float px = p0.x + mid + re * mid / scale;
        float py = p0.y + mid - im * mid / scale;  // flip Y
        if (px < p0.x || px >= p0.x+SZ || py < p0.y || py >= p0.y+SZ) continue;
        dl->AddCircleFilled({px, py}, 2.5f, IM_COL32(80, 200, 255, 210), 6);
    }

    dl->AddRect(p0, {p0.x+SZ, p0.y+SZ}, IM_COL32(50,50,90,255));
    ImGui::Dummy({(float)SZ, (float)SZ});
}

// ─── Settings panel (left column) ───────────────────────────────────────────
// Layout: label column fixed at LBL_W px, widget fills the rest.

static void draw_settings_panel() {
    bool running = g_tnc_running.load();

    // Two-column layout helper:
    // lbl()  – writes label, then SameLine(LBL_W) so next widget starts at fixed offset
    static const float LBL_W = 118.f;
    auto tbl_begin = []() {};
    auto tbl_end   = []() {};
    auto lbl = [](const char* text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", text);
        ImGui::SameLine(LBL_W);
        ImGui::SetNextItemWidth(-1);
    };

    // Slider + inline input box helpers (override SetNextItemWidth(-1) from lbl)
    auto sldr_i = [&](const char* id, int* v, int mn, int mx, const char* fmt) -> bool {
        const float iw = 52.f;
        char sid[40], eid[40];
        snprintf(sid, sizeof(sid), "###si%s", id);
        snprintf(eid, sizeof(eid), "###ei%s", id);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - iw - 4.f);
        bool c = ImGui::SliderInt(sid, v, mn, mx, fmt);
        ImGui::SameLine(0, 4);
        ImGui::SetNextItemWidth(iw);
        c |= ImGui::InputInt(eid, v, 0, 0);
        if (c) *v = std::max(mn, std::min(mx, *v));
        return c;
    };
    auto sldr_f = [&](const char* id, float* v, float mn, float mx, const char* fmt) -> bool {
        const float iw = 52.f;
        char sid[40], eid[40];
        snprintf(sid, sizeof(sid), "###sf%s", id);
        snprintf(eid, sizeof(eid), "###ef%s", id);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - iw - 4.f);
        bool c = ImGui::SliderFloat(sid, v, mn, mx, fmt);
        ImGui::SameLine(0, 4);
        ImGui::SetNextItemWidth(iw);
        c |= ImGui::InputFloat(eid, v, 0.f, 0.f, "%.1f");
        if (c) *v = std::max(mn, std::min(mx, *v));
        return c;
    };

    // ── Identity ───────────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "IDENTITY");
    tbl_begin();
    lbl("Callsign");
    if (ImGui::InputText("##call", s_callsign, sizeof(s_callsign)) && running)
        apply_settings();
    tbl_end();

    ImGui::Separator();

    // ── Audio (restart required) ───────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "AUDIO");
    ImGui::SameLine(); ImGui::TextDisabled("(restart to apply)");
    tbl_begin();
    {
        lbl("RX Input");
        std::vector<const char*> items;
        for (auto& s : g_ui.input_device_descriptions) items.push_back(s.c_str());
        if (ImGui::Combo("##in", &g_ui.audio_input_index,
                         items.empty() ? nullptr : items.data(), (int)items.size())
            && !running)
            g_ui.audio_input_device = g_ui.available_input_devices[g_ui.audio_input_index];
    }
    {
        lbl("TX Output");
        std::vector<const char*> items;
        for (auto& s : g_ui.output_device_descriptions) items.push_back(s.c_str());
        if (ImGui::Combo("##out", &g_ui.audio_output_index,
                         items.empty() ? nullptr : items.data(), (int)items.size())
            && !running)
            g_ui.audio_output_device = g_ui.available_output_devices[g_ui.audio_output_index];
    }
    tbl_end();
    if (ImGui::SmallButton("Refresh Devices")) refresh_audio_devs();

    ImGui::Separator();

    // ── Modem ──────────────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "MODEM");
    tbl_begin();
    {
        lbl("Modulation");
        std::vector<const char*> opts;
        for (auto& s : MODULATION_OPTIONS) opts.push_back(s.c_str());
        if (ImGui::Combo("##mod", &g_ui.modulation_index, opts.data(), (int)opts.size()))
            apply_settings();
    }
    {
        lbl("Code Rate");
        std::vector<const char*> opts;
        for (auto& s : CODE_RATE_OPTIONS) opts.push_back(s.c_str());
        if (ImGui::Combo("##cr", &g_ui.code_rate_index, opts.data(), (int)opts.size()))
            apply_settings();
    }
    {
        lbl("Frame Size");
        const char* frames[] = {"NORMAL","SHORT"};
        int fi = g_ui.short_frame ? 1 : 0;
        if (ImGui::Combo("##fs", &fi, frames, 2)) {
            g_ui.short_frame = (fi == 1); apply_settings();
        }
    }
    {
        lbl("Center Freq");
        if (sldr_i("cf", &g_ui.center_freq, 300, 3400, "%d Hz"))
            apply_settings();
    }
    tbl_end();

    ImGui::Separator();

    // ── CSMA ───────────────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "CSMA");
    tbl_begin();
    {
        lbl("Enabled");
        if (ImGui::Checkbox("##csma", &g_ui.csma_enabled)) apply_settings();
    }
    if (g_ui.csma_enabled) {
        lbl("Threshold");
        if (sldr_f("thr", &g_ui.carrier_threshold_db, -70.f, 0.f, "%.0f dB"))
            apply_settings();
        lbl("Persistence");
        if (sldr_i("pp", &g_ui.p_persistence, 1, 255, "%d"))
            apply_settings();
        lbl("Slot Time");
        if (sldr_i("sl", &g_ui.slot_time_ms, 50, 2000, "%d ms"))
            apply_settings();
    }
    tbl_end();

    ImGui::Separator();

    // ── Fragmentation ──────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "FRAGMENTATION");
    ImGui::SameLine(); ImGui::TextDisabled("(restart)");
    ImGui::TextDisabled("Both sides must have it enabled");
    tbl_begin();
    {
        lbl("Enabled");
        if (ImGui::Checkbox("##frag", &g_ui.fragmentation_enabled) && running)
            apply_settings();
    }
    tbl_end();

    ImGui::Separator();

    // ── TX Blanking ────────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "TX BLANKING");
    ImGui::SameLine(); ImGui::TextDisabled("(mute RX during TX)");
    tbl_begin();
    {
        lbl("Enabled");
        if (ImGui::Checkbox("##txb", &g_ui.tx_blanking_enabled) && running)
            apply_settings();
    }
    tbl_end();

    ImGui::Separator();

    // ── PTT ────────────────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "PTT");
    tbl_begin();
    {
        lbl("Type");
        std::vector<const char*> opts;
        for (auto& s : PTT_TYPE_OPTIONS) opts.push_back(s.c_str());
        if (ImGui::Combo("##ptt", &g_ui.ptt_type_index, opts.data(), (int)opts.size()))
            apply_settings();
    }
    if (g_ui.ptt_type_index == 1) { // RIGCTL
        lbl("Host");
        if (ImGui::InputText("##rh", s_righost, sizeof(s_righost)) && running)
            apply_settings();
        lbl("Port");
        if (ImGui::InputInt("##rp", &g_ui.rigctl_port, 0) && running)
            apply_settings();
    }
    if (g_ui.ptt_type_index == 2) { // VOX
        lbl("VOX Tone");
        if (sldr_i("vf", &g_ui.vox_tone_freq, 300, 3000, "%d Hz"))
            apply_settings();
        lbl("Lead time");
        if (sldr_i("vl", &g_ui.vox_lead_ms, 0, 2000, "%d ms"))
            apply_settings();
        lbl("Tail time");
        if (sldr_i("vt", &g_ui.vox_tail_ms, 0, 2000, "%d ms"))
            apply_settings();
    }
    if (g_ui.ptt_type_index == 3) { // COM
        // COM port: combo + Refresh button — two widgets on one row
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("COM Port");
        ImGui::SameLine(LBL_W);
        {
            float ref_w = ImGui::CalcTextSize("Refresh").x + 16.f;
            ImGui::SetNextItemWidth(-ref_w - 4.f);
            std::vector<const char*> items;
            for (auto& s : g_com_ports) items.push_back(s.c_str());
            if (ImGui::Combo("##cp", &g_com_port_idx,
                             items.empty() ? nullptr : items.data(), (int)items.size())) {
                strncpy(s_comport, g_com_ports[g_com_port_idx].c_str(), sizeof(s_comport)-1);
                if (running) apply_settings();
            }
            ImGui::SameLine(0,4);
            if (ImGui::SmallButton("Refresh##rcp")) refresh_com_ports();
        }
        lbl("PTT Line");
        {
            const char* lines[] = {"DTR","RTS","BOTH"};
            if (ImGui::Combo("##pl", &g_ui.com_ptt_line, lines, 3) && running)
                apply_settings();
        }
        lbl("Invert");
        {
            int inv = (!g_ui.com_invert_dtr && !g_ui.com_invert_rts) ? 0
                    : ( g_ui.com_invert_dtr && !g_ui.com_invert_rts) ? 1
                    : (!g_ui.com_invert_dtr &&  g_ui.com_invert_rts) ? 2 : 3;
            const char* inverts[] = {"NORMAL","INV DTR","INV RTS","INV BOTH"};
            if (ImGui::Combo("##iv", &inv, inverts, 4) && running) {
                g_ui.com_invert_dtr = (inv == 1 || inv == 3);
                g_ui.com_invert_rts = (inv == 2 || inv == 3);
                apply_settings();
            }
        }
    }
    tbl_end();

    ImGui::Separator();

    // ── Network (restart) ──────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "NETWORK");
    ImGui::SameLine(); ImGui::TextDisabled("(restart)");
    tbl_begin();
    lbl("KISS Port");
    ImGui::InputInt("##kp", &g_ui.port, 0);
    g_ui.port = std::max(1024, std::min(65535, g_ui.port));
    tbl_end();

    ImGui::Spacing();

    // ── Presets ────────────────────────────────────────────────────────────
    ImGui::TextColored({.9f,.8f,.3f,1.f}, "PRESETS");
    ImGui::Separator();

    // Helper: default dir for dialogs
    auto preset_default_dir = []() -> std::string {
        const char* appdata = getenv("APPDATA");
        if (appdata) return std::string(appdata) + "\\modem73";
        return "";
    };

    // Save current settings via native file-save dialog
    if (ImGui::SmallButton("Save to file...")) {
        std::string init_dir = preset_default_dir();
        char szFile[MAX_PATH] = "preset.ini";
        OPENFILENAMEA ofn = {};
        ofn.lStructSize     = sizeof(ofn);
        ofn.lpstrFilter     = "Modem73 Settings\0*.ini\0All Files\0*.*\0";
        ofn.lpstrFile       = szFile;
        ofn.nMaxFile        = sizeof(szFile);
        ofn.lpstrInitialDir = init_dir.empty() ? nullptr : init_dir.c_str();
        ofn.lpstrDefExt     = "ini";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (GetSaveFileNameA(&ofn)) {
            g_ui.callsign    = s_callsign;
            g_ui.com_port    = s_comport;
            g_ui.rigctl_host = s_righost;
            std::string saved_path = g_ui.config_file;
            g_ui.config_file = szFile;
            g_ui.save_settings();
            g_ui.config_file = saved_path;
            g_ui.add_log("Settings saved: " + std::string(szFile));
        }
    }

    ImGui::SameLine();

    // Load settings via native file-open dialog
    if (ImGui::SmallButton("Load from file...")) {
        std::string init_dir = preset_default_dir();
        char szFile[MAX_PATH] = "";
        OPENFILENAMEA ofn = {};
        ofn.lStructSize     = sizeof(ofn);
        ofn.lpstrFilter     = "Modem73 Settings\0*.ini\0All Files\0*.*\0";
        ofn.lpstrFile       = szFile;
        ofn.nMaxFile        = sizeof(szFile);
        ofn.lpstrInitialDir = init_dir.empty() ? nullptr : init_dir.c_str();
        ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            std::string saved_path = g_ui.config_file;
            g_ui.config_file = szFile;
            g_ui.load_settings();
            g_ui.config_file = saved_path;
            sync_buffers_from_ui();
            apply_settings();
            g_ui.add_log("Settings loaded: " + std::string(szFile));
        }
    }
}

// ─── Status tab ─────────────────────────────────────────────────────────────

static void draw_status_tab() {
    float lvl    = g_ui.carrier_level_db.load();
    float thresh = g_ui.carrier_threshold_db;
    bool  busy   = lvl > thresh;

    ImGui::TextDisabled("Signal Level");
    draw_level_bar(lvl, -80.f, 0.f, thresh);
    draw_level_history();

    ImGui::Separator();

    // Carrier / CSMA info row
    ImGui::Text("Carrier: ");
    ImGui::SameLine();
    ImVec4 lvl_col = busy ? ImVec4(1.f,.5f,.1f,1.f) : ImVec4(.3f,1.f,.3f,1.f);
    ImGui::TextColored(lvl_col, "%.1f dB", lvl);
    ImGui::SameLine(0,20);
    ImGui::TextDisabled("Threshold: %.0f dB", thresh);
    ImGui::SameLine(0,20);

    if (g_ui.csma_enabled) {
        if (busy)
            ImGui::TextColored({1.f,.5f,.1f,1.f}, "CSMA: BUSY");
        else
            ImGui::TextColored({.3f,1.f,.3f,1.f}, "CSMA: CLEAR");
    } else {
        ImGui::TextDisabled("CSMA: OFF");
    }

    // Stats row
    ImGui::Text("RX:");
    ImGui::SameLine();
    ImGui::TextColored({.3f,1.f,.4f,1.f}, "%d", (int)g_ui.rx_frame_count);
    ImGui::SameLine(0,14);
    ImGui::Text("TX:");
    ImGui::SameLine();
    ImGui::TextColored({1.f,.6f,.2f,1.f}, "%d", (int)g_ui.tx_frame_count);
    ImGui::SameLine(0,14);
    ImGui::Text("Err:");
    ImGui::SameLine();
    int errs = (int)g_ui.rx_error_count;
    int syncs = (int)g_ui.sync_count;
    if (errs > 0)
        ImGui::TextColored({1.f,.4f,.4f,1.f}, "%d/%d", errs, syncs);
    else
        ImGui::TextColored({.5f,.9f,.5f,1.f}, "0/%d", syncs);
    ImGui::SameLine(0,14);
    ImGui::Text("SNR:");
    ImGui::SameLine();
    float snr = g_ui.last_rx_snr.load();
    ImVec4 snr_col = snr>15.f ? ImVec4(.3f,1.f,.3f,1.f)
                   : snr>5.f  ? ImVec4(.9f,.9f,.2f,1.f)
                               : ImVec4(1.f,.4f,.4f,1.f);
    ImGui::TextColored(snr_col, "%.1f dB", snr);
    ImGui::SameLine(0,14);
    ImGui::Text("Clients: %d", (int)g_ui.client_count);
    ImGui::SameLine(0,14);
    ImGui::Text("Queue: %d", (int)g_ui.tx_queue_size);

    ImGui::Separator();

    // Modem info
    ImGui::TextColored({.6f,.9f,1.f,1.f}, "MODEM INFO");
    ImGui::SameLine(0,10);
    if (g_ui.bitrate_bps >= 1000)
        ImGui::Text("Payload %d B    %.1f kb/s", g_ui.mtu_bytes, g_ui.bitrate_bps/1000.f);
    else
        ImGui::Text("Payload %d B    %d b/s",    g_ui.mtu_bytes, g_ui.bitrate_bps);
    ImGui::SameLine(0,10);
    float tx_time = g_ui.total_tx_time.load();
    if (tx_time < 60)
        ImGui::Text("Frame %.2fs  TX %.0fs", g_ui.airtime_seconds, tx_time);
    else
        ImGui::Text("Frame %.2fs  TX %.1fm", g_ui.airtime_seconds, tx_time/60.f);

    // Audio/PTT status
    ImGui::SameLine(0,20);
    ImGui::Text("Audio:");
    ImGui::SameLine();
    if (g_ui.audio_connected.load())
        ImGui::TextColored({.3f,1.f,.3f,1.f}, "OK");
    else
        ImGui::TextColored({1.f,.3f,.3f,1.f}, "DISCONNECTED");

    ImGui::Separator();

    // Recent packets
    ImGui::TextColored({.6f,.9f,1.f,1.f}, "RECENT ACTIVITY");
    ImGui::BeginChild("##recent", {0, 0}, false);
    auto pkts = g_ui.get_recent_packets();
    if (pkts.empty()) {
        ImGui::TextDisabled("No recent packets...");
    }
    auto now = std::chrono::steady_clock::now();
    for (auto it = pkts.rbegin(); it != pkts.rend(); ++it) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->timestamp).count();
        if (it->is_tx) {
            ImGui::TextColored({1.f,.6f,.2f,1.f}, "TX");
        } else {
            ImGui::TextColored({.3f,1.f,.4f,1.f}, "RX");
        }
        ImGui::SameLine();
        ImGui::Text("%4dB", it->size);
        ImGui::SameLine();
        if (elapsed < 60)
            ImGui::TextDisabled("%lds ago", elapsed);
        else
            ImGui::TextDisabled("%ldm ago", elapsed/60);
        if (!it->is_tx && it->snr > 0) {
            ImGui::SameLine();
            ImGui::TextColored({.7f,.9f,1.f,1.f}, "%.0f dB SNR", it->snr);
        }
    }
    ImGui::EndChild();
}

// ─── Log tab ─────────────────────────────────────────────────────────────────

static void draw_log_tab() {
    static bool auto_scroll = true;
    ImGui::Checkbox("Auto-scroll", &auto_scroll);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        std::lock_guard<std::mutex> lk(g_ui.log_mutex);
        g_ui.log_entries.clear();
    }
    ImGui::Separator();

    ImGui::BeginChild("##logscroll", {0,0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    auto logs = g_ui.get_log();
    for (auto& line : logs) {
        bool err = line.find("error") != std::string::npos
                || line.find("Error") != std::string::npos
                || line.find("CRC")   != std::string::npos
                || line.find("fail")  != std::string::npos
                || line.find("FAIL")  != std::string::npos;
        bool rx  = line.find("Decoded") != std::string::npos
                || line.find("Frame")   != std::string::npos
                || line.find("Sync")    != std::string::npos
                || line.find("Client connected") != std::string::npos;
        bool tx  = line.find("TX")   != std::string::npos
                || line.find("Sent") != std::string::npos;

        ImGui::PushStyleColor(ImGuiCol_Text,
            err ? ImVec4(1.f,.4f,.4f,1.f)
                : rx  ? ImVec4(.4f,1.f,.5f,1.f)
                : tx  ? ImVec4(.9f,.8f,.2f,1.f)
                      : ImVec4(.85f,.85f,.85f,1.f));
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

// ─── Utils tab ───────────────────────────────────────────────────────────────

static void draw_utils_tab() {
    ImGui::TextColored({.6f,.9f,1.f,1.f}, "ACTIONS");
    ImGui::Separator();

    bool can_send = g_tnc_running && g_ui.on_send_data;

    auto& test_size = g_ui.random_data_size;
    if (test_size <= 0) test_size = g_ui.mtu_bytes > 0 ? g_ui.mtu_bytes : 256;

    if (!can_send) { ImGui::BeginDisabled(); }

    if (ImGui::Button("1. Send Test Pattern (0x55)")) {
        std::vector<uint8_t> data(test_size, 0x55);
        g_ui.on_send_data(data);
        g_ui.add_log("Sent test pattern (" + std::to_string(test_size) + " bytes)");
    }
    if (ImGui::Button("2. Send Random Data")) {
        std::vector<uint8_t> data(test_size);
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> dis(0,255);
        for (auto& b : data) b = (uint8_t)dis(gen);
        g_ui.on_send_data(data);
        g_ui.add_log("Sent random data (" + std::to_string(test_size) + " bytes)");
    }
    if (ImGui::Button("3. Send Ping")) {
        std::string ping = "PING:" + g_ui.callsign;
        std::vector<uint8_t> data(ping.begin(), ping.end());
        g_ui.on_send_data(data);
        g_ui.add_log("Sent ping");
    }

    if (!can_send) { ImGui::EndDisabled(); }

    ImGui::Spacing();

    if (ImGui::Button("4. Clear Stats")) {
        g_ui.rx_frame_count = 0;
        g_ui.tx_frame_count = 0;
        g_ui.rx_error_count = 0;
        g_ui.sync_count     = 0;
        g_ui.preamble_errors= 0;
        g_ui.symbol_errors  = 0;
        g_ui.crc_errors     = 0;
        g_ui.stats_reset_requested = true;
        g_ui.total_tx_time  = 0.f;
        g_ui.add_log("Stats cleared");
    }

    // Auto threshold calibration
    if (g_calibrating) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - g_calib_start).count();
        float lvl = g_ui.carrier_level_db.load();
        if (lvl > g_calib_max) g_calib_max = lvl;
        if (elapsed >= 3.f) {
            g_ui.carrier_threshold_db = g_calib_max + 3.f;
            g_calibrating = false;
            apply_settings();
            g_ui.add_log("Threshold set to " + std::to_string((int)g_ui.carrier_threshold_db) + " dB");
        }
        ImGui::TextColored({.9f,.9f,.2f,1.f}, "5. Calibrating... %.0fs / 3s  max=%.0f dB",
                           elapsed, g_calib_max);
    } else {
        if (ImGui::Button("5. Auto Threshold (3s listen)")) {
            g_calibrating = true;
            g_calib_max   = -100.f;
            g_calib_start = std::chrono::steady_clock::now();
            g_ui.add_log("Calibrating threshold (3s)...");
        }
    }

    if (!g_tnc_running) { ImGui::BeginDisabled(); }
    if (ImGui::Button("6. Reconnect Audio") && g_ui.on_reconnect_audio) {
        g_ui.add_log("Reconnecting audio...");
        bool ok = g_ui.on_reconnect_audio();
        g_ui.audio_connected = ok;
        g_ui.add_log(ok ? "Audio reconnected OK" : "Audio reconnect FAILED");
    }
    if (!g_tnc_running) { ImGui::EndDisabled(); }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored({.6f,.9f,1.f,1.f}, "TEST INFO");

    ImGui::Text("MTU:"); ImGui::SameLine();
    ImGui::Text("%d bytes", g_ui.mtu_bytes);
    if (g_ui.fragmentation_enabled) {
        ImGui::SameLine();
        ImGui::TextColored({.9f,.7f,.2f,1.f}, "[FRAG enabled]");
    }

    int max_size = g_ui.fragmentation_enabled ? 65535 : std::max(1, g_ui.mtu_bytes);
    if (test_size > max_size) test_size = max_size;
    if (test_size < 1) test_size = 1;
    ImGui::SetNextItemWidth(200);
    ImGui::SliderInt("Test Size [bytes]##ts", &test_size, 1, max_size);

    if (g_ui.fragmentation_enabled && test_size > g_ui.mtu_bytes && g_ui.mtu_bytes > 5) {
        int frags = (test_size + g_ui.mtu_bytes - 6) / (g_ui.mtu_bytes - 5);
        ImGui::SameLine();
        ImGui::TextColored({.9f,.8f,.2f,1.f}, "(%d fragments)", frags);
    }

    ImGui::TextDisabled("Pattern: 0x55 (alternating bits)");
    ImGui::TextDisabled("Frames TX: %d", (int)g_ui.tx_frame_count);
}

// ─── Constellation tab ───────────────────────────────────────────────────────

static void draw_constellation_tab() {
    // Constellation name
    const char* mod_name = "---";
    switch (g_ui.constellation_mod_bits) {
        case 1:  mod_name = "BPSK";    break;
        case 2:  mod_name = "QPSK";    break;
        case 3:  mod_name = "8PSK";    break;
        case 4:  mod_name = "QAM16";   break;
        case 6:  mod_name = "QAM64";   break;
        case 8:  mod_name = "QAM256";  break;
        case 10: mod_name = "QAM1024"; break;
        case 12: mod_name = "QAM4096"; break;
    }
    ImGui::TextColored({.6f,.9f,1.f,1.f}, "Constellation  [%s]", mod_name);
    draw_constellation();

    ImGui::Spacing();

    // SNR history
    {
        std::lock_guard<std::mutex> lk(g_ui.snr_mutex);
        if (g_ui.snr_history_count > 0) {
            ImGui::TextDisabled("SNR history (last %d packets)", g_ui.snr_history_count);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(.9f,.7f,.1f,1.f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,   ImVec4(.05f,.05f,.05f,1.f));
            ImGui::PlotLines("##snr", g_ui.snr_history, TNCUIState::SNR_HISTORY_SIZE,
                             g_ui.snr_history_pos, nullptr, 0.f, 40.f,
                             {220.f, 55.f});
            ImGui::PopStyleColor(2);
        } else {
            ImGui::TextDisabled("(no SNR data yet)");
        }
    }
}

// ─── Main render ─────────────────────────────────────────────────────────────

static void render_gui() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings);

    bool connected = g_tnc_running.load();

    // ── Top bar ───────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.3f,1.f,.4f,1.f));
    ImGui::Text("  MODEM73  -  OFDM KISS TNC  -  TCP port %d", g_ui.port);
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 30);

    // START / STOP button
    ImGui::PushStyleColor(ImGuiCol_Button,
        connected ? ImVec4(.65f,.1f,.1f,1.f) : ImVec4(.1f,.55f,.1f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        connected ? ImVec4(.85f,.2f,.2f,1.f) : ImVec4(.2f,.75f,.2f,1.f));
    if (ImGui::Button(connected ? "  STOP  " : "  START ", {100, 24})) {
        if (connected) stop_tnc(); else start_tnc();
        connected = g_tnc_running.load();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    const char* status_txt = !connected          ? "STOP"
                           : (bool)g_ui.transmitting ? "TX   ^"
                           : (bool)g_ui.receiving    ? "SYNC +"
                           :                           "Listen...";
    ImVec4 led = !connected          ? ImVec4(.5f,.5f,.5f,1.f)
               : (bool)g_ui.transmitting ? ImVec4(1.f,.5f,.1f,1.f)
               : (bool)g_ui.receiving    ? ImVec4(.2f,1.f,.3f,1.f)
               :                          ImVec4(.3f,.8f,.3f,1.f);
    ImGui::TextColored(led, "  * %s", status_txt);

    ImGui::Separator();

    // ── Left panel: settings ──────────────────────────────────────────────
    ImGui::BeginChild("##cfg", {360.f, 0.f}, true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6, 4});
    draw_settings_panel();
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Right panel: tabs ─────────────────────────────────────────────────
    ImGui::BeginChild("##right", {0.f, 0.f}, false);

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Status")) {
            draw_status_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Log")) {
            draw_log_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Utils")) {
            draw_utils_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Constellation")) {
            draw_constellation_tab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
    ImGui::End();
}

// ─── WinMain ─────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    // Default config dir: %APPDATA%\modem73\  (proper Windows path, not MSYS2 POSIX)
    std::string auto_config;
    {
        const char* appdata = getenv("APPDATA");
        if (appdata) {
            std::string config_dir = std::string(appdata) + "\\modem73";
            CreateDirectoryA(config_dir.c_str(), nullptr);
            auto_config           = config_dir + "\\settings.ini";
            g_ui.presets_file     = config_dir + "\\presets.ini";
        }
    }

    // Command-line: --config <file>  overrides automatic settings file
    // Usage: modem73_gui.exe --config C:\path\to\preset.ini
    std::string cli_config;
    for (int i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "--config") == 0) {
            cli_config = argv[i + 1];
            break;
        }
    }

    // Apply: CLI config if given, else last-used auto config
    g_ui.config_file = cli_config.empty() ? auto_config : cli_config;

    // Load settings and presets
    g_ui.load_settings();
    g_ui.load_presets();

    // Auto-save always goes to the automatic settings file (not the CLI-supplied config)
    if (!cli_config.empty())
        g_ui.config_file = auto_config;

    // Populate audio device lists
    refresh_audio_devs();

    // Match saved device names to indices
    for (size_t i = 0; i < g_ui.available_input_devices.size(); i++) {
        if (g_ui.available_input_devices[i] == g_ui.audio_input_device) {
            g_ui.audio_input_index = (int)i; break;
        }
    }
    for (size_t i = 0; i < g_ui.available_output_devices.size(); i++) {
        if (g_ui.available_output_devices[i] == g_ui.audio_output_device) {
            g_ui.audio_output_index = (int)i; break;
        }
    }

    // Fix Linux-style COM port default on Windows
    if (g_ui.com_port.substr(0, 5) == "/dev/")
        g_ui.com_port = "COM3";

    // Sync buffers from saved settings
    sync_buffers_from_ui();
    g_ui.update_modem_info();

    // Select second device by default if multiple exist (skip "default")
    if (g_ui.audio_input_index == 0 && g_ui.available_input_devices.size() > 1)
        g_ui.audio_input_index = 1;
    if (g_ui.audio_output_index == 0 && g_ui.available_output_devices.size() > 1)
        g_ui.audio_output_index = 1;

    // Enumerate COM ports
    refresh_com_ports();

    // GLFW
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1100, 740, "modem73  -  OFDM TNC", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Tab navigation

    // Load Arial with extended Latin (covers Polish diacritics: ą ć ę ł ń ó ś ź ż)
    static const ImWchar latin_ext_ranges[] = { 0x0020, 0x017F, 0 };
    bool font_loaded = false;
    const char* font_paths[] = {
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf",
        nullptr
    };
    for (int i = 0; font_paths[i]; i++) {
        if (io.Fonts->AddFontFromFileTTF(font_paths[i], 15.f, nullptr, latin_ext_ranges)) {
            font_loaded = true;
            break;
        }
    }
    if (!font_loaded)
        io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    ImGuiStyle& sty = ImGui::GetStyle();
    sty.WindowRounding = 4; sty.FrameRounding = 3; sty.GrabRounding = 3;
    sty.ItemSpacing    = {8,5}; sty.FramePadding = {6,3};
    sty.TabRounding    = 3;

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Auto-start TNC on launch
    start_tnc();

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_gui();

        ImGui::Render();
        int fw, fh; glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(.09f, .09f, .11f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    stop_tnc();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
