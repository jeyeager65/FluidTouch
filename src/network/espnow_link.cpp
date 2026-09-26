#include "network/espnow_link.h"

#include <stddef.h>
#include <string.h>

#include "debug_log.h"
#include "network/espnow_crypto.h"

using EspNowCrypto::Bytes;

namespace {

// Packet types
constexpr uint8_t PKT_DISCOVERY = 0x01;
constexpr uint8_t PKT_CHALLENGE = 0x02;
constexpr uint8_t PKT_DATA = 0x03;
constexpr uint8_t PKT_CONFIRM = 0x04;
constexpr uint8_t PKT_REALTIME = 0x05;
constexpr uint8_t PKT_KEEPALIVE = 0x06;
constexpr uint8_t PKT_RESULT = 0x07;
constexpr uint8_t PKT_COMPLETE = 0x08;

constexpr uint8_t PROTOCOL_VERSION = 4;
constexpr uint8_t MODE_PAIR = 1;

// DATA:      type, nonce(4), counter(4), sequence, chunk index, chunk count, payload
// REALTIME:  type, nonce(4), counter(4), command
// KEEPALIVE: type, our challenge(4) [, nonce(4), counter(4), flags]
constexpr size_t DATA_HEADER_SIZE = 12;
constexpr size_t REALTIME_SIZE = 10;
constexpr size_t PLAIN_KEEPALIVE_SIZE = 5;
constexpr size_t AUTH_KEEPALIVE_SIZE = 14;
constexpr uint8_t KEEPALIVE_CONFIRMED = 0x01;

constexpr const char* PMK_LABEL = "fluiddial-espnow-pmk-v1";
constexpr const char* WINDOW_LABEL = "fluidnc-espnow-pairing-window-v1";
constexpr const char* SESSION_LABEL = "fluidnc-espnow-pairing-session-v1";

// Timing (ms). FluidNC drops a pendant after 10 s of silence and keeps a
// pairing window open for 60 s.
constexpr uint32_t DISCOVERY_INTERVAL = 250;
constexpr uint32_t PAIRING_TIMEOUT = 90000;
constexpr uint32_t CONFIRM_RETRY = 300;
constexpr uint32_t RESULT_TIMEOUT = 5000;
constexpr uint32_t COMPLETE_RETRY = 100;
constexpr uint8_t COMPLETE_SENDS = 3;
constexpr uint32_t SYNC_RETRY = 300;
constexpr uint32_t SYNC_TIMEOUT = 3000;
constexpr uint32_t SEARCH_INTERVAL = 1000;
constexpr uint8_t SEARCH_SAVED_CHANNEL_TRIES = 3;
constexpr uint32_t KEEPALIVE_INTERVAL = 2000;
constexpr uint32_t LINK_TIMEOUT = 5000;
constexpr uint32_t REASSEMBLY_TIMEOUT = 3000;
constexpr size_t RX_LIMIT = 16384;

// Channels most access points use come first
constexpr uint8_t CHANNEL_ORDER[] = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
constexpr size_t CHANNEL_COUNT = sizeof(CHANNEL_ORDER);

const uint8_t BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct __attribute__((packed)) DiscoveryPacket {
    uint8_t type;
    uint8_t version;
    uint8_t mode;
    uint8_t mac[6];      // Ours
    uint8_t channel;     // Channel this was sent on
    uint8_t session[16];
    uint8_t pubkey[65];
};

struct __attribute__((packed)) ChallengePacket {
    uint8_t type;
    uint8_t version;
    uint8_t mode;
    uint8_t mac[6];        // FluidNC's
    uint8_t channel;       // FluidNC's operating channel
    uint8_t dial_channel;  // Channel field of the discovery it answered
    uint8_t session[16];
    uint8_t pubkey[65];
    char hostname[32];
};

// Also the layout of the Complete packet
struct __attribute__((packed)) ConfirmPacket {
    uint8_t type;
    uint8_t version;
    uint8_t mode;
    uint8_t mac[6];  // Ours
    uint8_t session[16];
    uint8_t tag[16];
};

struct __attribute__((packed)) ResultPacket {
    uint8_t type;
    uint8_t version;
    uint8_t mode;
    uint8_t mac[6];   // FluidNC's
    uint8_t channel;  // FluidNC's operating channel
    uint8_t session[16];
    char hostname[32];
    uint8_t tag[16];
};

static_assert(sizeof(DiscoveryPacket) == 91, "discovery layout");
static_assert(sizeof(ChallengePacket) == 124, "challenge layout");
static_assert(sizeof(ConfirmPacket) == 41, "confirm layout");
static_assert(sizeof(ResultPacket) == 74, "result layout");

// Both ESP32 and the simulator hosts are little-endian, as the protocol is
uint32_t get32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

void put32(uint8_t* p, uint32_t v) {
    memcpy(p, &v, 4);
}

bool validChannel(uint8_t c) {
    return c >= 1 && c <= 14;
}

bool sameMac(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
}

bool isRealtimeByte(uint8_t c) {
    return c == 0x18 || c == '?' || c == '!' || c == '~' || (c >= 0x80 && c <= 0xBF);
}

void fillPairPacket(ConfirmPacket& pkt, uint8_t type, const uint8_t mac[6], const uint8_t session[16],
                    const uint8_t lmk[16]) {
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = type;
    pkt.version = PROTOCOL_VERSION;
    pkt.mode = MODE_PAIR;
    memcpy(pkt.mac, mac, 6);
    memcpy(pkt.session, session, 16);
    Bytes body = {&pkt, offsetof(ConfirmPacket, tag)};
    EspNowCrypto::hmac16(lmk, 16, &body, 1, pkt.tag);
}

}  // namespace

