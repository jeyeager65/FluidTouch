#!/usr/bin/env python3
"""Minimal fake FluidNC WebSocket server for testing the FluidTouch simulator
without a real controller. Standard library only (Python 3.8+).

    python sim/tools/fake_fluidnc.py [--port 81] [--sd <folder>]

Then add a machine in the simulator with URL 127.0.0.1 and port 81.

It speaks just enough of the FluidNC websocket protocol for FluidTouch:
status reports (polled "?" or $Report/Interval auto-reports), $G parser
state, jogging ($J=), homing ($H/$HX..), zeroing (G10 L20), feed hold /
resume / reset, overrides, unlock and $Files/ListGcode (serving the folder
given with --sd, or a few sample files). Anything else gets "ok".

It also plays FluidNC's side of ESP-NOW (v4.0.4+) on UDP port 8182, for the
simulator's ESP-NOW radio: $espnow/pair, $espnow/list, $espnow/unpair=<n> and
$espnow/cancel work as on FluidNC, and --espnow-pair opens the pairing window
at startup. Pairings are remembered in sim/data/fake_espnow_pairings.json.

Test helpers (type them in the simulator's Terminal tab or console):
    $sim/alarm=<n>   raise ALARM:<n>
    $sim/msg=<text>  send [MSG:<text>]
    $sim/limit=XYZA  set limit pins (e.g. $sim/limit=X, or $sim/limit= to clear)
    $sim/probe=1|0   set probe pin
    $sim/state=Door:0  report any state string (e.g. Door:0, Sleep, Bogus)
"""

import argparse
import asyncio
import base64
import hashlib
import hmac
import json
import os
import re
import secrets
import struct
import time

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
AXES = "XYZA"


class Machine:
    def __init__(self):
        self.state = "Idle"
        self.mpos = [0.0, 0.0, 0.0, 0.0]
        self.wco = [0.0, 0.0, 0.0, 0.0]
        self.feed_ov = 100
        self.rapid_ov = 100
        self.spindle_ov = 100
        self.limits = ""
        self.probe = False
        self.busy_until = 0.0

    def tick(self):
        if self.state in ("Jog", "Home") and time.monotonic() >= self.busy_until:
            self.state = "Idle"

    def status(self):
        self.tick()
        mpos = ",".join(f"{v:.3f}" for v in self.mpos)
        wco = ",".join(f"{v:.3f}" for v in self.wco)
        pins = self.limits + ("P" if self.probe else "")
        report = (f"<{self.state}|MPos:{mpos}|FS:0,0|WCO:{wco}"
                  f"|Ov:{self.feed_ov},{self.rapid_ov},{self.spindle_ov}")
        if pins:
            report += f"|Pn:{pins}"
        return report + ">"


async def read_frame(reader):
    """Read one client frame -> (opcode, payload bytes)."""
    b1, b2 = await reader.readexactly(2)
    opcode = b1 & 0x0F
    length = b2 & 0x7F
    if length == 126:
        (length,) = struct.unpack(">H", await reader.readexactly(2))
    elif length == 127:
        (length,) = struct.unpack(">Q", await reader.readexactly(8))
    mask = await reader.readexactly(4) if b2 & 0x80 else b"\0\0\0\0"
    data = bytearray(await reader.readexactly(length))
    for i in range(length):
        data[i] ^= mask[i % 4]
    return opcode, bytes(data)


def make_frame(payload: bytes, opcode=0x2):
    header = bytes([0x80 | opcode])
    n = len(payload)
    if n < 126:
        header += bytes([n])
    elif n < 65536:
        header += bytes([126]) + struct.pack(">H", n)
    else:
        header += bytes([127]) + struct.pack(">Q", n)
    return header + payload


SAMPLE_FILES = {"/sd/": [("projects", -1), ("facing.nc", 18432), ("sign_v2.gcode", 254871)],
                "/sd/projects/": [("bracket.nc", 9811), ("drawer_front.nc", 120334)]}


