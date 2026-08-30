#!/usr/bin/env python3
"""
Generate the C++ and MicroPython panel tables from panels.yaml.

    python3 tools/gen_panels.py           # write the generated files
    python3 tools/gen_panels.py --check   # fail if they are stale (CI)

panels.yaml is the single source of truth. The two generated files must never
be hand-edited — this script overwrites them.

llms.txt (the file customers paste into an AI assistant) is a third consumer.
Only its panel table is generated, spliced between two markers; the prose
around them is hand-written. It is generated for the same reason the bindings
are: llms.txt is the only document most customers will ever put in front of an
AI, so a panel table that quietly goes stale there is worse than one that goes
stale in a header — the compiler never sees it.
"""
import argparse
import pathlib
import re
import sys

try:
    import yaml
except ImportError:
    sys.exit("PyYAML is required:  pip install pyyaml")

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "panels.yaml"
OUT_H = ROOT / "src" / "LB_Panels.h"
OUT_WIRING = ROOT / "src" / "LB_Wiring.h"
OUT_PY = ROOT / "micropython" / "lb_panels.py"
OUT_LLMS = ROOT / "llms.txt"

# The MicroPython driver modules, scanned for the classes they actually define
# so llms.txt can never claim MicroPython support that does not exist.
MP_MODULES = [ROOT / "micropython" / "st77xx.py", ROOT / "micropython" / "nv3007.py"]

BANNER = "GENERATED FROM panels.yaml BY tools/gen_panels.py — DO NOT EDIT"

LLMS_BEGIN = "<!-- BEGIN GENERATED PANEL TABLE — panels.yaml via tools/gen_panels.py -->"
LLMS_END = "<!-- END GENERATED PANEL TABLE -->"

DRIVERS = ["ST7735", "ST7789", "ST7796", "NV3007", "ILI9341"]
INIT_OPS = {None: "LB_INIT_NONE", "nv3007_279": "LB_INIT_NV3007_279"}


def const_name(panel) -> str:
    """tft_24 -> LB_TFT_24. An explicit `const:` wins, so an id can stay precise
    for device_info while the name a sketch uses stays short."""
    return panel.get("const") or ("LB_" + panel["id"].upper())


def mp_name(panel) -> str:
    """LB_TFT_24 -> TFT_24. MicroPython drops the prefix; C has no namespaces
    and Python does."""
    return const_name(panel)[3:]


def mp_cls(panel) -> str:
    """The MicroPython driver class a panel asks for. The two NV3007 panels are
    the same silicon with different init tables, so they are separate classes
    there rather than a runtime flag."""
    if panel["driver"] == "NV3007":
        return "NV3007_279" if panel.get("init_ops") == "nv3007_279" else "NV3007_168"
    return panel["driver"]


def mp_classes() -> set:
    """The classes the MicroPython drivers really define. Read from the source
    rather than listed here, so a panel whose driver was only ever written for
    Arduino is reported as unsupported instead of silently promised."""
    found = set()
    for path in MP_MODULES:
        found |= set(re.findall(r"^class (\w+)", path.read_text(), re.M))
    return found


def cbool(v) -> str:
    return "true" if v else "false"


# ── C++ ──────────────────────────────────────────────────────────────────────

def gen_header(doc) -> str:
    panels = doc["panels"]
    L = [
        f"// {BANNER}",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "enum LB_Driver : uint8_t {",
    ]
    L += [f"  LB_DRV_{d}," for d in DRIVERS]
    L += [
        "};",
        "",
        "// Panels whose controller needs a vendor init table that differs from the",
        "// driver's built-in default (same silicon, different voltage/gamma).",
        "enum LB_InitOps : uint8_t { LB_INIT_NONE, LB_INIT_NV3007_279 };",
        "",
        "struct LB_PanelDef {",
        "  const char      *id;",
        "  const char      *name;",
        "  LB_Driver        driver;",
        "  uint16_t         width;       // native size at `rotation`",
        "  uint16_t         height;",
        "  uint8_t          rotation;    // the orientation the panel ships in",
        "  bool             bgr;",
        "  bool             invert;",
        "  bool             flipX;",
        "  bool             flipY;",
        "  int16_t          colOff1;     // applied at rotation 0 / 2",
        "  int16_t          rowOff1;",
        "  int16_t          colOff2;     // applied at rotation 1 / 3",
        "  int16_t          rowOff2;",
        "  int32_t          spiHz;",
        "  bool             blActiveLow; // LOW turns the backlight ON",
        "  LB_InitOps       initOps;",
        "};",
        "",
        f"static const LB_PanelDef LB_PANELS[] = {{",
    ]
    for p in panels:
        off = p["offsets"]
        L.append(
            "  {{ \"{id}\", \"{name}\", LB_DRV_{drv}, {w}, {h}, {rot}, "
            "{bgr}, {inv}, {fx}, {fy}, "
            "{o0}, {o1}, {o2}, {o3}, {hz}, "
            "{bla}, {ops} }},".format(
                id=p["id"], name=p["name"], drv=p["driver"],
                w=p["width"], h=p["height"], rot=p["rotation"],
                bgr=cbool(p.get("bgr")),
                inv=cbool(p.get("invert")), fx=cbool(p.get("flip_x")),
                fy=cbool(p.get("flip_y")),
                o0=off[0], o1=off[1], o2=off[2], o3=off[3],
                hz=p["spi_hz"],
                bla=cbool(p["backlight_active_low"]),
                ops=INIT_OPS[p.get("init_ops")],
            )
        )
    L += ["};", "", f"#define LB_PANEL_COUNT {len(panels)}", ""]
    width = max(len(const_name(p)) for p in panels)

    L.append("// Pass one of these to LB_Display. Changing this constant is the only")
    L.append("// edit needed to swap panels — everything else is inherited.")
    L.append("")
    L.append("// ── Lonely Binary panels ─────────────────────────────────────")
    for i, p in enumerate(panels):
        if not p.get("product", True):
            continue
        L.append(
            f"#define {const_name(p):<{width}} (&LB_PANELS[{i}])"
            f"   // {p['name']} {p['width']}x{p['height']} {p['driver']}"
        )

    others = [(i, p) for i, p in enumerate(panels) if not p.get("product", True)]
    if others:
        L.append("")
        L.append("// ── Panels we do not sell, but can drive ─────────────────────")
        L.append("// Named by controller, not by size, so they cannot be confused with")
        L.append("// the products above. If you bought a display from us it is up there.")
        for i, p in others:
            L.append(
                f"#define {const_name(p):<{width}} (&LB_PANELS[{i}])"
                f"   // {p['driver']} {p['width']}x{p['height']}"
            )
    L.append("")
    return "\n".join(L)


