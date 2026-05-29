#!/usr/bin/env python3
"""Convert PNG assets to RGB565 C++ sources for TFT pushPixels (byte-swapped)."""

import argparse
import re
import sys
from pathlib import Path

from PIL import Image

# Full CYD keyboard set (must match assets/keyboard/ and firmware key lookup).
REQUIRED_KEYBOARD_PNGS: tuple[str, ...] = (
    "0.png",
    "1.png",
    "2.png",
    "3.png",
    "4.png",
    "5.png",
    "6.png",
    "7.png",
    "8.png",
    "9.png",
    "a.png",
    "b.png",
    "c.png",
    "caps.png",
    "d.png",
    "del.png",
    "e.png",
    "enter.png",
    "f.png",
    "g.png",
    "h.png",
    "i.png",
    "j.png",
    "k.png",
    "l.png",
    "m.png",
    "menu.png",
    "n.png",
    "o.png",
    "p.png",
    "q.png",
    "r.png",
    "r1.png",
    "r2.png",
    "r3.png",
    "r4.png",
    "s.png",
    "space-left.png",
    "space-right.png",
    "sym.png",
    "t.png",
    "u.png",
    "v.png",
    "w.png",
    "x.png",
    "y.png",
    "z.png",
)

# Included in output when present; not required for validation.
OPTIONAL_KEYBOARD_PNGS: tuple[str, ...] = ("blank.png",)


def keyboard_pngs_to_convert(input_dir: Path) -> list[str]:
    """Required PNGs plus any optional files that exist in input_dir."""
    names = list(REQUIRED_KEYBOARD_PNGS)
    for name in OPTIONAL_KEYBOARD_PNGS:
        if (input_dir / name).is_file():
            names.append(name)
    return names


def validate_keyboard_folder(input_dir: Path) -> list[str]:
    """Return names of required PNGs that are missing from input_dir."""
    missing: list[str] = []
    for name in REQUIRED_KEYBOARD_PNGS:
        if not (input_dir / name).is_file():
            missing.append(name)
    return missing


def require_keyboard_folder(input_dir: Path) -> None:
    missing = validate_keyboard_folder(input_dir)
    if not missing:
        return
    print(f"ERROR: {input_dir} is missing required keyboard PNG(s):", file=sys.stderr)
    for name in missing:
        print(f"  - {name}", file=sys.stderr)
    print(
        f"Expected {len(REQUIRED_KEYBOARD_PNGS)} files; missing {len(missing)}.",
        file=sys.stderr,
    )
    sys.exit(1)


def rgba_to_rgb565(r, g, b):
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return ((rgb565 & 0x00FF) << 8) | ((rgb565 & 0xFF00) >> 8)


def stem_to_prefix(stem: str) -> str:
    """e.g. space-left -> space_left for aImageWidth-style symbols."""
    prefix = re.sub(r"[^a-zA-Z0-9_]", "_", stem.replace("-", "_"))
    if prefix and prefix[0].isdigit():
        prefix = "key_" + prefix
    return prefix


def write_kbd_bin(output_dir: Path, prefix: str, width: int, height: int, pixels_rgb) -> None:
    """Write CYDK binary for SD keyboard-theme (same RGB565 byte order as firmware)."""
    out_path = output_dir / f"{prefix}.kbd"
    with out_path.open("wb") as f:
        f.write(b"CYDK")
        f.write(width.to_bytes(2, "little"))
        f.write(height.to_bytes(2, "little"))
        for y in range(height):
            for x in range(width):
                r, g, b = pixels_rgb[y * width + x]
                rgb565 = rgba_to_rgb565(r, g, b)
                f.write(rgb565.to_bytes(2, "little"))


def convert_png(
    input_path: Path,
    output_dir: Path,
    prefix: str | None = None,
    bin_dir: Path | None = None,
) -> str:
    if prefix is None:
        prefix = stem_to_prefix(input_path.stem)

    with Image.open(input_path) as img:
        img = img.convert("RGB")
        width, height = img.size
        pixels = list(img.getdata())

    symbol = f"{prefix}Image"
    header_path = output_dir / f"{prefix}_image.h"
    source_path = output_dir / f"{prefix}_image.cpp"

    header_path.write_text(
        "#pragma once\n\n"
        "#include <cstdint>\n\n"
        f"extern const uint16_t {symbol}Width;\n"
        f"extern const uint16_t {symbol}Height;\n\n"
        f"extern const uint16_t {symbol}Data[];\n",
        encoding="utf-8",
    )

    lines = ["#include <cstdint>\n", f'#include "{prefix}_image.h"\n\n']
    lines.append(f"const uint16_t {symbol}Width = {width};\n")
    lines.append(f"const uint16_t {symbol}Height = {height};\n\n")
    lines.append(f"const uint16_t {symbol}Data[] = {{\n")

    for y in range(height):
        row_parts = []
        for x in range(width):
            r, g, b = pixels[y * width + x]
            rgb565 = rgba_to_rgb565(r, g, b)
            row_parts.append(f"0x{rgb565:04x}")
        lines.append(", ".join(row_parts) + ",\n")

    lines.append("};\n")
    source_path.write_text("".join(lines), encoding="utf-8")

    if bin_dir is not None:
        bin_dir.mkdir(parents=True, exist_ok=True)
        write_kbd_bin(bin_dir, prefix, width, height, pixels)

    return prefix