const char* EspNowLink::stateName(State state) {
    switch (state) {
        case State::Unpaired: return "Not paired";
        case State::Discovering: return "Pairing...";
        case State::Confirming: return "Confirming...";
        case State::Searching: return "Searching...";
        case State::Synchronizing: return "Synchronizing...";
        case State::Connected: return "Connected";
    }
    return "?";
}

bool EspNowLink::begin() {
    if (_started) {
        return true;
    }
    uint8_t pmk[16];
    EspNowCrypto::keyFromLabel(PMK_LABEL, pmk);
    _started = _radio.begin(pmk);
    EspNowCrypto::wipe(pmk, sizeof(pmk));
    if (!_started) {
        LOG_PRINTLN("[ESP-NOW] Radio init failed");
        return false;
    }
    _radio.localMac(_mac);
    LOG_PRINTF("[ESP-NOW] Started, MAC %02x:%02x:%02x:%02x:%02x:%02x\n", _mac[0], _mac[1], _mac[2], _mac[3],
               _mac[4], _mac[5]);
    resetSession();
    return true;
}

bool EspNowLink::pairingChanged() {
    bool changed = _pairingChanged;
    _pairingChanged = false;
    return changed;
}

void EspNowLink::setState(State state) {
    if (state != _state) {
        LOG_PRINTF("[ESP-NOW] %s -> %s\n", stateName(_state), stateName(state));
        _state = state;
        _stateSince = _now;
    }
}

void EspNowLink::resetSession() {
    _rxNonce = EspNowCrypto::randomNonZero32();
    _peerNonce = 0;
    _peerNonceKnown = false;
    _txCounter = 0;
    _replay = ReplayWindow();
    _syncAuthSent = false;
    _txLine.clear();
    _rxPending = false;
}

// ---------------------------------------------------------------------------
// Saved pairing / reconnection
// ---------------------------------------------------------------------------

void EspNowLink::setPairing(const EspNowPairing& pairing) {
    if (!_started) {
        return;
    }
    if (_state == State::Discovering || _state == State::Confirming) {
        clearHandshake();
    }
    if (_hasPairing && !sameMac(_pairing.mac, pairing.mac)) {
        _radio.removePeer(_pairing.mac);
    }
    _pairing = pairing;
    _pairing.hostname[ESPNOW_HOSTNAME_SIZE - 1] = '\0';
    if (!validChannel(_pairing.channel)) {
        _pairing.channel = CHANNEL_ORDER[0];
    }
    _hasPairing = true;
    restorePairing();
}

