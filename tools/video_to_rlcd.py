#!/usr/bin/env python3
"""
Turn a video into a 1-bit clip for a Lonely Binary reflective LCD.

    python3 tools/video_to_rlcd.py clip.webm --size 250x122 --fps 24 \
        --crop 16:12:448:334 --out clip.h

Needs ffmpeg on the PATH, and numpy and Pillow.

What it does, frame by frame:
  1. ffmpeg crops, scales (to fit inside the screen, black bars at the sides)
     and resamples to --fps.
  2. Colour becomes one grey level. Saturated colours are darkened a little
     (--sat-ink) so a bright dress does not vanish against a white floor.
  3. The grey is dithered to black and white. Ordered (Bayer) dithering is the
     default: it does not shimmer from frame to frame the way error diffusion
     does, and it compresses far better.
  4. Frames are packed 8 pixels to a byte, XORed with the frame before, and the
     result is run-length coded. A player decodes the stream in order and starts
     again from a blank frame when it loops.

Output (--out):  .h   a C array for PROGMEM (the format RLCD_Dance.ino reads)
                 .bin the same bytes

Stream format, all little-endian:
  header   'L','B','1','V', width u16, height u16, fps u8, frames u16
  per frame   u16 payload length, then the payload:
      token 0x00-0x7F  copy the next (token + 1) bytes
      token 0x80-0xFF  (token & 0x7F) + 1 zero bytes
  A frame is (width + 7) / 8 bytes a row, height rows, bit 7 the leftmost
  pixel, 1 = black. Each frame is stored as its XOR with the one before.
"""
import argparse
import subprocess
import sys

import numpy as np
from PIL import Image

BAYER8 = np.array([
    [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21]], dtype=np.float32)


def read_frames(path, w, h, fps, crop, fit):
    """Yield h x w x 3 uint8 frames. `fit` letterboxes inside w x h; otherwise fill and crop."""
    vf = []
    if crop:
        cw, ch, cx, cy = crop[2], crop[3], crop[0], crop[1]
        vf.append(f"crop={cw}:{ch}:{cx}:{cy}")
    vf.append(f"fps={fps}")
    if fit:
        vf.append(f"scale={w}:{h}:force_original_aspect_ratio=decrease:flags=lanczos")
        vf.append(f"pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:black")
    else:
        vf.append(f"scale={w}:{h}:force_original_aspect_ratio=increase:flags=lanczos")
        vf.append(f"crop={w}:{h}")
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-vf", ",".join(vf), "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    size = w * h * 3
    while True:
        buf = p.stdout.read(size)
        if len(buf) < size:
            break
        yield np.frombuffer(buf, np.uint8).reshape(h, w, 3)
    p.wait()


def to_grey(rgb, sat_ink, gamma, black, white):
    f = rgb.astype(np.float32) / 255.0
    luma = 0.299 * f[..., 0] + 0.587 * f[..., 1] + 0.114 * f[..., 2]
    mx, mn = f.max(-1), f.min(-1)
    sat = np.where(mx > 1e-3, (mx - mn) / np.maximum(mx, 1e-3), 0)
    g = luma * (1 - sat_ink * sat)
    g = np.clip((g - black) / (white - black), 0, 1) ** gamma
    return g


