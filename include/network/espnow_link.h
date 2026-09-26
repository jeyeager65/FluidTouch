#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "network/espnow_radio.h"

constexpr size_t ESPNOW_HOSTNAME_SIZE = 32;

// What a FluidTouch machine slot stores for an ESP-NOW connection
struct EspNowPairing {
    uint8_t mac[6];                      // FluidNC's MAC (differs between its AP and STA modes)
    uint8_t lmk[16];                     // Per-pairing ESP-NOW key
    uint8_t channel;                     // Last WiFi channel FluidNC was found on
    char hostname[ESPNOW_HOSTNAME_SIZE]; // FluidNC hostname, for display
};

// Client side of the FluidNC ESP-NOW pendant protocol (FluidNC v4.0.4+):
// pairing, session keepalives, anti-replay tags, message fragmentation and
// realtime commands. Presents the connection as a byte stream.
class EspNowLink {
public:
    enum class State : uint8_t {
        Unpaired,       // No pairing and not pairing
        Discovering,    // Pairing: broadcasting on each channel for FluidNC
        Confirming,     // Pairing: key exchange with a FluidNC that answered
        Searching,      // Paired: looking for FluidNC across channels
        Synchronizing,  // Paired: FluidNC found, agreeing on session nonces
        Connected
    };

    explicit EspNowLink(EspNowRadio& radio) : _radio(radio) {}

    bool begin();
    void poll(uint32_t now_ms);

    // Connect using a saved pairing, or forget it
    void setPairing(const EspNowPairing& pairing);
    void clearPairing();
    bool hasPairing() const { return _hasPairing; }
    const EspNowPairing& pairing() const { return _pairing; }
    // True once after pairing completes or FluidNC moves channel: save pairing()
    bool pairingChanged();

    // Pairing needs $espnow/pair run on FluidNC within its 60 second window
    void startPairing();
    void cancelPairing();

    State state() const { return _state; }
    static const char* stateName(State state);
    bool connected() const { return _state == State::Connected; }
    uint8_t channel() const { return _channel; }
    int8_t rssi() const { return _rssi; }

    // Lines are sent when their '\n' arrives. Realtime bytes (reset, ?, !, ~
    // and 0x80-0xBF) at the start of a line go out immediately on their own.
    void write(const uint8_t* data, size_t len);
    void writeRealtime(uint8_t c);
    int read();  // -1 when nothing is buffered

private:
    struct ReplayWindow {
        uint32_t nonce = 0;
        uint32_t top = 0;
        uint64_t seen = 0;
    };

    static constexpr size_t SESSION_ID_SIZE = 16;
    static constexpr size_t PAIR_PACKET_SIZE = 41;  // Confirm and Complete packets
    static constexpr size_t DATA_CHUNK = ESPNOW_MAX_PAYLOAD - 12;
    static constexpr size_t MAX_CHUNKS = 8;

    EspNowRadio& _radio;
    bool _started = false;
    State _state = State::Unpaired;
    uint32_t _now = 0;
    uint32_t _stateSince = 0;
    uint8_t _mac[6] = {};
    uint8_t _channel = 1;
    int8_t _rssi = -127;

    EspNowPairing _pairing = {};
    bool _hasPairing = false;
    bool _pairingChanged = false;

    // Pairing handshake
    uint8_t _pairPrivate[32] = {};
    uint8_t _pairPublic[65] = {};
    uint8_t _sessionId[SESSION_ID_SIZE] = {};
    uint8_t _pairLmk[16] = {};
    uint8_t _confirmPacket[PAIR_PACKET_SIZE] = {};
    uint8_t _completePacket[PAIR_PACKET_SIZE] = {};
    EspNowPairing _newPairing = {};
    uint32_t _pairingStarted = 0;
    uint32_t _lastDiscovery = 0;
    uint8_t _discoveryIndex = 0;
    uint32_t _confirmStarted = 0;
    uint32_t _lastConfirm = 0;
    uint8_t _completeSends = 0;
    uint32_t _lastComplete = 0;

    // Session
    uint32_t _rxNonce = 0;        // Our challenge: FluidNC stamps its frames with it
    uint32_t _peerNonce = 0;      // FluidNC's challenge: we stamp our frames with it
    bool _peerNonceKnown = false;
    uint32_t _txCounter = 0;
    ReplayWindow _replay;
    bool _syncAuthSent = false;
    uint32_t _lastRx = 0;
    uint32_t _lastKeepalive = 0;
    uint32_t _lastSearch = 0;
    uint8_t _searchTries = 0;

    // Outgoing line and fragment sequence
    std::string _txLine;
    uint8_t _txSeq = 0;

    // Incoming message reassembly and byte stream
    uint8_t _rxChunks[MAX_CHUNKS][DATA_CHUNK];
    uint8_t _rxChunkLen[MAX_CHUNKS] = {};
    uint8_t _rxGot = 0;
    uint8_t _rxTotal = 0;
    uint8_t _rxSeq = 0;
    bool _rxPending = false;
    uint32_t _rxStarted = 0;
    std::string _rx;
    size_t _rxPos = 0;

    void setState(State state);
    void resetSession();
    void clearHandshake();
    void restorePairing();
    void beginSearching();
    void beginSynchronizing();
    void setConnected();
    void activatePairing();

    void sendDiscovery();
    bool sendKeepalive();
    void sendMessage(const uint8_t* data, size_t len);
    void stampTag(uint8_t* out);
    bool acceptReplay(uint32_t nonce, uint32_t counter);

    void handleFrame(const EspNowFrame& frame);
    void handleChallenge(const EspNowFrame& frame);
    void handleResult(const EspNowFrame& frame);
    void handleKeepalive(const EspNowFrame& frame);
    void handleData(const EspNowFrame& frame);
};

#endif  // ESPNOW_LINK_H
