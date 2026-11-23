"""Writes Data/Interface/Transporter_SF_Countdown.swf and _LRG.swf.

The menu SWF holds no art and no ActionScript: like the ship HUD, it imports
GravJumpComponent.swf (plus the fonts and button clips that file uses) with
zero-symbol ImportAssets2 tags, which makes that file's AS3 classes, such as
DefaultGravJumpComponent, available in this movie. Transporter_SF.dll then creates the
component and drives it from C++ (src/CountdownMenu.cpp).

Whichever GravJumpComponent.swf is installed is used: the game's, or a
replacement such as Grav Jump Component UI Overhaul.

Usage: python make_countdown_swf.py <output folder>"""
import os
import struct
import sys
import zlib

STAGE_W, STAGE_H = 1920, 1080   # the ship HUD's stage; the box sits where the HUD puts it
FRAME_RATE = 60
IMPORTS = ["fonts_en.swf", "ButtonClips.swf", "GravJumpComponent.swf"]


def tag(code, data=b""):
    if len(data) < 63:
        return struct.pack("<H", (code << 6) | len(data)) + data
    return struct.pack("<HI", (code << 6) | 63, len(data)) + data


def rect(xmax, ymax):
    # RECT with 0,xmax,0,ymax in twips; one bit width for all four fields
    values = [0, xmax * 20, 0, ymax * 20]
    nbits = max(v.bit_length() for v in values) + 1
    bits = format(nbits, "05b") + "".join(format(v, "0%db" % nbits) for v in values)
    bits += "0" * (-len(bits) % 8)
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))


def encoded_u32(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def movie(imports):
    body = rect(STAGE_W, STAGE_H) + struct.pack("<HH", FRAME_RATE << 8, 1)
    body += tag(69, struct.pack("<I", 0x08))                       # FileAttributes: ActionScript 3
    body += tag(9, b"\x00\x00\x00")                                # SetBackgroundColor
    body += tag(86, encoded_u32(1) + encoded_u32(0) + b"Scene 1\x00" + encoded_u32(0))  # one scene, no labels
    for url in imports:                                            # ImportAssets2, zero symbols (as the vanilla HUD)
        body += tag(71, url.encode("ascii") + b"\x00" + b"\x01\x00" + struct.pack("<H", 0))
    body += tag(1)                                                 # ShowFrame
    body += tag(0)                                                 # End
    compressed = zlib.compress(body, 9)
    return b"CWS" + bytes([12]) + struct.pack("<I", len(body) + 8) + compressed


def main(folder):
    os.makedirs(folder, exist_ok=True)
    for name in ("Transporter_SF_Countdown.swf", "Transporter_SF_Countdown_LRG.swf"):
        path = os.path.join(folder, name)
        with open(path, "wb") as f:
            f.write(movie(IMPORTS))
        print("wrote", path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "Data/Interface")