def dither(g, mode):
    h, w = g.shape
    if mode == "threshold":
        return g < 0.5
    if mode == "bayer":
        t = (np.tile(BAYER8, (h // 8 + 1, w // 8 + 1))[:h, :w] + 0.5) / 64.0
        return g < t                       # True = black
    # Floyd-Steinberg
    a = g.copy()
    out = np.zeros_like(g, bool)
    for y in range(h):
        for x in range(w):
            old = a[y, x]
            new = 1.0 if old >= 0.5 else 0.0
            out[y, x] = new == 0.0
            e = old - new
            if x + 1 < w: a[y, x + 1] += e * 7 / 16
            if y + 1 < h:
                if x > 0: a[y + 1, x - 1] += e * 3 / 16
                a[y + 1, x] += e * 5 / 16
                if x + 1 < w: a[y + 1, x + 1] += e * 1 / 16
    return out


def pack(black):
    h, w = black.shape
    return np.packbits(black, axis=1).tobytes()   # rows padded to a whole byte, bit 7 = leftmost


def rle(data):
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        if data[i] == 0:
            j = i
            while j < n and data[j] == 0 and j - i < 128:
                j += 1
            out.append(0x80 | (j - i - 1))
            i = j
        else:
            j = i
            while j < n and j - i < 128 and not (data[j] == 0 and j + 1 < n and data[j + 1] == 0):
                j += 1
            out.append(j - i - 1)
            out += data[i:j]
            i = j
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("video")
    ap.add_argument("--size", default="250x122", help="WxH of the screen (default 250x122, the 2.13 inch landscape)")
    ap.add_argument("--fps", type=int, default=24)
    ap.add_argument("--crop", help="x:y:w:h of the picture inside the video, to cut off black bars")
    ap.add_argument("--fill", action="store_true", help="fill the screen and crop, instead of fitting with bars")
    ap.add_argument("--dither", choices=["bayer", "fs", "threshold"], default="bayer")
    ap.add_argument("--sat-ink", type=float, default=0.55, help="how much saturated colour is darkened, 0-1")
    ap.add_argument("--gamma", type=float, default=1.0)
    ap.add_argument("--black", type=float, default=0.05, help="grey level that becomes black")
    ap.add_argument("--white", type=float, default=0.80, help="grey level that becomes white")
    ap.add_argument("--max-seconds", type=float, default=0)
    ap.add_argument("--out", help=".h or .bin")
    ap.add_argument("--preview", help="write a PNG contact sheet of every Nth frame (--preview-every)")
    ap.add_argument("--preview-every", type=int, default=24)
    a = ap.parse_args()

    w, h = (int(v) for v in a.size.lower().split("x"))
    crop = tuple(int(v) for v in a.crop.split(":")) if a.crop else None
    rowb = (w + 7) // 8
    frames, previews = [], []
    prev = bytes(rowb * h)
    total_in = 0
    for n, rgb in enumerate(read_frames(a.video, w, h, a.fps, crop, not a.fill)):
        if a.max_seconds and n >= a.max_seconds * a.fps:
            break
        black = dither(to_grey(rgb, a.sat_ink, a.gamma, a.black, a.white), a.dither)
        raw = pack(black)
        diff = bytes(x ^ y for x, y in zip(raw, prev))
        payload = rle(diff)
        frames.append(payload)
        total_in += len(raw)
        prev = raw
        if a.preview and n % a.preview_every == 0:
            previews.append(np.where(black, 0, 255).astype(np.uint8))

    stream = bytearray(b"LB1V")
    stream += w.to_bytes(2, "little") + h.to_bytes(2, "little") + bytes([a.fps]) + len(frames).to_bytes(2, "little")
    for p in frames:
        stream += len(p).to_bytes(2, "little") + p
    print(f"{len(frames)} frames, {len(frames) / a.fps:.1f} s, {total_in // 1024} KB raw -> {len(stream) // 1024} KB "
          f"({100 * len(stream) // max(total_in, 1)} %), largest frame {max(len(p) for p in frames)} bytes", file=sys.stderr)

    if a.preview and previews:
        cols = 4
        rows = (len(previews) + cols - 1) // cols
        sheet = Image.new("L", (cols * (w + 4), rows * (h + 4)), 128)
        for i, p in enumerate(previews):
            sheet.paste(Image.fromarray(p), ((i % cols) * (w + 4), (i // cols) * (h + 4)))
        sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
        sheet.save(a.preview)

    if a.out:
        if a.out.endswith(".bin"):
            open(a.out, "wb").write(stream)
        else:
            name = "lb_clip"
            with open(a.out, "w") as f:
                f.write(f"// Made by tools/video_to_rlcd.py - {w}x{h}, {a.fps} fps, {len(frames)} frames.\n")
                f.write("#pragma once\n#include <pgmspace.h>\n")
                f.write(f"static const uint8_t {name}[] PROGMEM = {{\n")
                for i in range(0, len(stream), 24):
                    f.write("  " + ",".join(f"0x{b:02X}" for b in stream[i:i + 24]) + ",\n")
                f.write("};\n")
                f.write(f"static const uint32_t {name}_len = {len(stream)};\n")
                f.write(f"static const uint16_t {name}_w = {w}, {name}_h = {h}, {name}_fps = {a.fps}, {name}_frames = {len(frames)};\n")


if __name__ == "__main__":
    main()
