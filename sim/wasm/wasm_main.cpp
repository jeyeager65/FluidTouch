// FluidTouch in the browser (Emscripten): the wasm counterpart of
// sim/src/sim_main.cpp. Runs the firmware's own setup()/loop() unchanged on a
// pthread (-sPROXY_TO_PTHREAD), so delay() and the blocking WebSocket reads
// work as on the desktop. The display, touch and network are the page's
// (display_touch_wasm.cpp, net_wasm.cpp).
//
// Data directory /data (MEMFS: settings live until the page reloads). On the
// first boot it gets one machine, "LowRider (browser)", a WiFi machine at
// FluidNC's virtual address, selected and loaded without the machine list.

#include <Arduino.h>
#include <Preferences.h>
#include <emscripten.h>

#include <filesystem>

#include "config.h"
#include "ui/machine_config.h"

void setup();
void loop();

namespace sim {

const char *dataDir() { return "/data"; }

void shutdown() {}

[[noreturn]] void deepSleep() {
    printf("\n[SIM] esp_deep_sleep_start() - device powered off\n");
    MAIN_THREAD_ASYNC_EM_ASM({ if (Module.onPowerOff) Module.onPowerOff(); });
    while (true) delay(1000);
}

// ESP.restart(): the page starts a new instance (Module.onRestart)
[[noreturn]] void restart() {
    printf("\n[SIM] ESP.restart() - the page restarts FluidTouch\n");
    MAIN_THREAD_ASYNC_EM_ASM({ if (Module.onRestart) Module.onRestart(); });
    while (true) delay(1000);
}

namespace {
void addBrowserMachine() {
    if (MachineConfigManager::hasConfiguredMachines()) return;
    MachineConfig m;
    strcpy(m.name, "LowRider (browser)");
    m.connection_type = CONN_WIFI;
    strcpy(m.ssid, "browser");
    strcpy(m.fluidnc_url, "192.168.0.1");  // FluidNC wasm's VirtualNet address
    m.websocket_port = 80;
    m.is_configured = true;
    MachineConfigManager::saveMachine(0, m);
    MachineConfigManager::setSelectedMachineIndex(0);

    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, false);
    prefs.putBool("show_mach_sel", false);
    prefs.end();
    printf("[SIM] Added machine '%s'\n", m.name);
}
} // namespace

} // namespace sim

int main() {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(sim::dataDir()) / "sd", ec);
    sim::addBrowserMachine();

    setup();
    while (true) {
        loop();
    }
}
