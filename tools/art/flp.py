#!/usr/bin/env python3
"""[U-427] A tiny FL Studio project (.flp) reader / writer — no extra packages (PyFLP is not needed for this).

An FLP file: "FLhd" header (format, channels, PPQ) + "FLdt" chunk = a stream of events.
Event id < 64: 1 byte of data; 64-127: 2 bytes; 128-191: 4 bytes; ≥ 192: a varint length + that many bytes.
Pattern notes = event 224 (the notes of the pattern selected by the last event 65 "new pattern"), 24 bytes per note:
position u32, flags u16, rack channel u16, length u32, key u16, group u16, fine pitch u8, ?, release u8,
midi channel u8, pan u8, velocity u8, mod x u8, mod y u8.
"""
import struct, sys

def read(path):
    b = open(path, "rb").read()
    assert b[:4] == b"FLhd", "not an FL Studio project"
    hlen = struct.unpack_from("<I", b, 4)[0]
    fmt, nch, ppq = struct.unpack_from("<hHH", b, 8)
    p = 8 + hlen
    assert b[p:p + 4] == b"FLdt"
    dlen = struct.unpack_from("<I", b, p + 4)[0]
    p += 8
    end = p + dlen
    ev = []
    while p < end:
        i = b[p]; p += 1
        if i < 64: v = b[p:p + 1]; p += 1
        elif i < 128: v = b[p:p + 2]; p += 2
        elif i < 192: v = b[p:p + 4]; p += 4
        else:
            n = 0; s = 0
            while True:
                c = b[p]; p += 1
                n |= (c & 0x7F) << s; s += 7
                if not c & 0x80: break
            v = b[p:p + n]; p += n
        ev.append((i, v))
    return (fmt, nch, ppq), ev

def write(path, header, ev):
    fmt, nch, ppq = header
    data = bytearray()
    for i, v in ev:
        data.append(i)
        if i >= 192:
            n = len(v)
            while True:
                c = n & 0x7F; n >>= 7
                data.append(c | (0x80 if n else 0))
                if not n: break
        data += v
    out = b"FLhd" + struct.pack("<I", 6) + struct.pack("<hHH", fmt, nch, ppq) + b"FLdt" + struct.pack("<I", len(data)) + bytes(data)
    open(path, "wb").write(out)

def note(pos, length, key, vel=100, ch=0, pan=64):
    return struct.pack("<IHHIHHBBBBBBBB", pos, 0x4000, ch, length, key, 0, 120, 0, 64, 0, pan, vel, 128, 128)

def text(v):
    try:
        return v.decode("utf-16-le").rstrip("\0") if len(v) % 2 == 0 and b"\0" in v else v.decode("latin-1").rstrip("\0")
    except Exception:
        return repr(v[:20])

if __name__ == "__main__":
    hdr, ev = read(sys.argv[1])
    print("header", hdr, "events", len(ev))
    names = {64: "ChanNew", 65: "PatNew", 21: "ChanType", 192: "?", 196: "PluginName?", 201: "DefPluginName", 203: "PluginName", 224: "PatNotes", 193: "PatName"}
    for i, v in ev:
        if i in (64, 65, 21, 201, 203, 193, 224, 156, 199):
            t = text(v) if i >= 192 and i != 224 else (len(v) // 24 if i == 224 else int.from_bytes(v, "little"))
            print(i, names.get(i, ""), t)
