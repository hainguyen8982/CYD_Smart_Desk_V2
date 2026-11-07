# -*- coding: utf-8 -*-
"""
generate_vn_fonts.py
Generates 4-bit Anti-Aliased Vietnamese UTF-8 PROGMEM fonts (VnFontData.h)
for CYD SmartDesk V2 (320x240 16-bit IPS ST7789).
Zero heap allocation, O(log N) binary search lookup, full Vietnamese glyph coverage.
"""

import os
from PIL import Image, ImageDraw, ImageFont

# Full Vietnamese character set + ASCII + UI symbols
ASCII_CHARS = "".join(chr(c) for c in range(0x20, 0x7F))
VN_CHARS = (
    "ÀÁÂÃÈÉÊÌÍÒÓÔÕÙÚÝàáâãèéêìíòóôõùúý"
    "ĂăĐđĨĩŨũƠơƯư"
    "ẠạẢảẤấẦầẨẩẪẫẬậẮắẰằẲẳẴẵẶặ"
    "ẸẹẺẻẼẽẾếỀềỂểỄễỆệ"
    "ỈỉỊị"
    "ỌọỎỏỐốỒồỔổỖỗỘộỚớỜờỞởỠỡỢợ"
    "ỤụỦủỨứỪừỬửỮữỰự"
    "ỲỳỴỵỶỷỸỹ"
)
SYMBOLS = "°•–—…×▲▼◀▶"

ALL_CHARS = sorted(set(ASCII_CHARS + VN_CHARS + SYMBOLS), key=lambda c: ord(c))


def find_font(candidates):
    font_dir = "C:/Windows/Fonts"
    for name in candidates:
        p = os.path.join(font_dir, name)
        if os.path.exists(p):
            return p
    raise FileNotFoundError(f"None of {candidates} found in {font_dir}")


def normalize_glyph_grid(grid, gw, gh, core_thresh=120, ridge_thresh=72):
    """
    Ensures uniform 100% stroke core brightness (nibble = 15) across all
    vertical stems, horizontal bars, curved spines, and Vietnamese diacritics
    so text never looks blotchy or uneven in brightness on a 140 PPI RGB565 LCD.
    """
    out = [[0] * gw for _ in range(gh)]
    for y in range(gh):
        for x in range(gw):
            v = grid[y][x]
            if v == 0:
                continue
            # 1. Strong coverage is always solid core (15)
            if v >= core_thresh:
                out[y][x] = 15
                continue

            # 2. Check if this pixel is the local peak (ridge) of a thin horizontal
            #    or vertical stroke / diacritic (where FreeType split a 1px stroke across 2 pixels)
            left = grid[y][x - 1] if x > 0 else 0
            right = grid[y][x + 1] if x + 1 < gw else 0
            up = grid[y - 1][x] if y > 0 else 0
            down = grid[y + 1][x] if y + 1 < gh else 0

            is_h_peak = (v >= ridge_thresh) and (v >= left) and (v >= right) and (left < core_thresh) and (right < core_thresh)
            is_v_peak = (v >= ridge_thresh) and (v >= up) and (v >= down) and (up < core_thresh) and (down < core_thresh)

            if is_h_peak or is_v_peak:
                out[y][x] = 15
            else:
                # Smooth outer anti-aliasing fringe (mapped cleanly to 0..13)
                norm = (v / float(core_thresh)) ** 0.85
                nib = int(round(norm * 13.0))
                if nib < 2:
                    nib = 0
                elif nib > 13:
                    nib = 13
                out[y][x] = nib
    return out


