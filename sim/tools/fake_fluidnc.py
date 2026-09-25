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

Test helpers (type them in the simulator's Terminal tab or console):
    $sim/alarm=<n>   raise ALARM:<n>
    $sim/msg=<text>  send [MSG:<text>]
    $sim/limit=XYZA  set limit pins (e.g. $sim/limit=X, or $sim/limit= to clear)
    $sim/probe=1|0   set probe pin
"""

import argparse
import asyncio
import base64
import hashlib
import json
import os
import re
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
    def __init__(self, reader, writer, machine, sd_root=None):
        self.reader, self.writer, self.m = reader, writer, machine
        self.sd_root = sd_root
        self.report_interval = 0

    def send(self, line: str):
        # FluidNC sends controller output as newline-terminated lines in binary
        # frames (a real controller may also pack several lines into one frame)
        self.writer.write(make_frame((line + "\n").encode()))

    async def auto_report(self):
        while True:
            if self.report_interval:
                self.send(self.m.status())
                await self.writer.drain()
                await asyncio.sleep(self.report_interval / 1000)
            else:
                await asyncio.sleep(0.1)

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
        elif up.startswith("$SIM/MSG="):
            self.send(f"[MSG:{line.split('=', 1)[1]}]")
        elif up.startswith("$SIM/LIMIT="):
            m.limits = "".join(a for a in line.split("=", 1)[1].upper() if a in AXES)
            self.send("ok")
        elif up.startswith("$SIM/PROBE="):
            m.probe = line.split("=", 1)[1].strip() == "1"
            self.send("ok")
        else:
            self.send("ok")

    async def run(self):
        self.send("[MSG:INFO: FluidNC fake server for FluidTouch simulator]")
        reporter = asyncio.create_task(self.auto_report())
        try:
            while True:
                opcode, data = await read_frame(self.reader)
                if opcode == 0x8:  # close
                    self.writer.write(make_frame(b"", 0x8))
                    break
                if opcode == 0x9:  # ping
                    self.writer.write(make_frame(data, 0xA))
                    continue
                if opcode not in (0x1, 0x2):
                    continue
                text = data.decode(errors="replace")
                # Single-byte realtime commands arrive as their own message
                if len(data) == 1 and self.handle_realtime(data[0]):
                    pass
                else:
                    for line in text.replace("\r", "").split("\n"):
                        if len(line) == 1 and self.handle_realtime(ord(line)):
                            continue
                        print(f"  <- {line}")
                        self.handle_line(line.strip())
                await self.writer.drain()
        finally:
            reporter.cancel()


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
    try:
        await Session(reader, writer, machine, sd_root).run()
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
    args = parser.parse_args()
    machine = Machine()
    server = await asyncio.start_server(lambda r, w: handle_client(r, w, machine, args.sd), args.host, args.port)
    print(f"Fake FluidNC listening on ws://{args.host}:{args.port}/ (Ctrl+C to stop)")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