def list_files(sd_root, path):
    """-> (files list of {"name","size"}, error or None) for $Files/ListGcode."""
    if not path.endswith("/"):
        path += "/"
    if sd_root is None:
        entries = SAMPLE_FILES.get(path)
        if entries is None:
            return [], "Not a directory"
        return [{"name": n, "size": str(sz)} for n, sz in entries], None
    rel = path[len("/sd/"):] if path.startswith("/sd/") else path.lstrip("/")
    folder = os.path.join(sd_root, rel)
    if not os.path.isdir(folder):
        return [], "Not a directory"
    files = []
    for name in sorted(os.listdir(folder)):
        full = os.path.join(folder, name)
        files.append({"name": name, "size": "-1" if os.path.isdir(full) else str(os.path.getsize(full))})
    return files, None


class Session:
    """One client channel. `out` takes controller output bytes (newline-terminated lines)."""

    def __init__(self, out, machine, sd_root=None, report_interval=0):
        self.out, self.m = out, machine
        self.sd_root = sd_root
        self.report_interval = report_interval

    def send(self, line: str):
        self.out((line + "\n").encode())

    async def auto_report(self):
        while True:
            if self.report_interval:
                self.send(self.m.status())
                await asyncio.sleep(self.report_interval / 1000)
            else:
                await asyncio.sleep(0.1)

    def handle_input(self, data: bytes):
        # Single-byte realtime commands arrive as their own message
        if len(data) == 1 and self.handle_realtime(data[0]):
            return
        for line in data.decode(errors="replace").replace("\r", "").split("\n"):
            if len(line) == 1 and self.handle_realtime(ord(line)):
                continue
            if line:
                print(f"  <- {line}")
            self.handle_line(line.strip())

    def handle_realtime(self, ch: int) -> bool:
        m = self.m
        if ch == ord("?"):
            self.send(m.status())
        elif ch == ord("!"):
            m.state = "Hold:0"
        elif ch == ord("~"):
            if m.state.startswith("Hold"):
                m.state = "Idle"
        elif ch == 0x18:
            m.state = "Idle"
            self.send("Grbl 3.9 [FluidNC fake (sim) '$' for help]")
        elif ch == 0x85:
            if m.state == "Jog":
                m.state = "Idle"
        elif 0x90 <= ch <= 0x9D:
            if ch == 0x90: m.feed_ov = 100
            elif ch == 0x91: m.feed_ov = min(200, m.feed_ov + 10)
            elif ch == 0x92: m.feed_ov = max(10, m.feed_ov - 10)
            elif ch == 0x93: m.feed_ov = min(200, m.feed_ov + 1)
            elif ch == 0x94: m.feed_ov = max(10, m.feed_ov - 1)
            elif ch == 0x95: m.rapid_ov = 100
            elif ch == 0x96: m.rapid_ov = 50
            elif ch == 0x97: m.rapid_ov = 25
            elif ch == 0x99: m.spindle_ov = 100
            elif ch == 0x9A: m.spindle_ov = min(200, m.spindle_ov + 10)
            elif ch == 0x9B: m.spindle_ov = max(10, m.spindle_ov - 10)
            elif ch == 0x9C: m.spindle_ov = min(200, m.spindle_ov + 1)
            elif ch == 0x9D: m.spindle_ov = max(10, m.spindle_ov - 1)
        else:
            return False
        return True

    def handle_line(self, line: str):
        m = self.m
        up = line.upper()
        if not line:
            return
        if up.startswith("$REPORT/INTERVAL="):
            self.report_interval = int(line.split("=", 1)[1] or 0)
            self.send(f"[MSG:INFO: Report/Interval={self.report_interval}]")
            self.send("ok")
        elif up == "$G":
            self.send("[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]")
            self.send("ok")
        elif up == "$X":
            m.state = "Idle"
            self.send("[MSG:Caution: Unlocked]")
            self.send("ok")
        elif up.startswith("$H"):
            axes = up[2:] or AXES
            for a in axes:
                if a in AXES:
                    m.mpos[AXES.index(a)] = 0.0
            m.state = "Home"
            m.busy_until = time.monotonic() + 1.5
            self.send("ok")
        elif up.startswith("$J="):
            relative = "G91" in up
            for axis, value in re.findall(r"([XYZA])(-?[\d.]+)", up[3:]):
                i = AXES.index(axis)
                v = float(value)
                m.mpos[i] = m.mpos[i] + v if relative else v + (m.wco[i] if "G53" not in up else 0)
            m.state = "Jog"
            m.busy_until = time.monotonic() + 0.5
            self.send("ok")
        elif up.startswith("G10") and "L20" in up:
            for axis, value in re.findall(r"([XYZA])(-?[\d.]+)", up):
                i = AXES.index(axis)
                m.wco[i] = m.mpos[i] - float(value)
            self.send("ok")
        elif up.startswith("$FILES/LISTGCODE"):
            path = line.split("=", 1)[1].strip() if "=" in line else "/sd/"
            files, error = list_files(self.sd_root, path or "/sd/")
            reply = {"error": error} if error else {"files": files, "path": path}
            self.send("[JSON:" + json.dumps(reply, separators=(",", ":")) + "]")
            self.send("ok")
        elif up.startswith("$SIM/ALARM="):
            m.state = "Alarm"
            self.send(f"ALARM:{line.split('=', 1)[1]}")
        elif up.startswith("$SIM/STATE="):
            m.state = line.split("=", 1)[1].strip() or "Idle"
            self.send("ok")
        elif up.startswith("$SIM/MSG="):
            self.send(f"[MSG:{line.split('=', 1)[1]}]")
        elif up.startswith("$SIM/LIMIT="):
            m.limits = "".join(a for a in line.split("=", 1)[1].upper() if a in AXES)
            self.send("ok")
        elif up.startswith("$SIM/PROBE="):
            m.probe = line.split("=", 1)[1].strip() == "1"
            self.send("ok")
        elif up.startswith("$ESPNOW/"):
            if espnow_server:
                espnow_server.command(self, up[len("$ESPNOW/"):])
            else:
                self.send("error:3")
        else:
            self.send("ok")