def pixels_to_rgb565_bytes(width: int, height: int, pixels_rgb) -> bytes:
    out = bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b = pixels_rgb[y * width + x]
            rgb565 = rgba_to_rgb565(r, g, b)
            out.extend(rgb565.to_bytes(2, "little"))
    return bytes(out)


def load_png_pixels(input_path: Path) -> tuple[str, int, int, list]:
    prefix = stem_to_prefix(input_path.stem)
    with Image.open(input_path) as img:
        img = img.convert("RGB")
        width, height = img.size
        pixels = list(img.getdata())
    return prefix, width, height, pixels


def build_keyboard_pack(input_dir: Path, pack_path: Path) -> None:
    """Build a CYKT pack file for <game>.kbd alongside a .tap/.tzx on SD."""
    require_keyboard_folder(input_dir)
    items: list[tuple[str, int, int, bytes]] = []
    for name in keyboard_pngs_to_convert(input_dir):
        png = input_dir / name
        prefix, width, height, pixels = load_png_pixels(png)
        pix = pixels_to_rgb565_bytes(width, height, pixels)
        items.append((prefix, width, height, pix))
        print(f"  pack: {name} -> {prefix} ({width}x{height})")

    payload = bytearray()
    meta: list[tuple[str, int, int, int]] = []
    for prefix, width, height, pix in items:
        meta.append((prefix, width, height, 8 + len(payload)))
        payload.extend(pix)

    table = bytearray(b"CYKT")
    table.extend((1).to_bytes(2, "little"))
    table.extend(len(meta).to_bytes(2, "little"))
    for prefix, width, height, offset in meta:
        name = prefix.encode("ascii")
        if len(name) > 31:
            raise ValueError(f"asset name too long: {prefix}")
        table.append(len(name))
        table.extend(name)
        table.extend(width.to_bytes(2, "little"))
        table.extend(height.to_bytes(2, "little"))
        table.extend(offset.to_bytes(4, "little"))

    pack_path.parent.mkdir(parents=True, exist_ok=True)
    pack_path.write_bytes(table + payload)
    print(f"Wrote keyboard pack {pack_path} ({len(meta)} keys, {len(table) + len(payload)} bytes)")


def convert_directory(input_dir: Path, output_dir: Path, bin_dir: Path | None = None) -> list[str]:
    require_keyboard_folder(input_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    prefixes = []
    for name in keyboard_pngs_to_convert(input_dir):
        png = input_dir / name
        prefix = convert_png(png, output_dir, bin_dir=bin_dir)
        prefixes.append(prefix)
        print(f"  {name} -> {prefix}_image.{{h,cpp}} ({prefix}ImageWidth x Height)")

    manifest = output_dir / "keyboard_images.h"
    includes = "\n".join(f'#include "{p}_image.h"' for p in prefixes)
    manifest.write_text(
        "#pragma once\n\n"
        "// Auto-generated by utils/png_converter.py — do not edit by hand.\n\n"
        f"{includes}\n",
        encoding="utf-8",
    )
    print(f"Wrote manifest {manifest} ({len(prefixes)} images)")
    return prefixes


def main():
    parser = argparse.ArgumentParser(description="Convert PNG to RGB565 firmware image sources.")
    parser.add_argument("input", nargs="?", help="PNG file or directory (with --batch)")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output directory (default: firmware/src/Screens/images/keyboard for batch)",
    )
    parser.add_argument(
        "--batch",
        action="store_true",
        help="Convert every *.png in input directory",
    )
    parser.add_argument(
        "--bin-dir",
        type=Path,
        help="Also write .kbd binaries for SD keyboard-theme (use with --batch)",
    )
    parser.add_argument(
        "--pack",
        type=Path,
        metavar="FILE",
        help="Write a CYKT keyboard pack (.kbd) for a game tape (use with --batch)",
    )
    parser.add_argument(
        "--pack-only",
        action="store_true",
        help="Only write --pack; do not update firmware C++ keyboard images",
    )
    args = parser.parse_args()

    if not args.input:
        parser.print_help()
        sys.exit(1)

    input_path = Path(args.input)
    if args.batch or input_path.is_dir():
        if not input_path.is_dir():
            print(f"Not a directory: {input_path}")
            sys.exit(1)
        require_keyboard_folder(input_path)
        output_dir = args.output or Path("firmware/src/Screens/images/keyboard")
        print(
            f"Converting {input_path} -> {output_dir} "
            f"({len(REQUIRED_KEYBOARD_PNGS)} keyboard PNGs OK)"
        )
        if args.bin_dir:
            print(f"  SD theme binaries -> {args.bin_dir}")
        if args.pack:
            print(f"  keyboard pack -> {args.pack}")
            build_keyboard_pack(input_path, args.pack)
        if args.pack_only:
            if not args.pack:
                print("ERROR: --pack-only requires --pack FILE", file=sys.stderr)
                sys.exit(1)
            return
        convert_directory(input_path, output_dir, bin_dir=args.bin_dir)
        return

    output_dir = args.output or Path(".")
    output_dir.mkdir(parents=True, exist_ok=True)
    prefix = convert_png(input_path, output_dir, bin_dir=args.bin_dir)
    print(f"Conversion completed: {prefix}_image.h / {prefix}_image.cpp")


if __name__ == "__main__":
    main()