def gen_wiring(doc) -> str:
    w = doc["wiring"]
    s3, c = w["esp32s3"], w["esp32"]
    L = [
        f"// {BANNER}",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "// Every panel in the range shares one 15-pin FPC and one breakout, so the",
        "// pins depend on the MCU only — never on the panel. Written once here",
        "// instead of once per example sketch.",
        "",
        "struct LB_Wiring {",
        "  int8_t  cs, rst, dc, mosi, sclk, backlight, miso;",
        "  uint8_t spiHost;",
        "  bool    psram;",
        "};",
        "",
        "#if defined(CONFIG_IDF_TARGET_ESP32S3)",
        f"  #define LB_WIRING_NAME \"ESP32-S3\"",
        "  static const LB_Wiring LB_WIRING = {"
        f" {s3['cs']}, {s3['rst']}, {s3['dc']}, {s3['mosi']}, {s3['sclk']},"
        f" {s3['backlight']}, {s3['miso']}, {s3['spi_host']}, {cbool(s3['psram'])} }};",
        "#else",
        f"  #define LB_WIRING_NAME \"ESP32\"",
        "  // GPIO 41/42 do not exist on a classic ESP32 and 6-11 are the flash bus,",
        "  // so the S3 pins above cannot be reused. These are the standard VSPI pins.",
        "  static const LB_Wiring LB_WIRING = {"
        f" {c['cs']}, {c['rst']}, {c['dc']}, {c['mosi']}, {c['sclk']},"
        f" {c['backlight']}, {c['miso']}, {c['spi_host']}, {cbool(c['psram'])} }};",
        "#endif",
        "",
    ]
    return "\n".join(L)


# ── MicroPython ──────────────────────────────────────────────────────────────

def gen_python(doc) -> str:
    panels, w = doc["panels"], doc["wiring"]
    L = [
        f'"""{BANNER}"""',
        "",
        "# Wiring is per-MCU, not per-panel — the whole range shares one breakout.",
        "WIRING = {",
    ]
    for key in ("esp32s3", "esp32"):
        d = w[key]
        L.append(
            f'    "{key}": {{"cs": {d["cs"]}, "rst": {d["rst"]}, "dc": {d["dc"]}, '
            f'"mosi": {d["mosi"]}, "sclk": {d["sclk"]}, '
            f'"backlight": {d["backlight"]}, "spi_id": {d["mp_spi_id"]}, '
            f'"psram": {bool(d["psram"])}}},'
        )
    L += ["}", "", "# The MicroPython driver takes a single xstart/ystart, so the offset pair", "# matching each panel's default rotation is pre-selected here.", ""]

    names = []
    for p in panels:
        off = p["offsets"]
        xs, ys = (off[2], off[3]) if p["rotation"] % 2 else (off[0], off[1])
        name = mp_name(p)
        names.append(name)
        cls = mp_cls(p)
        L += [
            f"{name} = {{",
            f'    "id": "{p["id"]}",',
            f'    "name": "{p["name"]}",',
            f'    "cls": "{cls}",',
            f'    "width": {p["width"]},',
            f'    "height": {p["height"]},',
            f'    "rotation": {p["rotation"]},',
            f'    "xstart": {xs},',
            f'    "ystart": {ys},',
            f'    "bgr": {bool(p.get("bgr"))},',
            f'    "invert": {bool(p.get("invert"))},',
            f'    "flip_x": {bool(p.get("flip_x"))},',
            f'    "flip_y": {bool(p.get("flip_y"))},',
            f'    "baudrate": {p["spi_hz"]},',
            f'    "bl_active_low": {bool(p["backlight_active_low"])},',
            "}",
            "",
        ]
    L.append("# Panels Lonely Binary sells.")
    L.append("PRODUCTS = {")
    L += [f'    "{p["id"]}": {n},' for p, n in zip(panels, names)
          if p.get("product", True)]
    L += ["}", "", "# Everything the library can drive, products included.", "PANELS = {"]
    L += [f'    "{p["id"]}": {n},' for p, n in zip(panels, names)]
    L += ["}", ""]
    return "\n".join(L)