async def run_websocket(reader, writer, session):
    session.send("[MSG:INFO: FluidNC fake server for FluidTouch simulator]")
    reporter = asyncio.create_task(session.auto_report())
    try:
        while True:
            await writer.drain()
            opcode, data = await read_frame(reader)
            if opcode == 0x8:  # close
                writer.write(make_frame(b"", 0x8))
                break
            if opcode == 0x9:  # ping
                writer.write(make_frame(data, 0xA))
                continue
            if opcode in (0x1, 0x2):
                session.handle_input(data)
    finally:
        reporter.cancel()


# ---------------------------------------------------------------------------
# ESP-NOW (FluidNC v4.0.4+ pendant protocol) over UDP
#
# The simulator's radio (sim/src/espnow_radio_udp.cpp) sends each ESP-NOW frame
# as a UDP datagram: channel, source MAC, destination MAC, encrypted flag,
# payload. This plays FluidNC's side: $espnow/pair pairing, keepalives with
# anti-replay tags, fragmented line data, realtime bytes and 200 ms status
# auto-reports. Frames sent with the wrong encryption state are rejected, as
# real hardware would.
# ---------------------------------------------------------------------------

PKT_DISCOVERY, PKT_CHALLENGE, PKT_DATA, PKT_CONFIRM = 0x01, 0x02, 0x03, 0x04
PKT_REALTIME, PKT_KEEPALIVE, PKT_RESULT, PKT_COMPLETE = 0x05, 0x06, 0x07, 0x08
PAIR_PACKETS = {PKT_DISCOVERY: "discovery", PKT_CONFIRM: "confirm", PKT_COMPLETE: "complete"}
BROADCAST = b"\xff" * 6
WINDOW_LABEL = b"fluidnc-espnow-pairing-window-v1"
SESSION_LABEL = b"fluidnc-espnow-pairing-session-v1"
MAX_PAIRINGS = 8
CHUNK = 238  # 250-byte frame minus the 12-byte data header

# NIST P-256, pure Python (fast enough for one key exchange per pairing)
P256_P = 0xFFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF
P256_A = P256_P - 3
P256_B = 0x5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B
P256_N = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
P256_G = (0x6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296,
          0x4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5)


def p256_on_curve(pt):
    x, y = pt
    return (y * y - x * x * x - P256_A * x - P256_B) % P256_P == 0


def p256_add(p1, p2):
    if p1 is None:
        return p2
    if p2 is None:
        return p1
    (x1, y1), (x2, y2) = p1, p2
    if x1 == x2:
        if (y1 + y2) % P256_P == 0:
            return None
        slope = (3 * x1 * x1 + P256_A) * pow(2 * y1, -1, P256_P) % P256_P
    else:
        slope = (y2 - y1) * pow(x2 - x1, -1, P256_P) % P256_P
    x3 = (slope * slope - x1 - x2) % P256_P
    return x3, (slope * (x1 - x3) - y1) % P256_P


