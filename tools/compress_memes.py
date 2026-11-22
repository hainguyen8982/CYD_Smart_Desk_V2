"""Compress 18 160x160 RGB565 bitmaps in include/MemeBitmaps.h into 8-bit Palette + RLE8."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_H = os.path.join(ROOT, "include", "MemeBitmaps.h")
OUT_H = os.path.join(ROOT, "include", "MemeBitmaps.h")


def rgb565_to_rgb888(c):
    r = ((c >> 11) & 0x1F) * 255 // 31
    g = ((c >> 5) & 0x3F) * 255 // 63
    b = (c & 0x1F) * 255 // 31
    return r, g, b


def rgb888_to_rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def quantize_median_cut(pixels_565, max_colors=255):
    """Quantize non-zero RGB565 pixels to <= max_colors using Pillow if available, else 4-4-4 / popularity."""
    non_zero = [p for p in pixels_565 if p != 0]
    unique = set(non_zero)
    if len(unique) <= max_colors:
        pal = [0] + sorted(unique)
        lut = {c: i for i, c in enumerate(pal)}
        indices = [lut[p] for p in pixels_565]
        return pal, indices

    try:
        from PIL import Image
        img = Image.new("RGB", (160, 160))
        img.putdata([rgb565_to_rgb888(p) for p in pixels_565])
        # Quantize using fast octree (method=2) to max_colors
        q = img.quantize(colors=max_colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
        raw_pal = q.getpalette()[: max_colors * 3]
        pal565 = [0]
        idx_map = {}
        for i in range(max_colors):
            r, g, b = raw_pal[i * 3 : i * 3 + 3]
            c565 = rgb888_to_rgb565(r, g, b)
            if c565 == 0:
                c565 = 0x0821  # Keep 0 exclusively for transparent black
            pal565.append(c565)
            idx_map[i] = len(pal565) - 1

        qdata = list(q.getdata())
        indices = [0 if pixels_565[i] == 0 else idx_map[qdata[i]] for i in range(len(pixels_565))]
        return pal565, indices
    except ImportError:
        # Pure-Python fallback: 3-bit/3-bit/2-bit bucket averaging
        buckets = {}
        for p in non_zero:
            key = ((p >> 13) & 0x7, (p >> 8) & 0x7, (p >> 3) & 0x3)
            if key not in buckets:
                buckets[key] = [0, 0, 0, 0]
            r, g, b = rgb565_to_rgb888(p)
            buckets[key][0] += r
            buckets[key][1] += g
            buckets[key][2] += b
            buckets[key][3] += 1
        pal = [0]
        key_to_idx = {}
        for k, (sr, sg, sb, cnt) in buckets.items():
            c565 = rgb888_to_rgb565(sr // cnt, sg // cnt, sb // cnt)
            if c565 == 0:
                c565 = 0x0821
            key_to_idx[k] = len(pal)
            pal.append(c565)
        indices = []
        for p in pixels_565:
            if p == 0:
                indices.append(0)
            else:
                k = ((p >> 13) & 0x7, (p >> 8) & 0x7, (p >> 3) & 0x3)
                indices.append(key_to_idx[k])
        return pal, indices


def rle8_encode(indices):
    """PackBits-style RLE8:
    - Control byte >= 0x80: repeat next byte (ctrl - 0x80 + 1) times (1..128)
    - Control byte < 0x80:  copy next (ctrl + 1) literal bytes (1..128)
    """
    out = bytearray()
    n = len(indices)
    i = 0
    while i < n:
        # Check for run of >= 3 identical bytes
        run_len = 1
        while i + run_len < n and run_len < 128 and indices[i + run_len] == indices[i]:
            run_len += 1
        if run_len >= 3:
            out.append(0x80 | (run_len - 1))
            out.append(indices[i])
            i += run_len
        else:
            lit_start = i
            lit_len = 0
            while i < n and lit_len < 128:
                # Stop literal if a run of >= 3 starts here
                if i + 2 < n and indices[i] == indices[i + 1] == indices[i + 2]:
                    break
                i += 1
                lit_len += 1
            out.append(lit_len - 1)
            out.extend(indices[lit_start : lit_start + lit_len])
    return bytes(out)


def rle8_decode_verify(rle_bytes, expected_len):
    out = []
    i = 0
    n = len(rle_bytes)
    while i < n:
        ctrl = rle_bytes[i]
        i += 1
        if ctrl & 0x80:
            cnt = (ctrl & 0x7F) + 1
            val = rle_bytes[i]
            i += 1
            out.extend([val] * cnt)
        else:
            cnt = ctrl + 1
            out.extend(rle_bytes[i : i + cnt])
            i += cnt
    assert len(out) == expected_len, f"Decoded {len(out)} != {expected_len}"
    return out


def main():
    with open(SRC_H, "r", encoding="utf-8") as f:
        text = f.read()

    if "MemeRleBitmap" in text:
        print("MemeBitmaps.h is already compressed!")
        return

    pattern = re.compile(
        r"const\s+uint16_t\s+(\w+)\s*\[\s*25600\s*\]\s*PROGMEM\s*=\s*\{([^}]+)\};",
        re.MULTILINE,
    )
    matches = list(pattern.finditer(text))
    print(f"Found {len(matches)} meme bitmaps.")
    assert len(matches) == 18, f"Expected 18 bitmaps, found {len(matches)}"

    total_raw = 0
    total_comp = 0
    out_lines = [
        "// ═══════════════════════════════════════════════════════════════════════",
        "//  MemeBitmaps.h — 18 Compressed 160x160 RGB565 Meme Sprites (Palette+RLE8)",
        "//  Auto-generated by tools/compress_memes.py (0 B RAM decode to TFT)",
        "// ═══════════════════════════════════════════════════════════════════════",
        "#ifndef MEME_BITMAPS_H",
        "#define MEME_BITMAPS_H",
        "",
        "#include <Arduino.h>",
        "",
        "#define MEME_BITMAP_W 160",
        "#define MEME_BITMAP_H 160",
        "#define MEME_TRANSPARENT_COLOR 0x0000",
        "",
        "struct MemeRleBitmap {",
        "    const uint16_t* palette;",
        "    const uint8_t*  rle;",
        "    uint32_t        rleSize;",
        "};",
        "",
    ]

    names = []
    for m in matches:
        name = m.group(1)
        names.append(name)
        vals = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", m.group(2))]
        assert len(vals) == 25600, f"{name}: expected 25600 pixels, got {len(vals)}"
        total_raw += len(vals) * 2

        pal, idx = quantize_median_cut(vals, max_colors=255)
        rle = rle8_encode(idx)
        dec = rle8_decode_verify(rle, 25600)
        assert dec == idx, f"{name}: RLE roundtrip mismatch!"

        comp_bytes = len(pal) * 2 + len(rle)
        total_comp += comp_bytes
        print(f"  {name:28s}: 51200 B -> {comp_bytes:6d} B ({len(pal):3d} colors, {len(rle):5d} B RLE)")

        out_lines.append(f"static const uint16_t {name}_pal[{len(pal)}] PROGMEM = {{")
        for i in range(0, len(pal), 16):
            chunk = ", ".join(f"0x{c:04X}" for c in pal[i : i + 16])
            out_lines.append(f"    {chunk},")
        out_lines.append("};")

        out_lines.append(f"static const uint8_t {name}_rle[{len(rle)}] PROGMEM = {{")
        for i in range(0, len(rle), 20):
            chunk = ", ".join(f"0x{b:02X}" for b in rle[i : i + 20])
            out_lines.append(f"    {chunk},")
        out_lines.append("};")
        out_lines.append(f"static const MemeRleBitmap {name} = {{ {name}_pal, {name}_rle, {len(rle)} }};")
        out_lines.append("")

    out_lines.append("static inline const MemeRleBitmap* getMemeBitmap(uint8_t pack, uint8_t mood) {")
    out_lines.append("    static const MemeRleBitmap* const TABLE[3][6] = {")
    for p in range(3):
        row = ", ".join(f"&{names[p * 6 + m]}" for m in range(6))
        out_lines.append(f"        {{ {row} }},")
    out_lines.append("    };")
    out_lines.append("    if (pack >= 3) pack = 0;")
    out_lines.append("    if (mood >= 6) mood = 0;")
    out_lines.append("    return TABLE[pack][mood];")
    out_lines.append("}")
    out_lines.append("")
    out_lines.append("#endif // MEME_BITMAPS_H")
    out_lines.append("")

    with open(OUT_H, "w", encoding="utf-8") as f:
        f.write("\n".join(out_lines))

    saved = total_raw - total_comp
    print(f"\nTOTAL: {total_raw:,} B -> {total_comp:,} B (SAVED {saved:,} B = {saved/1024:.1f} KB, {100*total_comp/total_raw:.1f}%)")


if __name__ == "__main__":
    main()