void EspNowLink::clearPairing() {
    if (_hasPairing && _started) {
        _radio.removePeer(_pairing.mac);
    }
    EspNowCrypto::wipe(&_pairing, sizeof(_pairing));
    _hasPairing = false;
    if (_state != State::Discovering && _state != State::Confirming) {
        resetSession();
        setState(State::Unpaired);
    }
}

void EspNowLink::restorePairing() {
    _channel = _pairing.channel;
    _radio.setChannel(_channel);
    _radio.setPeer(_pairing.mac, _pairing.lmk);
    beginSearching();
}

void EspNowLink::beginSearching() {
    resetSession();
    _searchTries = 0;
    _lastSearch = _now - SEARCH_INTERVAL;  // Probe on the next poll
    setState(State::Searching);
}

void EspNowLink::beginSynchronizing() {
    setState(State::Synchronizing);
    _syncAuthSent = sendKeepalive();
    _lastKeepalive = _now;
}

void EspNowLink::setConnected() {
    setState(State::Connected);
    _lastRx = _now;
    _lastKeepalive = _now;
    if (_channel != _pairing.channel) {
        _pairing.channel = _channel;
        _pairingChanged = true;
    }
    LOG_PRINTF("[ESP-NOW] Connected to %s on channel %u\n", _pairing.hostname, _channel);
}

// ---------------------------------------------------------------------------
// Pairing
// ---------------------------------------------------------------------------

void EspNowLink::startPairing() {
    if (!_started) {
        return;
    }
    clearHandshake();
    if (_hasPairing) {
        // Keep the current machine's traffic out of the way while pairing
        _radio.removePeer(_pairing.mac);
    }
    resetSession();
    if (!EspNowCrypto::generateKeyPair(_pairPrivate, _pairPublic)) {
        LOG_PRINTLN("[ESP-NOW] Key generation failed");
        return;
    }
    static const uint8_t zero[SESSION_ID_SIZE] = {};
    do {
        EspNowCrypto::randomBytes(_sessionId, sizeof(_sessionId));
    } while (EspNowCrypto::equal(_sessionId, zero, sizeof(_sessionId)));

    _discoveryIndex = 0;
    _pairingStarted = _now;
    _lastDiscovery = _now - DISCOVERY_INTERVAL;
    setState(State::Discovering);
}

void EspNowLink::cancelPairing() {
    if (_state != State::Discovering && _state != State::Confirming) {
        return;
    }
    clearHandshake();
    if (_hasPairing) {
        restorePairing();
    } else {
        resetSession();
        setState(State::Unpaired);
    }
}

void EspNowLink::clearHandshake() {
    if (_state == State::Discovering || _state == State::Confirming) {
        _radio.removePeer(BROADCAST);
        if (_state == State::Confirming && !(_hasPairing && sameMac(_newPairing.mac, _pairing.mac))) {
            _radio.removePeer(_newPairing.mac);
        }
    }
    EspNowCrypto::wipe(_pairPrivate, sizeof(_pairPrivate));
    EspNowCrypto::wipe(_pairLmk, sizeof(_pairLmk));
    EspNowCrypto::wipe(&_newPairing, sizeof(_newPairing));
    _completeSends = 0;
}

void EspNowLink::sendDiscovery() {
    uint8_t channel = CHANNEL_ORDER[_discoveryIndex++ % CHANNEL_COUNT];
    _channel = channel;
    _radio.setChannel(channel);
    _radio.setPeer(BROADCAST, nullptr);

    DiscoveryPacket pkt = {};
    pkt.type = PKT_DISCOVERY;
    pkt.version = PROTOCOL_VERSION;
    pkt.mode = MODE_PAIR;
    memcpy(pkt.mac, _mac, 6);
    pkt.channel = channel;
    memcpy(pkt.session, _sessionId, sizeof(pkt.session));
    memcpy(pkt.pubkey, _pairPublic, sizeof(pkt.pubkey));
    _radio.send(BROADCAST, &pkt, sizeof(pkt));
    _lastDiscovery = _now;
}

