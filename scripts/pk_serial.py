"""PoolKeeper panel soros diagnosztika.

Használat (a repó gyökeréből):
  tools\\venv\\Scripts\\python.exe scripts\\pk_serial.py --port COM10 log 5           # 5 mp log
  tools\\venv\\Scripts\\python.exe scripts\\pk_serial.py --port COM10 cmd "r" "c" ... # parancsok
  tools\\venv\\Scripts\\python.exe scripts\\pk_serial.py --port COM10 shot out.png    # képernyőkép
  tools\\venv\\Scripts\\python.exe scripts\\pk_serial.py --port COM10 script f.txt    # parancsfájl; "shot NAME.png" és "wait MS" sorok

A port megnyitása NEM indítja újra a panelt (DTR/RTS alacsonyan tartva).
"""
import argparse
import sys
import time

import serial
from PIL import Image, ImageDraw


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.1
    s.dtr = False
    s.rts = False
    s.open()
    return s


def drain(s, seconds, echo=True):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        d = s.read(4096)
        if d:
            buf += d
    if echo and buf:
        sys.stdout.write(buf.decode("utf-8", "replace"))
        sys.stdout.flush()
    return buf


def send(s, line, settle=0.35):
    s.write((line + "\n").encode())
    s.flush()
    return drain(s, settle)


def shot(s, path, round_mask=True, retries=3):
    for i in range(retries):
        try:
            return _shot(s, path, round_mask)
        except RuntimeError as e:
            print("képernyőkép újrapróbálás:", e)
            drain(s, 0.5, echo=False)
    raise RuntimeError("képernyőkép sikertelen")


def _shot(s, path, round_mask=True):
    s.reset_input_buffer()
    s.write(b"shot\n")
    s.flush()
    buf = b""
    deadline = time.time() + 15
    hdr_idx = -1
    while time.time() < deadline:
        buf += s.read(65536)
        hdr_idx = buf.find(b"PKSHOT ")
        if hdr_idx >= 0 and b"\n" in buf[hdr_idx:]:
            break
    if hdr_idx < 0:
        raise RuntimeError("nincs PKSHOT fejléc: %r" % buf[-200:])
    nl = buf.index(b"\n", hdr_idx)
    w, h, n = (int(x) for x in buf[hdr_idx + 7 : nl].split())
    data = buf[nl + 1 :]
    while len(data) < n and time.time() < deadline:
        data += s.read(n - len(data))
    if len(data) < n:
        raise RuntimeError("hiányos kép: %d/%d" % (len(data), n))
    raw = data[:n]
    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        v = raw[2 * i] | (raw[2 * i + 1] << 8)
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        px[i % w, i // w] = (r * 255 // 31, g * 255 // 63, b * 255 // 31)
    if round_mask:
        # A kerek kijelzőn nem látható sarkak jelölése szürkével
        mask = Image.new("L", (w, h), 0)
        ImageDraw.Draw(mask).ellipse((0, 0, w - 1, h - 1), fill=255)
        bg = Image.new("RGB", (w, h), (60, 60, 60))
        img = Image.composite(img, bg, mask)
    img.save(path)
    drain(s, 0.3, echo=False)
    print("kép mentve:", path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM10")
    ap.add_argument("action", choices=["log", "cmd", "shot", "script"])
    ap.add_argument("args", nargs="*")
    a = ap.parse_args()
    s = open_port(a.port)
    try:
        if a.action == "log":
            drain(s, float(a.args[0]) if a.args else 3)
        elif a.action == "cmd":
            for c in a.args:
                send(s, c, 0.6)
        elif a.action == "shot":
            shot(s, a.args[0] if a.args else "shot.png")
        elif a.action == "script":
            for raw in open(a.args[0], encoding="utf-8"):
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                if line.startswith("shot "):
                    shot(s, line[5:].strip())
                elif line.startswith("wait "):
                    drain(s, int(line[5:]) / 1000.0)
                else:
                    print(">", line)
                    send(s, line, 0.5)
    finally:
        s.close()


if __name__ == "__main__":
    main()
