// FluidTouch simulator entry point.
//
// Runs the firmware's own setup()/loop() from src/main.cpp unchanged. This
// file only provides main(), the simulator data directory, restart/deep-sleep
// emulation, screenshots (F12) and a tiny input script runner (--script) for
// automated screenshots / smoke tests.

#include <Arduino.h>
#include <lvgl.h>
#include <SDL2/SDL.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
// Arduino.h (force-included) defines INPUT/OUTPUT, which clash with windows.h
#undef INPUT
#undef OUTPUT
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void setup();
void loop();

namespace sim {

// pc_keyboard.cpp
void pcKeyboardOnSdlEvent(const SDL_Event *e);
void pcKeyboardProcess();
void pcKeyboardQueueText(const std::string &text);
bool pcKeyboardQueueKey(const std::string &name);

int g_argc = 0;
char **g_argv = nullptr;

// Scripted touch state, read by touch_driver_sim.cpp
bool g_touch_override = false;
bool g_touch_pressed = false;
int g_touch_x = 0;
int g_touch_y = 0;

namespace {
std::string g_data_dir;
bool g_screenshot_requested = false;

std::string exePath(const char *argv0) {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) return buf;
#endif
    std::error_code ec;
    auto p = std::filesystem::absolute(argv0, ec);
    return ec ? std::string(argv0) : p.string();
}

// ---------------------------------------------------------------------------
// Screenshots: read LVGL's full-frame buffer (DIRECT render mode), which
// already includes the top/system layers (popups, backlight overlay).
// ---------------------------------------------------------------------------
bool saveScreenshot(std::string path) {
    lv_display_t *disp = lv_display_get_default();
    if (!disp) return false;
    lv_refr_now(disp);
    lv_draw_buf_t *buf = lv_display_get_buf_active(disp);
    if (!buf || !buf->data) return false;
    int w = lv_display_get_horizontal_resolution(disp);
    int h = lv_display_get_vertical_resolution(disp);
    uint32_t stride = buf->header.stride;

    if (path.empty()) {
        char name[64];
        time_t now = time(nullptr);
        struct tm tmv;
        localtime_r(&now, &tmv);
        strftime(name, sizeof(name), "fluidtouch_%Y%m%d_%H%M%S.bmp", &tmv);
        path = (std::filesystem::path(g_data_dir) / "screenshots" / name).string();
    }
    std::error_code ec;
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);

    // 24-bit bottom-up BMP
    uint32_t rowBytes = (uint32_t)((w * 3 + 3) & ~3);
    uint32_t dataSize = rowBytes * h;
    uint8_t hdr[54] = {'B', 'M'};
    auto put32 = [&](int off, uint32_t v) { for (int i = 0; i < 4; i++) hdr[off + i] = uint8_t(v >> (8 * i)); };
    put32(2, 54 + dataSize);
    put32(10, 54);
    put32(14, 40);
    put32(18, (uint32_t)w);
    put32(22, (uint32_t)h);
    hdr[26] = 1;
    hdr[28] = 24;
    put32(34, dataSize);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<char *>(hdr), sizeof(hdr));
    std::vector<uint8_t> row(rowBytes, 0);
    for (int y = h - 1; y >= 0; y--) {
        const uint8_t *src = buf->data + (size_t)y * stride;
        for (int x = 0; x < w; x++) {
#if LV_COLOR_DEPTH == 16
            uint16_t px = (uint16_t)(src[x * 2] | (src[x * 2 + 1] << 8));
            uint8_t r = (px >> 11) & 0x1F, g = (px >> 5) & 0x3F, b = px & 0x1F;
            row[x * 3 + 0] = uint8_t((b << 3) | (b >> 2));
            row[x * 3 + 1] = uint8_t((g << 2) | (g >> 4));
            row[x * 3 + 2] = uint8_t((r << 3) | (r >> 2));
#else
            const int bpp = LV_COLOR_DEPTH / 8;
            row[x * 3 + 0] = src[x * bpp + 0];
            row[x * 3 + 1] = src[x * bpp + 1];
            row[x * 3 + 2] = src[x * bpp + 2];
#endif
        }
        f.write(reinterpret_cast<char *>(row.data()), rowBytes);
    }
    printf("[SIM] Screenshot saved: %s\n", path.c_str());
    return true;
}

int SDLCALL eventWatch(void *, SDL_Event *e) {
    pcKeyboardOnSdlEvent(e);
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F12 && !e->key.repeat) {
        g_screenshot_requested = true;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// --script: semicolon/newline separated commands, run in order:
//   wait <ms>            pause
//   click <x> <y>        tap (press 100ms, release)
//   press <x> <y>        finger down (drag by pressing again elsewhere)
//   release              finger up
//   type <text>          type into the field the on-screen keyboard targets
//   key <name>           enter, esc, backspace, left, right
//   shot [file.bmp]      screenshot (relative paths go under <data>/screenshots)
//   exit                 quit the simulator
// ---------------------------------------------------------------------------
struct ScriptCmd {
    std::string op;
    std::vector<std::string> args;
};
std::vector<ScriptCmd> g_script;
size_t g_script_pos = 0;
unsigned long g_script_next_ms = 0;

void loadScript(const std::string &spec) {
    std::string text = spec;
    std::ifstream f(spec);
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        text = ss.str();
    }
    for (char &c : text) if (c == ';') c = '\n';
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        ScriptCmd cmd;
        if (!(words >> cmd.op)) continue;
        std::string a;
        while (words >> a) cmd.args.push_back(a);
        g_script.push_back(cmd);
    }
}