void EspNowLink::handleChallenge(const EspNowFrame& frame) {
    if (_state != State::Discovering || frame.len != sizeof(ChallengePacket)) {
        return;
    }
    ChallengePacket challenge;
    memcpy(&challenge, frame.data, sizeof(challenge));
    if (challenge.version != PROTOCOL_VERSION || challenge.mode != MODE_PAIR || !sameMac(challenge.mac, frame.src) ||
        !EspNowCrypto::equal(challenge.session, _sessionId, sizeof(_sessionId)) ||
        !validChannel(challenge.channel) || !validChannel(challenge.dial_channel) || challenge.pubkey[0] != 0x04) {
        return;
    }

    uint8_t secret[EspNowCrypto::SECRET_SIZE];
    if (!EspNowCrypto::sharedSecret(_pairPrivate, challenge.pubkey, secret)) {
        LOG_PRINTLN("[ESP-NOW] Pairing: invalid public key from FluidNC");
        return;
    }

    // The pairing key covers both handshake packets exactly as FluidNC saw
    // them, so rebuild our discovery with the channel FluidNC heard it on
    DiscoveryPacket discovery = {};
    discovery.type = PKT_DISCOVERY;
    discovery.version = PROTOCOL_VERSION;
    discovery.mode = MODE_PAIR;
    memcpy(discovery.mac, _mac, 6);
    discovery.channel = challenge.dial_channel;
    memcpy(discovery.session, _sessionId, sizeof(discovery.session));
    memcpy(discovery.pubkey, _pairPublic, sizeof(discovery.pubkey));

    uint8_t windowKey[16];
    EspNowCrypto::keyFromLabel(WINDOW_LABEL, windowKey);
    Bytes parts[] = {
        {SESSION_LABEL, strlen(SESSION_LABEL)},
        {secret, sizeof(secret)},
        {&discovery, sizeof(discovery)},
        {&challenge, sizeof(challenge)},
    };
    EspNowCrypto::hmac16(windowKey, sizeof(windowKey), parts, 4, _pairLmk);
    EspNowCrypto::wipe(secret, sizeof(secret));
    EspNowCrypto::wipe(windowKey, sizeof(windowKey));
    EspNowCrypto::wipe(_pairPrivate, sizeof(_pairPrivate));

    ConfirmPacket confirm;
    fillPairPacket(confirm, PKT_CONFIRM, _mac, _sessionId, _pairLmk);
    memcpy(_confirmPacket, &confirm, sizeof(confirm));

    memset(&_newPairing, 0, sizeof(_newPairing));
    memcpy(_newPairing.mac, challenge.mac, 6);

    // Answer on the channel the challenge arrived on
    if (validChannel(frame.channel) && frame.channel != _channel) {
        _channel = frame.channel;
        _radio.setChannel(_channel);
    }
    _radio.setPeer(challenge.mac, nullptr);
    _radio.send(challenge.mac, _confirmPacket, sizeof(_confirmPacket));
    _confirmStarted = _now;
    _lastConfirm = _now;
    _completeSends = 0;
    setState(State::Confirming);
}

void EspNowLink::handleResult(const EspNowFrame& frame) {
    if (_state != State::Confirming || _completeSends > 0 || frame.len != sizeof(ResultPacket) ||
        !sameMac(frame.src, _newPairing.mac)) {
        return;
    }
    ResultPacket result;
    memcpy(&result, frame.data, sizeof(result));
    uint8_t expected[16];
    Bytes body = {&result, offsetof(ResultPacket, tag)};
    EspNowCrypto::hmac16(_pairLmk, sizeof(_pairLmk), &body, 1, expected);
    if (result.version != PROTOCOL_VERSION || result.mode != MODE_PAIR || !sameMac(result.mac, frame.src) ||
        !EspNowCrypto::equal(result.session, _sessionId, sizeof(_sessionId)) ||
        !EspNowCrypto::equal(expected, result.tag, sizeof(expected))) {
        LOG_PRINTLN("[ESP-NOW] Pairing: rejected result from FluidNC");
        return;
    }

    memcpy(_newPairing.lmk, _pairLmk, sizeof(_newPairing.lmk));
    _newPairing.channel = validChannel(result.channel) ? result.channel : _channel;
    memcpy(_newPairing.hostname, result.hostname, sizeof(_newPairing.hostname));
    _newPairing.hostname[ESPNOW_HOSTNAME_SIZE - 1] = '\0';

    // Completion goes out a few times before we switch to the encrypted
    // peer, since FluidNC only activates the pairing once it arrives
    ConfirmPacket complete;
    fillPairPacket(complete, PKT_COMPLETE, _mac, _sessionId, _pairLmk);
    memcpy(_completePacket, &complete, sizeof(complete));
    _radio.send(_newPairing.mac, _completePacket, sizeof(_completePacket));
    _completeSends = 1;
    _lastComplete = _now;
}