def p256_mul(k, pt):
    result = None
    while k:
        if k & 1:
            result = p256_add(result, pt)
        pt = p256_add(pt, pt)
        k >>= 1
    return result


def p256_keypair():
    d = secrets.randbelow(P256_N - 1) + 1
    x, y = p256_mul(d, P256_G)
    return d, b"\x04" + x.to_bytes(32, "big") + y.to_bytes(32, "big")


def p256_shared(d, pub):
    if len(pub) != 65 or pub[0] != 4:
        raise ValueError("not an uncompressed P-256 point")
    pt = (int.from_bytes(pub[1:33], "big"), int.from_bytes(pub[33:], "big"))
    if not p256_on_curve(pt):
        raise ValueError("point not on P-256")
    return p256_mul(d, pt)[0].to_bytes(32, "big")


def hmac16(key, *parts):
    h = hmac.new(key, digestmod=hashlib.sha256)
    for part in parts:
        h.update(part)
    return h.digest()[:16]


def nonzero32():
    v = 0
    while not v:
        v = secrets.randbits(32)
    return v


def mac_str(mac):
    return ":".join(f"{b:02x}" for b in mac)


class ReplayWindow:
    """Accepts each counter once, within 64 of the highest seen, for the current challenge."""

    def __init__(self):
        self.nonce, self.top, self.seen = 0, 0, 0

    def accept(self, challenge, nonce, counter):
        if self.nonce != challenge:
            self.nonce, self.top, self.seen = challenge, 0, 0
        if nonce != challenge or counter == 0:
            return False
        if counter > self.top:
            shift = counter - self.top
            self.seen = 0 if shift >= 64 else (self.seen << shift) & (2**64 - 1)
            self.seen |= 1
            self.top = counter
            return True
        back = self.top - counter
        if back >= 64 or self.seen & (1 << back):
            return False
        self.seen |= 1 << back
        return True


class EspNowPeer:
    def __init__(self, server, mac, lmk):
        self.server, self.mac, self.lmk = server, mac, lmk
        # FluidNC sets a 200 ms auto-report interval on the ESP-NOW channel
        self.session = Session(self.out, server.machine, server.sd_root, report_interval=200)
        self.reporter = asyncio.get_event_loop().create_task(self.session.auto_report())
        self.reset()

    def reset(self):
        self.rx_nonce = nonzero32()  # our challenge: the pendant stamps its frames with it
        self.peer_nonce, self.peer_known = 0, False
        self.tx_counter = 0
        self.replay = ReplayWindow()
        self.connected = False
        self.last_rx = 0.0
        self.tx_seq = 0
        self.frag = None

    def stamp(self):
        self.tx_counter += 1
        return struct.pack("<II", self.peer_nonce, self.tx_counter)

    def out(self, data: bytes):
        """Controller output -> DATA frames (FluidNC drops output while no pendant is connected)."""
        if not (self.connected and self.peer_known):
            return
        chunks = [data[i:i + CHUNK] for i in range(0, len(data), CHUNK)][:8] or [b""]
        seq, self.tx_seq = self.tx_seq, (self.tx_seq + 1) & 0xFF
        for i, chunk in enumerate(chunks):
            self.server.send(self.mac, bytes([PKT_DATA]) + self.stamp() + bytes([seq, i, len(chunks)]) + chunk, True)

    def close(self):
        self.reporter.cancel()