# ── llms.txt (the AI-facing panel table) ─────────────────────────────────────

def mhz(hz: int) -> str:
    return f"{hz / 1_000_000:g} MHz"


def screen_size(panel) -> str:
    """What the sketch actually gets. A panel that ships at rotation 1 reports
    its axes swapped, so quoting the native size here would have an AI laying
    out a 170-pixel-wide UI on a 320-pixel-wide screen."""
    w, h = panel["width"], panel["height"]
    return f"{h} x {w}" if panel["rotation"] % 2 else f"{w} x {h}"


def gen_llms_panels(doc) -> str:
    panels = doc["panels"]
    have = mp_classes()

    def row(p, label):
        cls = mp_cls(p)
        mp = f"`{mp_name(p)}`" if cls in have else "not supported"
        return (
            f"| {label} | `{const_name(p)}` | {mp} | {screen_size(p)} | "
            f"{p['driver']} | {mhz(p['spi_hz'])} | "
            f"{'active low' if p['backlight_active_low'] else 'active high'} |"
        )

    head = [
        "| Panel | Arduino constant | MicroPython name | Screen size | Driver IC | SPI clock | Backlight |",
        "|---|---|---|---|---|---|---|",
    ]

    products = [p for p in panels if p.get("product", True)]
    others = [p for p in panels if not p.get("product", True)]

    L = [
        "### Panels Lonely Binary sells",
        "",
        "Pick by the size printed on the display. That is the whole decision, and",
        "there is never more than one entry for a size. Everything else in this",
        "table follows from the constant — do not pass any of it to the library by",
        "hand.",
        "",
    ] + head + [row(p, p["name"]) for p in products]

    rotated = [p for p in panels if p["rotation"] % 2]
    if rotated:
        L += [
            "",
            "**Screen size is the size after the panel's default rotation**, which is",
            "what `width()` / `height()` return and what you should lay a UI out",
            "against. These ship rotated (landscape), so their size here is the",
            "transpose of the raw glass:",
            "",
        ]
        L += [
            f"- `{const_name(p)}` — glass is {p['width']} x {p['height']}, "
            f"ships at rotation {p['rotation']}, so you get {screen_size(p)}"
            for p in rotated
        ]

    if others:
        L += [
            "",
            "### Panels the library can drive but Lonely Binary does not sell",
            "",
            "**If the customer bought a display from Lonely Binary, it is in the table",
            "above and this one is irrelevant.** These are named by controller and",
            "resolution rather than by size, precisely so they cannot be mistaken for a",
            "product. Values are measured on one sample; another sample of the same",
            "controller may well need `setColorOrder()` / `setInverted()` /",
            "`setBacklightActiveLow()`.",
            "",
        ] + head + [row(p, p["driver"]) for p in others]

    return "\n".join(L)


def gen_llms(doc, current: str) -> str:
    """Splice the panel table into the hand-written llms.txt."""
    start, end = current.find(LLMS_BEGIN), current.find(LLMS_END)
    if start < 0 or end < 0:
        sys.exit(f"{OUT_LLMS.name} is missing the generated-panel-table markers")
    return (current[:start + len(LLMS_BEGIN)]
            + "\n\n" + gen_llms_panels(doc) + "\n\n"
            + current[end:])


# ── Driver ───────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if any generated file is out of date")
    args = ap.parse_args()

    doc = yaml.safe_load(SRC.read_text())
    if not OUT_LLMS.exists():
        sys.exit(f"{OUT_LLMS.name} is missing — it is hand-written, not generated")
    outputs = {
        OUT_H: gen_header(doc),
        OUT_WIRING: gen_wiring(doc),
        OUT_PY: gen_python(doc),
        OUT_LLMS: gen_llms(doc, OUT_LLMS.read_text()),
    }

    stale = []
    for path, text in outputs.items():
        current = path.read_text() if path.exists() else None
        if current == text:
            continue
        if args.check:
            stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(ROOT)}")

    if stale:
        print("Generated files are stale — run: python3 tools/gen_panels.py")
        for p in stale:
            print(f"  {p}")
        return 1
    if args.check:
        print(f"panel tables up to date ({len(doc['panels'])} panels)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