void EspNowLink::activatePairing() {
    _radio.removePeer(BROADCAST);
    if (_hasPairing && !sameMac(_pairing.mac, _newPairing.mac)) {
        _radio.removePeer(_pairing.mac);
    }
    _pairing = _newPairing;
    _hasPairing = true;
    _pairingChanged = true;
    EspNowCrypto::wipe(&_newPairing, sizeof(_newPairing));
    EspNowCrypto::wipe(_pairLmk, sizeof(_pairLmk));
    _completeSends = 0;

    LOG_PRINTF("[ESP-NOW] Paired with %s (%02x:%02x:%02x:%02x:%02x:%02x) on channel %u\n", _pairing.hostname,
               _pairing.mac[0], _pairing.mac[1], _pairing.mac[2], _pairing.mac[3], _pairing.mac[4], _pairing.mac[5],
               _pairing.channel);

    _channel = _pairing.channel;
    _radio.setChannel(_channel);
    _radio.setPeer(_pairing.mac, _pairing.lmk);
    resetSession();
    beginSynchronizing();
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

void EspNowLink::stampTag(uint8_t* out) {
    put32(out, _peerNonce);
    put32(out + 4, ++_txCounter);
}

// Accepts each counter once, within a 64-entry window, for our current challenge
bool EspNowLink::acceptReplay(uint32_t nonce, uint32_t counter) {
    if (_replay.nonce != _rxNonce) {
        _replay = ReplayWindow();
        _replay.nonce = _rxNonce;
    }
    if (nonce != _rxNonce || counter == 0) {
        return false;
    }
    if (counter > _replay.top) {
        uint32_t shift = counter - _replay.top;
        _replay.seen = shift >= 64 ? 0 : _replay.seen << shift;
        _replay.seen |= 1;
        _replay.top = counter;
        return true;
    }
    uint32_t back = _replay.top - counter;
    if (back >= 64) {
        return false;
    }
    uint64_t bit = 1ULL << back;
    if (_replay.seen & bit) {
        return false;
    }
    _replay.seen |= bit;
    return true;
}

// Returns true if the keepalive was authenticated (FluidNC's challenge known)
bool EspNowLink::sendKeepalive() {
    uint8_t pkt[AUTH_KEEPALIVE_SIZE];
    pkt[0] = PKT_KEEPALIVE;
    put32(pkt + 1, _rxNonce);
    _lastKeepalive = _now;
    if (!_peerNonceKnown) {
        _radio.send(_pairing.mac, pkt, PLAIN_KEEPALIVE_SIZE);
        return false;
    }
    stampTag(pkt + 5);
    pkt[13] = _state == State::Connected ? KEEPALIVE_CONFIRMED : 0;
    return _radio.send(_pairing.mac, pkt, sizeof(pkt));
}

void EspNowLink::handleKeepalive(const EspNowFrame& frame) {
    const uint8_t* d = frame.data;
    if (frame.len == AUTH_KEEPALIVE_SIZE) {
        uint32_t advertised = get32(d + 1);
        if (advertised == 0 || !acceptReplay(get32(d + 5), get32(d + 9))) {
            return;
        }
        _peerNonce = advertised;
        _peerNonceKnown = true;
        _lastRx = _now;
        bool confirmed = d[13] & KEEPALIVE_CONFIRMED;
        if (_state == State::Searching) {
            beginSynchronizing();
        } else if (_state == State::Synchronizing) {
            if (confirmed && _syncAuthSent) {
                setConnected();
            } else {
                _syncAuthSent = sendKeepalive() || _syncAuthSent;
            }
        }
    } else if (frame.len == PLAIN_KEEPALIVE_SIZE) {
        uint32_t advertised = get32(d + 1);
        if (advertised == 0) {
            return;
        }
        bool newSession = !_peerNonceKnown || _peerNonce != advertised;
        _peerNonce = advertised;
        _peerNonceKnown = true;
        _lastRx = _now;
        if (newSession || _state == State::Searching) {
            // FluidNC started a fresh session (e.g. it rebooted): new challenge
            _txCounter = 0;
            _rxNonce = EspNowCrypto::randomNonZero32();
            _replay = ReplayWindow();
            _syncAuthSent = false;
            beginSynchronizing();
        } else {
            sendKeepalive();
        }
    }
}

void EspNowLink::handleData(const EspNowFrame& frame) {
    const uint8_t* d = frame.data;
    if (frame.len < DATA_HEADER_SIZE || !acceptReplay(get32(d + 1), get32(d + 5))) {
        return;
    }
    uint8_t seq = d[9];
    uint8_t index = d[10];
    uint8_t total = d[11];
    size_t len = frame.len - DATA_HEADER_SIZE;
    if (total == 0 || total > MAX_CHUNKS || index >= total || len > DATA_CHUNK) {
        return;
    }
    _lastRx = _now;

    if (_rxPending && (seq != _rxSeq || _now - _rxStarted > REASSEMBLY_TIMEOUT)) {
        _rxPending = false;
    }
    if (!_rxPending) {
        _rxPending = true;
        _rxSeq = seq;
        _rxTotal = total;
        _rxGot = 0;
        _rxStarted = _now;
    }
    if (total != _rxTotal) {
        return;
    }
    memcpy(_rxChunks[index], d + DATA_HEADER_SIZE, len);
    _rxChunkLen[index] = (uint8_t)len;
    _rxGot |= (uint8_t)(1u << index);
    uint8_t all = (uint8_t)((1u << total) - 1);
    if ((_rxGot & all) != all) {
        return;
    }
    _rxPending = false;

    if (_rx.size() - _rxPos > RX_LIMIT) {
        LOG_PRINTLN("[ESP-NOW] Receive buffer full, dropping message");
        return;
    }
    char last = '\n';
    for (uint8_t i = 0; i < total; i++) {
        for (uint8_t j = 0; j < _rxChunkLen[i]; j++) {
            char c = (char)_rxChunks[i][j];
            if (c != '\r') {
                _rx.push_back(c);
                last = c;
            }
        }
    }
    if (last != '\n') {
        _rx.push_back('\n');
    }
}

// ---------------------------------------------------------------------------
// Byte stream
// ---------------------------------------------------------------------------

void EspNowLink::write(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        if (_txLine.empty()) {
            if (isRealtimeByte(c)) {
                writeRealtime(c);
                continue;
            }
            if (c == '\n' || c == '\r') {
                continue;  // FluidNC would answer an empty line with a stray ok
            }
        }
        _txLine.push_back((char)c);
        if (c == '\n' || _txLine.size() >= DATA_CHUNK * MAX_CHUNKS) {
            sendMessage((const uint8_t*)_txLine.data(), _txLine.size());
            _txLine.clear();
        }
    }
}