void runScript() {
    unsigned long now = millis();
    while (g_script_pos < g_script.size() && now >= g_script_next_ms) {
        const ScriptCmd &c = g_script[g_script_pos++];
        auto arg = [&](size_t i) { return i < c.args.size() ? std::atoi(c.args[i].c_str()) : 0; };
        if (c.op == "wait") {
            g_script_next_ms = now + arg(0);
        } else if (c.op == "click" || c.op == "press") {
            g_touch_override = true;
            g_touch_pressed = true;
            g_touch_x = arg(0);
            g_touch_y = arg(1);
            if (c.op == "click") {
                g_script.insert(g_script.begin() + g_script_pos, {ScriptCmd{"release", {}}});
                g_script_next_ms = now + 100;
            } else {
                g_script_next_ms = now + 50;
            }
        } else if (c.op == "release") {
            g_touch_pressed = false;
            g_script_next_ms = now + 50;
        } else if (c.op == "type") {
            std::string text;
            for (size_t i = 0; i < c.args.size(); i++) text += (i ? " " : "") + c.args[i];
            pcKeyboardQueueText(text);
            g_script_next_ms = now + 50;
        } else if (c.op == "key") {
            if (c.args.empty() || !pcKeyboardQueueKey(c.args[0])) printf("[SIM] Unknown key in script\n");
            g_script_next_ms = now + 50;
        } else if (c.op == "shot") {
            std::string p = c.args.empty() ? "" : c.args[0];
            if (!p.empty() && std::filesystem::path(p).is_relative()) {
                p = (std::filesystem::path(g_data_dir) / "screenshots" / p).string();
            }
            saveScreenshot(p);
        } else if (c.op == "exit") {
            printf("[SIM] Script finished - exiting\n");
            SDL_Quit();
            exit(0);
        } else {
            printf("[SIM] Unknown script command: %s\n", c.op.c_str());
        }
    }
    if (g_script_pos >= g_script.size() && !g_touch_pressed) g_touch_override = false;
}

void delayHook() {
    SDL_PumpEvents();
    if (g_screenshot_requested) {
        g_screenshot_requested = false;
        saveScreenshot("");
    }
    if (!g_script.empty()) runScript();
    pcKeyboardProcess();
}

void printUsage() {
    printf("FluidTouch simulator\n\n"
           "Usage: fluidtouch_sim [--data <dir>] [--script <file or commands>]\n\n"
           "  --data <dir>     Where to keep simulated NVS settings (preferences.json)\n"
           "                   and the display SD card (sd/). Default: %s\n"
           "  --script <s>     Run scripted input, e.g. \"wait 3000; click 400 240; shot a.bmp; exit\"\n"
           "                   Commands: wait <ms>, click <x> <y>, press <x> <y>, release,\n"
           "                   type <text>, key <enter|esc|backspace|left|right>,\n"
           "                   shot [file.bmp], exit\n\n"
           "Mouse = touch. While an on-screen keyboard is open, the PC keyboard types\n"
           "into it (Enter = OK, Esc = close). F12 = save screenshot to <data>/screenshots\n"
           "Type a line in this console to send it to FluidNC (like the serial monitor).\n",
           SIM_DEFAULT_DATA_DIR);
}
} // namespace

const char *dataDir() { return g_data_dir.c_str(); }

void shutdown() {
    SDL_Quit();
}

[[noreturn]] void deepSleep() {
    printf("\n[SIM] esp_deep_sleep_start() - device powered off, exiting simulator\n");
    fflush(stdout);
    shutdown();
    exit(0);
}

} // namespace sim

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);

    sim::g_data_dir = SIM_DEFAULT_DATA_DIR;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if ((a == "--data" || a == "-d") && i + 1 < argc) {
            sim::g_data_dir = argv[++i];
        } else if (a == "--script" && i + 1 < argc) {
            sim::loadScript(argv[++i]);
        } else if (a == "--help" || a == "-h") {
            sim::printUsage();
            return 0;
        } else {
            printf("Unknown argument: %s\n\n", argv[i]);
            sim::printUsage();
            return 1;
        }
    }

    // Arguments for ESP.restart() to relaunch with: absolute exe path, and
    // without --script (a reboot shouldn't replay the input script).
    static std::string self = sim::exePath(argv[0]);
    static std::vector<char *> restartArgs;
    restartArgs.push_back(self.data());
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--script") { i++; continue; }
        restartArgs.push_back(argv[i]);
    }
    restartArgs.push_back(nullptr);
    sim::g_argc = (int)restartArgs.size() - 1;
    sim::g_argv = restartArgs.data();

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(sim::g_data_dir) / "sd", ec);
    printf("[SIM] Data directory: %s\n", sim::g_data_dir.c_str());

    SDL_SetMainReady();

    sim::delay_hook = sim::delayHook;

    setup();
    SDL_AddEventWatch(sim::eventWatch, nullptr);  // after setup() - SDL is initialised by then

    while (true) {
        loop();
    }
}
