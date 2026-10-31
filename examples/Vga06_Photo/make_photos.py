#!/usr/bin/env python3
"""
make_photos.py - turn your own pictures into the photos.h that Vga06_Photo embeds.

    pip install pillow
    python3 make_photos.py picture1.jpg picture2.png picture3.jpg [...]

Each picture is scaled to cover 800 x 600 (scaled up until it covers, then the
extra is cropped from the centre), saved as a BASELINE JPEG and written into
photos.h as a byte array. Up to 8 pictures; keep the total under about 1 MB or
the sketch will not fit the default partition (Tools > Partition Scheme >
"Huge APP" gives you 3 MB).

Why baseline: the decoder inside the library cannot read progressive JPEGs. It
does not crash, it just draws nothing - so this script refuses to write one.
"""
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("This script needs Pillow:  pip install pillow")

W, H = 800, 600
HERE = os.path.dirname(os.path.abspath(__file__))

if len(sys.argv) < 2:
    sys.exit(__doc__.strip())

out = ["// Made by make_photos.py - do not edit by hand.",
       "#pragma once", "#include <stdint.h>", ""]
names, total = [], 0
for n, path in enumerate(sys.argv[1:9]):
    im = Image.open(path).convert("RGB")
    s = max(W / im.width, H / im.height)
    im = im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS)
    l, t = (im.width - W) // 2, (im.height - H) // 2
    im = im.crop((l, t, l + W, t + H))
    tmp = os.path.join(HERE, "_tmp.jpg")
    im.save(tmp, "JPEG", quality=82, progressive=False, optimize=False, subsampling=2)
    data = open(tmp, "rb").read()
    os.remove(tmp)
    # SOF0 (0xFFC0) is baseline, SOF2 (0xFFC2) is progressive.
    assert b"\xff\xc0" in data and b"\xff\xc2" not in data, "not a baseline JPEG"
    name = f"PHOTO_{n}"
    names.append(name)
    total += len(data)
    out.append(f"// {os.path.basename(path)}  ({len(data)} bytes)")
    out.append(f"static const uint8_t {name}[] = {{")
    for i in range(0, len(data), 24):
        out.append("  " + ",".join(f"0x{b:02X}" for b in data[i:i + 24]) + ",")
    out.append("};\n")
out.append("struct PhotoRef { const uint8_t *data; uint32_t len; };")
out.append("static const PhotoRef PHOTOS[] = {")
for n in names:
    out.append(f"  {{{n}, sizeof({n})}},")
out.append("};")
out.append(f"static const int PHOTO_COUNT = {len(names)};")
open(os.path.join(HERE, "photos.h"), "w").write("\n".join(out) + "\n")
print(f"photos.h: {len(names)} pictures, {total} bytes of JPEG")