void EspNowLink::writeRealtime(uint8_t c) {
    if (_state != State::Connected) {
        return;
    }
    uint8_t pkt[REALTIME_SIZE];
    pkt[0] = PKT_REALTIME;
    stampTag(pkt + 1);
    pkt[9] = c;
    _radio.send(_pairing.mac, pkt, sizeof(pkt));
}

void EspNowLink::sendMessage(const uint8_t* data, size_t len) {
    if (_state != State::Connected) {
        LOG_PRINTLN("[ESP-NOW] Not connected, dropping line");
        return;
    }
    uint8_t total = (uint8_t)((len + DATA_CHUNK - 1) / DATA_CHUNK);
    uint8_t seq = _txSeq++;
    uint8_t pkt[ESPNOW_MAX_PAYLOAD];
    for (uint8_t i = 0; i < total; i++) {
        size_t offset = (size_t)i * DATA_CHUNK;
        size_t chunk = len - offset < DATA_CHUNK ? len - offset : DATA_CHUNK;
        pkt[0] = PKT_DATA;
        stampTag(pkt + 1);
        pkt[9] = seq;
        pkt[10] = i;
        pkt[11] = total;
        memcpy(pkt + DATA_HEADER_SIZE, data + offset, chunk);
        _radio.send(_pairing.mac, pkt, DATA_HEADER_SIZE + chunk);
    }
}