def generate_font_face(name, font_path, pt_size, line_height, baseline, tracking=0, core_thresh=118, ridge_thresh=70):
    font = ImageFont.truetype(font_path, pt_size)
    glyphs = []
    bitmaps = bytearray()

    for ch in ALL_CHARS:
        code = ord(ch)
        if code == 0x20:
            # Space character
            adv = max(3, int(round(font.getlength(" "))) + tracking)
            glyphs.append((code, 0, 0, adv, 0, 0, len(bitmaps)))
            continue

        # Render glyph directly at native integer pixel grid (NO bicubic resize!)
        cw, ch_h = 64, 64
        ox, oy = 12, baseline + 8
        img = Image.new("L", (cw, ch_h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((ox, oy), ch, fill=255, font=font, anchor="ls")

        bbox = img.getbbox()
        raw_adv = int(round(font.getlength(ch))) + tracking
        adv = max(1, raw_adv)

        if bbox is None:
            glyphs.append((code, 0, 0, adv, 0, 0, len(bitmaps)))
            continue

        x0, y0, x1, y1 = bbox
        gw = x1 - x0
        gh = y1 - y0
        x_off = x0 - ox
        # y_off relative to top of line box (where baseline is at `baseline` px from top)
        y_off = y0 - (oy - baseline)

        # Ensure advance width never clips glyph slightly on tight tracking
        if x_off + gw > adv and tracking < 0:
            adv = max(adv, x_off + gw)

        grid = [[img.getpixel((px, py)) for px in range(x0, x1)] for py in range(y0, y1)]
        norm_grid = normalize_glyph_grid(grid, gw, gh, core_thresh=core_thresh, ridge_thresh=ridge_thresh)

        offset = len(bitmaps)
        # Pack 2 pixels per byte (4-bit alpha: high nibble = even x, low nibble = odd x)
        raw_pixels = [norm_grid[ry][rx] for ry in range(gh) for rx in range(gw)]

        # Pad to even count if odd
        if len(raw_pixels) % 2 != 0:
            raw_pixels.append(0)

        for i in range(0, len(raw_pixels), 2):
            bitmaps.append((raw_pixels[i] << 4) | raw_pixels[i + 1])

        glyphs.append((code, gw, gh, adv, x_off, y_off, offset))

    space_w = next((g[3] for g in glyphs if g[0] == 0x20), 3)
    return {
        "name": name,
        "line_height": line_height,
        "baseline": baseline,
        "space_width": space_w,
        "glyphs": glyphs,
        "bitmaps": bitmaps,
    }


def measure_text(face, text):
    gmap = {g[0]: g for g in face["glyphs"]}
    w = 0
    for ch in text:
        g = gmap.get(ord(ch))
        if g:
            w += g[3]
        else:
            w += face["space_width"]
    return w


def main():
    font_small_path = find_font(["seguisb.ttf", "segoeuib.ttf", "tahomabd.ttf", "arialbd.ttf"])
    font_med_path = find_font(["segoeuib.ttf", "seguisb.ttf", "arialbd.ttf"])
    font_large_path = find_font(["segoeuib.ttf", "arialbd.ttf"])

    print(f"Using fonts: small={font_small_path}, med={font_med_path}, large={font_large_path}")

    # 1. Small font (replaces Font 1): native 11px without bicubic resize, uniform 100% core brightness
    face_small = generate_font_face(
        "VN_FONT_SMALL", font_small_path, pt_size=11, line_height=12, baseline=9, tracking=0, core_thresh=112, ridge_thresh=64
    )

    # 2. Medium font (replaces Font 2): native 13px bold, line_height=16 matching TFT_eSPI Font 2 height!
    face_med = generate_font_face(
        "VN_FONT_MEDIUM", font_med_path, pt_size=13, line_height=16, baseline=12, tracking=0, core_thresh=115, ridge_thresh=68
    )

    # 3. Large font (replaces Font 4 for text/UTF-8 headers): native 19px bold, line_height=24
    face_large = generate_font_face(
        "VN_FONT_LARGE", font_large_path, pt_size=19, line_height=24, baseline=19, tracking=0, core_thresh=118, ridge_thresh=70
    )

    for f in (face_small, face_med, face_large):
        print(f"{f['name']}: {len(f['glyphs'])} glyphs, {len(f['bitmaps'])} bitmap bytes")

    print("Sample widths (Small):")
    for s in ["CHUÔNG", "K.ĐỘNG", "ĐỔI WIFI", "TẬP TRUNG", "NGHỈ 5P", "NGHỈ 15P", "CHẠM XEM CHI TIẾT >"]:
        print(f"  '{s}' -> {measure_text(face_small, s)}px")

    print("Sample widths (Medium):")
    for s in [
        "Tiếng Việt",
        "Phím cơ",
        "Giọt nước",
        "THÁNG 11  •  2026",
        "THỨ SÁU, 06/11/2026",
        "Đang chạy deadline • Xin đừng làm phiền!",
    ]:
        print(f"  '{s}' -> {measure_text(face_med, s)}px")

    out_path = "include/VnFontData.h"
    with open(out_path, "w", encoding="utf-8") as fp:
        fp.write("// ═══════════════════════════════════════════════════════════════════════\n")
        fp.write("//  VnFontData.h — Auto-generated 4-bit Anti-Aliased Vietnamese Font Data\n")
        fp.write("//  Stored 100% in PROGMEM Flash (0 bytes RAM allocation)\n")
        fp.write("// ═══════════════════════════════════════════════════════════════════════\n")
        fp.write("#ifndef VN_FONT_DATA_H\n#define VN_FONT_DATA_H\n\n")
        fp.write("#include <Arduino.h>\n\n")
        fp.write("struct VnGlyph {\n")
        fp.write("    uint16_t unicode;\n")
        fp.write("    uint8_t  width;\n")
        fp.write("    uint8_t  height;\n")
        fp.write("    uint8_t  xAdvance;\n")
        fp.write("    int8_t   xOffset;\n")
        fp.write("    int8_t   yOffset;\n")
        fp.write("    uint32_t bitmapOffset;\n")
        fp.write("};\n\n")
        fp.write("struct VnFontFace {\n")
        fp.write("    uint16_t       glyphCount;\n")
        fp.write("    uint8_t        lineHeight;\n")
        fp.write("    uint8_t        baseline;\n")
        fp.write("    uint8_t        spaceWidth;\n")
        fp.write("    const VnGlyph* glyphs;\n")
        fp.write("    const uint8_t* bitmaps;\n")
        fp.write("};\n\n")

        for face in (face_small, face_med, face_large):
            name = face["name"]
            fp.write(f"static const uint8_t {name}_BITMAPS[] PROGMEM = {{\n    ")
            b = face["bitmaps"]
            for idx, byte in enumerate(b):
                fp.write(f"0x{byte:02X},")
                if (idx + 1) % 24 == 0:
                    fp.write("\n    ")
            fp.write("\n};\n\n")

            fp.write(f"static const VnGlyph {name}_GLYPHS[] PROGMEM = {{\n")
            for g in face["glyphs"]:
                fp.write(
                    f"    {{ 0x{g[0]:04X}, {g[1]}, {g[2]}, {g[3]}, {g[4]}, {g[5]}, {g[6]}U }},\n"
                )
            fp.write("};\n\n")

            fp.write(f"static const VnFontFace {name} = {{\n")
            fp.write(f"    {len(face['glyphs'])},\n")
            fp.write(f"    {face['line_height']},\n")
            fp.write(f"    {face['baseline']},\n")
            fp.write(f"    {face['space_width']},\n")
            fp.write(f"    {name}_GLYPHS,\n")
            fp.write(f"    {name}_BITMAPS\n")
            fp.write("};\n\n")

        fp.write("#endif // VN_FONT_DATA_H\n")

    print(f"Successfully wrote {out_path}!")


if __name__ == "__main__":
    main()