class EspNowServer(asyncio.DatagramProtocol):
    def __init__(self, machine, sd_root, channel, store, hostname):
        self.machine, self.sd_root = machine, sd_root
        self.mac = bytes([0x02, 0x46, 0x4E, 0x43, 0x00, 0x01])  # locally administered, "FNC"
        self.channel, self.store, self.hostname = channel, store, hostname
        self.peers = {}   # MAC -> EspNowPeer
        self.addrs = {}   # MAC -> UDP address last heard from
        self.window_until = 0.0
        self.txn = None   # pairing transaction in progress
        self.transport = None

    def log(self, text):
        print(f"ESP-NOW: {text}")

    def connection_made(self, transport):
        self.transport = transport
        if self.store and os.path.exists(self.store):
            with open(self.store) as f:
                for entry in json.load(f):
                    mac = bytes.fromhex(entry["mac"])
                    self.peers[mac] = EspNowPeer(self, mac, bytes.fromhex(entry["lmk"]))
            self.log(f"loaded {len(self.peers)} saved pairing(s) from {self.store}")
        asyncio.get_event_loop().create_task(self.tick())

    def save(self):
        if self.store:
            os.makedirs(os.path.dirname(os.path.abspath(self.store)), exist_ok=True)
            with open(self.store, "w") as f:
                json.dump([{"mac": p.mac.hex(), "lmk": p.lmk.hex()} for p in self.peers.values()], f, indent=2)

    def send(self, mac, payload, encrypted):
        addr = self.addrs.get(mac)
        if addr and self.transport:
            header = bytes([self.channel]) + self.mac + mac + bytes([1 if encrypted else 0])
            self.transport.sendto(header + payload, addr)

    # -- $espnow/... commands -------------------------------------------------

    def command(self, session, cmd):
        if cmd == "PAIR":
            self.open_window()
            session.send("[MSG:INFO: ESP-NOW: pairing enabled for 60 seconds]")
        elif cmd == "CANCEL":
            self.window_until, self.txn = 0.0, None
            session.send("[MSG:INFO: ESP-NOW: pairing cancelled]")
        elif cmd == "LIST" or cmd == "UNPAIR":
            if not self.peers:
                session.send("[MSG:INFO: ESP-NOW: whitelist is empty]")
            for i, mac in enumerate(self.peers, 1):
                session.send(f"[MSG:INFO: ESP-NOW: {i}: {mac_str(mac)}]")
        elif cmd.startswith("UNPAIR="):
            index = int(cmd.split("=", 1)[1] or 0)
            macs = list(self.peers)
            if index == 0:
                for peer in self.peers.values():
                    peer.close()
                self.peers.clear()
                session.send("[MSG:INFO: ESP-NOW: whitelist cleared]")
            elif 1 <= index <= len(macs):
                self.peers.pop(macs[index - 1]).close()
                session.send(f"[MSG:INFO: ESP-NOW: removed peripheral {mac_str(macs[index - 1])}]")
            else:
                session.send("error:3")
                return
            self.save()
        else:
            session.send("error:3")
            return
        session.send("ok")

    def open_window(self, seconds=60):
        self.window_until = time.monotonic() + seconds
        self.txn = None
        self.log(f"pairing window open for {seconds} s on channel {self.channel}")

    # -- Frames ---------------------------------------------------------------

    def datagram_received(self, data, addr):
        if len(data) < 15:
            return
        channel, src, dst, encrypted, pkt = data[0], data[1:7], data[7:13], data[13], data[14:]
        if dst not in (self.mac, BROADCAST):
            return
        self.addrs[src] = addr
        if channel != self.channel:
            return  # our radio is on another channel
        kind = pkt[0]
        if kind in PAIR_PACKETS:
            if encrypted:
                self.log(f"rejected encrypted {PAIR_PACKETS[kind]} packet from {mac_str(src)}")
            elif kind == PKT_DISCOVERY:
                self.on_discovery(src, pkt)
            elif kind == PKT_CONFIRM:
                self.on_confirm(src, pkt)
            else:
                self.on_complete(src, pkt)
            return
        peer = self.peers.get(src)
        if not peer:
            return
        if not encrypted:
            self.log(f"rejected unencrypted packet type {kind} from paired {mac_str(src)}")
            return
        if kind == PKT_KEEPALIVE:
            self.on_keepalive(peer, pkt)
        elif kind == PKT_DATA:
            self.on_data(peer, pkt)
        elif kind == PKT_REALTIME:
            self.on_realtime(peer, pkt)

    def authenticated(self, peer):
        peer.last_rx = time.monotonic()
        if not peer.connected:
            peer.connected = True
            self.log(f"peripheral connected {mac_str(peer.mac)}")

    def on_keepalive(self, peer, pkt):
        if len(pkt) == 14:
            advertised, nonce, counter = struct.unpack_from("<III", pkt, 1)
            if not advertised or not peer.replay.accept(peer.rx_nonce, nonce, counter):
                return
            peer.peer_nonce, peer.peer_known = advertised, True
            self.authenticated(peer)
        elif len(pkt) == 5:
            (advertised,) = struct.unpack_from("<I", pkt, 1)
            if not advertised:
                return
            if not peer.peer_known or peer.peer_nonce != advertised:
                # Pendant started a new session: fresh challenge, not connected until it authenticates
                if peer.connected:
                    self.log(f"peripheral {mac_str(peer.mac)} restarted its session")
                peer.tx_counter, peer.rx_nonce, peer.replay = 0, nonzero32(), ReplayWindow()
                peer.connected = False
            peer.peer_nonce, peer.peer_known = advertised, True
        else:
            return
        reply = bytes([PKT_KEEPALIVE]) + struct.pack("<I", peer.rx_nonce)
        if peer.peer_known:
            reply += peer.stamp() + bytes([1 if peer.connected else 0])
        self.send(peer.mac, reply, True)

    def on_data(self, peer, pkt):
        if len(pkt) < 12:
            return
        nonce, counter = struct.unpack_from("<II", pkt, 1)
        if not peer.replay.accept(peer.rx_nonce, nonce, counter):
            return
        seq, index, total, payload = pkt[9], pkt[10], pkt[11], pkt[12:]
        if not 1 <= total <= 8 or index >= total:
            return
        self.authenticated(peer)
        now = time.monotonic()
        if peer.frag and (peer.frag[0] != seq or peer.frag[1] != total or now - peer.frag[3] > 3):
            peer.frag = None
        if not peer.frag:
            peer.frag = (seq, total, {}, now)
        parts = peer.frag[2]
        parts[index] = payload
        if len(parts) == total:
            message = b"".join(parts[i] for i in range(total))
            peer.frag = None
            peer.session.handle_input(message if message.endswith(b"\n") else message + b"\n")

    def on_realtime(self, peer, pkt):
        if len(pkt) != 10:
            return
        nonce, counter = struct.unpack_from("<II", pkt, 1)
        if peer.replay.accept(peer.rx_nonce, nonce, counter):
            self.authenticated(peer)
            peer.session.handle_realtime(pkt[9])

    # -- Pairing --------------------------------------------------------------

    def hostname32(self):
        return self.hostname.encode()[:31].ljust(32, b"\0")

    def on_discovery(self, src, pkt):
        if time.monotonic() > self.window_until or len(pkt) != 91:
            return
        version, mode, mac, dial_channel = pkt[1], pkt[2], pkt[3:9], pkt[9]
        session, pubkey = pkt[10:26], pkt[26:91]
        if version != 4 or mode != 1 or mac != src or session == bytes(16) or not 1 <= dial_channel <= 14:
            return
        if self.txn:
            if self.txn["mac"] == mac and self.txn["session"] == session and self.txn["state"] == "confirm":
                self.txn["last"] = time.monotonic()
                self.send(mac, self.txn["challenge"], False)
            return
        existing = self.peers.get(mac)
        if existing and existing.connected:
            self.log(f"rejecting re-pair attempt from active peripheral {mac_str(mac)}")
            return
        if not existing and len(self.peers) >= MAX_PAIRINGS:
            self.log(f"pairing roster full, rejecting {mac_str(mac)}")
            return
        d, our_pub = p256_keypair()
        try:
            shared = p256_shared(d, pubkey)
        except ValueError as e:
            self.log(f"bad public key from {mac_str(mac)}: {e}")
            return
        challenge = (bytes([PKT_CHALLENGE, 4, 1]) + self.mac + bytes([self.channel, dial_channel]) +
                     session + our_pub + self.hostname32())
        window_key = hashlib.sha256(WINDOW_LABEL).digest()[:16]
        lmk = hmac16(window_key, SESSION_LABEL, shared, pkt, challenge)
        now = time.monotonic()
        self.txn = {"mac": mac, "session": session, "lmk": lmk, "challenge": challenge,
                    "state": "confirm", "started": now, "last": now}
        self.send(mac, challenge, False)
        self.log(f"challenge sent to {mac_str(mac)}")

    def on_confirm(self, src, pkt):
        txn = self.txn
        if not txn or txn["state"] != "confirm" or len(pkt) != 41 or src != txn["mac"] or pkt[3:9] != src:
            return
        if pkt[9:25] != txn["session"] or not hmac.compare_digest(hmac16(txn["lmk"], pkt[:25]), pkt[25:41]):
            self.log(f"pairing confirmation failed from {mac_str(src)}")
            return
        result = bytes([PKT_RESULT, 4, 1]) + self.mac + bytes([self.channel]) + txn["session"] + self.hostname32()
        txn["result"] = result + hmac16(txn["lmk"], result)
        txn["state"] = "complete"
        txn["last"] = time.monotonic()
        self.send(src, txn["result"], False)

    def on_complete(self, src, pkt):
        txn = self.txn
        if not txn or txn["state"] != "complete" or len(pkt) != 41 or src != txn["mac"] or pkt[3:9] != src:
            return
        if pkt[9:25] != txn["session"] or not hmac.compare_digest(hmac16(txn["lmk"], pkt[:25]), pkt[25:41]):
            return
        old = self.peers.pop(src, None)
        if old:
            old.close()
        self.peers[src] = EspNowPeer(self, src, txn["lmk"])
        self.txn, self.window_until = None, 0.0
        self.save()
        self.log(f"paired peripheral {mac_str(src)}")

    async def tick(self):
        while True:
            await asyncio.sleep(0.05)
            now = time.monotonic()
            txn = self.txn
            if txn:
                if now > self.window_until or now - txn["last"] > 10 or now - txn["started"] > 10:
                    self.log("pairing handshake timed out")
                    self.txn = None
                elif txn["state"] == "complete" and now - txn.get("resent", 0) >= 0.3:
                    txn["resent"] = now
                    self.send(txn["mac"], txn["result"], False)
            elif self.window_until and now > self.window_until:
                self.window_until = 0.0
                self.log("pairing window closed")
            for peer in self.peers.values():
                if peer.connected and now - peer.last_rx > 10:
                    self.log(f"peripheral disconnected {mac_str(peer.mac)}")
                    peer.reset()