int EspNowLink::read() {
    if (_rxPos >= _rx.size()) {
        return -1;
    }
    uint8_t c = (uint8_t)_rx[_rxPos++];
    if (_rxPos == _rx.size()) {
        _rx.clear();
        _rxPos = 0;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Poll
// ---------------------------------------------------------------------------

void EspNowLink::handleFrame(const EspNowFrame& frame) {
    if (frame.len == 0) {
        return;
    }
    uint8_t type = frame.data[0];
    if (type == PKT_CHALLENGE) {
        handleChallenge(frame);
        return;
    }
    if (type == PKT_RESULT) {
        handleResult(frame);
        return;
    }
    if (!_hasPairing || !sameMac(frame.src, _pairing.mac) ||
        (_state != State::Searching && _state != State::Synchronizing && _state != State::Connected)) {
        return;
    }
    _rssi = _rssi == -127 ? frame.rssi : (int8_t)((_rssi * 4 + frame.rssi) / 5);
    if (type == PKT_KEEPALIVE) {
        handleKeepalive(frame);
    } else if (type == PKT_DATA) {
        handleData(frame);
    }
}

void EspNowLink::poll(uint32_t now_ms) {
    _now = now_ms;
    if (!_started) {
        return;
    }

    EspNowFrame frame;
    for (int i = 0; i < 32 && _radio.receive(frame); i++) {
        handleFrame(frame);
    }

    switch (_state) {
        case State::Unpaired:
            break;

        case State::Discovering:
            if (_now - _pairingStarted > PAIRING_TIMEOUT) {
                LOG_PRINTLN("[ESP-NOW] Pairing timed out");
                cancelPairing();
            } else if (_now - _lastDiscovery >= DISCOVERY_INTERVAL) {
                sendDiscovery();
            }
            break;

        case State::Confirming:
            if (_completeSends > 0) {
                if (_now - _lastComplete >= COMPLETE_RETRY) {
                    if (_completeSends < COMPLETE_SENDS) {
                        _radio.send(_newPairing.mac, _completePacket, sizeof(_completePacket));
                        _completeSends++;
                        _lastComplete = _now;
                    } else {
                        activatePairing();
                    }
                }
            } else if (_now - _confirmStarted > RESULT_TIMEOUT) {
                // FluidNC never answered: go back to discovery
                _radio.removePeer(_newPairing.mac);
                _lastDiscovery = _now - DISCOVERY_INTERVAL;
                setState(State::Discovering);
            } else if (_now - _lastConfirm >= CONFIRM_RETRY) {
                _radio.send(_newPairing.mac, _confirmPacket, sizeof(_confirmPacket));
                _lastConfirm = _now;
            }
            break;

        case State::Searching:
            if (_now - _lastSearch >= SEARCH_INTERVAL) {
                // A few tries on the saved channel, then sweep all of them
                uint8_t channel = _searchTries < SEARCH_SAVED_CHANNEL_TRIES
                                      ? _pairing.channel
                                      : CHANNEL_ORDER[(_searchTries - SEARCH_SAVED_CHANNEL_TRIES) % CHANNEL_COUNT];
                if (_searchTries < 255) {
                    _searchTries++;
                }
                if (channel != _channel) {
                    _channel = channel;
                    _radio.setChannel(channel);
                }
                sendKeepalive();
                _lastSearch = _now;
            }
            break;

        case State::Synchronizing:
            if (_now - _stateSince > SYNC_TIMEOUT) {
                beginSearching();
            } else if (_now - _lastKeepalive >= SYNC_RETRY) {
                _syncAuthSent = sendKeepalive() || _syncAuthSent;
            }
            break;

        case State::Connected:
            if (_now - _lastRx > LINK_TIMEOUT) {
                LOG_PRINTLN("[ESP-NOW] Connection lost");
                beginSearching();
            } else if (_now - _lastKeepalive >= KEEPALIVE_INTERVAL) {
                sendKeepalive();
            }
            break;
    }
}
