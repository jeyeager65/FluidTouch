// Console test for the ESP-NOW link against sim/tools/fake_fluidnc.py:
//
//   python sim/tools/fake_fluidnc.py --espnow-pair
//   sim/build/espnow_link_test --pair      (pairs, saves the pairing, talks)
//   sim/build/espnow_link_test             (reconnects with the saved pairing)
//
// Options: --pair, --store <file> (default sim/data/espnow_test_pairing.bin),
// --seconds <n> (default 30). Exits 0 once a status report, a $G reply and
// an ok have all come back over the link. With --hold it keeps running for
// the full time (e.g. to restart the fake server underneath it) and exits 0
// if it is connected and receiving status reports at the end.

#include <Arduino.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "network/espnow_link.h"

bool g_serialMuted = false;

namespace sim {
int g_argc = 0;
char** g_argv = nullptr;
void shutdown() { exit(0); }
}  // namespace sim

static bool loadPairing(const std::string& path, EspNowPairing& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    bool ok = fread(&out, sizeof(out), 1, f) == 1;
    fclose(f);
    return ok;
}

static void savePairing(const std::string& path, const EspNowPairing& pairing) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        fwrite(&pairing, sizeof(pairing), 1, f);
        fclose(f);
    }
}

int main(int argc, char** argv) {
    sim::g_argc = argc;
    sim::g_argv = argv;
    bool pair = false;
    bool hold = false;
    std::string store = SIM_DEFAULT_DATA_DIR "/espnow_test_pairing.bin";
    int seconds = 30;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--pair") {
            pair = true;
        } else if (a == "--hold") {
            hold = true;
        } else if (a == "--store" && i + 1 < argc) {
            store = argv[++i];
        } else if (a == "--seconds" && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        }
    }
    setvbuf(stdout, nullptr, _IONBF, 0);

    EspNowLink link(espnowPlatformRadio());
    if (!link.begin()) {
        return 2;
    }
    EspNowPairing saved;
    if (!pair && loadPairing(store, saved)) {
        printf("Using saved pairing for %s, channel %u\n", saved.hostname, saved.channel);
        link.setPairing(saved);
    } else {
        printf("Pairing: run $espnow/pair on FluidNC (or start the fake server with --espnow-pair)\n");
        link.startPairing();
    }

    bool sent = false, gotStatus = false, gotParser = false, gotOk = false;
    uint32_t lastStatus = 0;
    std::string line;
    uint32_t end = millis() + (uint32_t)seconds * 1000;
    while (millis() < end) {
        link.poll(millis());
        if (link.pairingChanged()) {
            savePairing(store, link.pairing());
            printf("Saved pairing (%s, channel %u) to %s\n", link.pairing().hostname, link.pairing().channel,
                   store.c_str());
        }
        if (!link.connected()) {
            sent = false;  // Ask again after a reconnect
        } else if (!sent) {
            const char* cmd = "$G\n";
            link.write((const uint8_t*)cmd, strlen(cmd));
            link.writeRealtime('?');
            sent = true;
        }
        for (int c; (c = link.read()) >= 0;) {
            if (c != '\n') {
                line.push_back((char)c);
                continue;
            }
            bool status = line.rfind("<", 0) == 0;
            if (!(hold && status)) {
                printf("  <- %s\n", line.c_str());
            }
            if (status) {
                lastStatus = millis();
            }
            gotStatus |= status;
            gotParser |= line.rfind("[GC:", 0) == 0;
            gotOk |= line == "ok";
            line.clear();
        }
        if (!hold && gotStatus && gotParser && gotOk) {
            printf("PASS: status report, $G reply and ok received (channel %u)\n", link.channel());
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (hold && link.connected() && millis() - lastStatus < 1000) {
        printf("PASS: connected on channel %u and receiving status reports\n", link.channel());
        return 0;
    }
    printf("FAIL: state %s, status=%d $G=%d ok=%d\n", EspNowLink::stateName(link.state()), gotStatus, gotParser,
           gotOk);
    return 1;
}