espnow_server = None


async def handle_client(reader, writer, machine, sd_root):
    peer = writer.get_extra_info("peername")
    request = await reader.readuntil(b"\r\n\r\n")
    key = re.search(rb"Sec-WebSocket-Key:\s*(\S+)", request, re.I)
    if not key:
        writer.close()
        return
    accept = base64.b64encode(hashlib.sha1(key.group(1) + WS_GUID.encode()).digest()).decode()
    writer.write(("HTTP/1.1 101 Switching Protocols\r\n"
                  "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                  f"Sec-WebSocket-Accept: {accept}\r\n\r\n").encode())
    await writer.drain()
    print(f"Client connected: {peer}")
    # FluidNC sends controller output as newline-terminated lines in binary
    # frames (a real controller may also pack several lines into one frame)
    session = Session(lambda data: writer.write(make_frame(data)), machine, sd_root)
    try:
        await run_websocket(reader, writer, session)
    except (asyncio.IncompleteReadError, ConnectionError):
        pass
    finally:
        print(f"Client disconnected: {peer}")
        writer.close()


async def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=81)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--sd", help="folder to serve as the controller's SD card (default: sample files)")
    parser.add_argument("--espnow-port", type=int, default=8182, help="UDP port for simulated ESP-NOW (0 = off)")
    parser.add_argument("--espnow-channel", type=int, default=6, help="WiFi channel FluidNC is on (1-13)")
    parser.add_argument("--espnow-pair", action="store_true", help="open the 60 s pairing window at startup")
    parser.add_argument("--espnow-store", default=os.path.join(os.path.dirname(__file__), "..", "data",
                                                               "fake_espnow_pairings.json"),
                        help="file that remembers paired pendants ('' = don't save)")
    parser.add_argument("--hostname", default="fakefluidnc")
    args = parser.parse_args()
    machine = Machine()
    server = await asyncio.start_server(lambda r, w: handle_client(r, w, machine, args.sd), args.host, args.port)
    print(f"Fake FluidNC listening on ws://{args.host}:{args.port}/ (Ctrl+C to stop)")
    if args.espnow_port:
        global espnow_server
        espnow_server = EspNowServer(machine, args.sd, args.espnow_channel, args.espnow_store, args.hostname)
        await asyncio.get_running_loop().create_datagram_endpoint(
            lambda: espnow_server, local_addr=(args.host, args.espnow_port))
        print(f"ESP-NOW on UDP {args.host}:{args.espnow_port}, channel {args.espnow_channel}, "
              f"MAC {mac_str(espnow_server.mac)}")
        if args.espnow_pair:
            espnow_server.open_window()
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
