#!/usr/bin/env python3
import math
import struct
import json
import sys


TYPEFACE_ATLAS = struct.Struct('<ifiiHH')
TYPEFACE_METRICS = struct.Struct('<fffff')
TYPEFACE_GLYPH = struct.Struct('<f')
TYPEFACE_BOUNDS = struct.Struct('<ffff')


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <mtsdf pixels bin> <glyphs json> <output format>")
        exit(-1)

    mtsdf_pixels_bin = sys.argv[1]
    glyphs_json = sys.argv[2]
    output_format = sys.argv[3]

    glyphs = json.load(open(glyphs_json))

    first = 64 * 1024
    last = 0

    computer_glyphs = [None] * 64 * 1024

    for glyph in glyphs['glyphs']:
        computer_glyphs[glyph['unicode']] = glyph
        if glyph['unicode'] < first:
            first = glyph['unicode']
        if glyph['unicode'] > last:
            last = glyph['unicode']

    assert last > first

    from PIL import Image, ImageColor

    im = Image.new('RGB', (512, 256))

    FONT_SIZE = 64

    # for x in range(512):
    #     for y in range(256):
    #         im.putpixel((x, y), 0x0000FF)

    pixels = struct.unpack('<' + str(glyphs['atlas']['width'] * glyphs['atlas']['height']) + 'I', open(mtsdf_pixels_bin, 'rb').read())

    full_atlas_width = int(glyphs['atlas']['width'])

    # for y in range(208):
    #     for x in range(208):
    #         im.putpixel((x, y), pixels[x + y * 208])

    def median(r, g, b):
        return max(min(r, g), min(max(r, g), b))

    def screen_px_range():
        print((FONT_SIZE / glyphs['atlas']['size']) * glyphs['atlas']['distanceRange'])
        return (FONT_SIZE / glyphs['atlas']['size']) * glyphs['atlas']['distanceRange']

    def clamp(d, minv, maxv):
        return max(minv, min(d, maxv))

    def mix(back, fore, opacity):
        br = (back & 0xFF)
        bg = ((back >> 8) & 0xFF)
        bb = ((back >> 16) & 0xFF)

        fr = (fore & 0xFF)
        fg = ((fore >> 8) & 0xFF)
        fb = ((fore >> 16) & 0xFF)

        r = int(fr * opacity + br * (1 - opacity)) & 0xFF
        g = int(fg * opacity + bg * (1 - opacity)) & 0xFF
        b = int(fb * opacity + bb * (1 - opacity)) & 0xFF

        return r | (g << 8) | (b << 16)

    def process_pixel(pix, bg, fg):
        r = (pix & 0xFF)
        g = ((pix >> 8) & 0xFF)
        b = ((pix >> 16) & 0xFF)
        sd = median(r, g, b) / 255
        screen_px_distance = screen_px_range() * (sd - 0.5)
        opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0)
        return mix(bg, fg, opacity)

    def draw_char(x, y, c, color):
        y += FONT_SIZE

        glyph = computer_glyphs[ord(c)]

        if 'planeBounds' not in glyph:
            return

        planeBounds = glyph['planeBounds']
        atlasBounds = glyph['atlasBounds']

        plane_width = int((planeBounds['right'] * FONT_SIZE) - (planeBounds['left'] * FONT_SIZE))
        plane_height = int((planeBounds['bottom'] * FONT_SIZE) - (planeBounds['top'] * FONT_SIZE))
        plane_x = x + int(planeBounds['left'] * FONT_SIZE)
        plane_y = y + int(planeBounds['top'] * FONT_SIZE)

        atlas_width = (atlasBounds['right']) - (atlasBounds['left'])
        atlas_height = (atlasBounds['bottom']) - (atlasBounds['top'])
        atlas_x = atlasBounds['left']
        atlas_y = atlasBounds['top']

        for yy in range(plane_height):
            for xx in range(plane_width):
                a_x = (xx / plane_width) * atlas_width
                a_y = (yy / plane_height) * atlas_height
                a_x = int(atlas_x + a_x)
                a_y = int(atlas_y + a_y)
                pix = pixels[a_x + a_y * full_atlas_width]

                pos = (plane_x + xx, plane_y + yy)
                print(hex(a_x), hex(a_y))
                pix = process_pixel(pix, 0xff242424, color)
                im.putpixel(pos, pix)

    def draw_string(x, y, s, color):
        for c in s:
            glyph = computer_glyphs[ord(c)]
            draw_char(x, y, c, color)
            x += int(glyph['advance'] * FONT_SIZE)

    draw_string(0, 0, 'Hello world!', 0x00FF00)

    im.save('test.png')

    with open(output_format, 'wb') as f:
        f.write(TYPEFACE_ATLAS.pack(
            glyphs['atlas']['distanceRange'],
            glyphs['atlas']['size'],
            glyphs['atlas']['width'],
            glyphs['atlas']['height'],
            first,
            last))

        f.write(TYPEFACE_METRICS.pack(
            glyphs['metrics']['lineHeight'],
            glyphs['metrics']['ascender'],
            glyphs['metrics']['descender'],
            glyphs['metrics']['underlineY'],
            glyphs['metrics']['underlineThickness'],
        ))

        print(f.tell())

        for glyph in computer_glyphs[first:last+1]:
            if glyph is None:
                f.write(TYPEFACE_GLYPH.pack(0))
                f.write(TYPEFACE_BOUNDS.pack(0, 0, 0, 0))
                f.write(TYPEFACE_BOUNDS.pack(0, 0, 0, 0))
            else:
                f.write(TYPEFACE_GLYPH.pack(glyph['advance']))
                if 'planeBounds' in glyph:
                    f.write(TYPEFACE_BOUNDS.pack(
                        glyph['planeBounds']['left'],
                        glyph['planeBounds']['top'],
                        glyph['planeBounds']['right'],
                        glyph['planeBounds']['bottom'],
                    ))
                else:
                    f.write(TYPEFACE_BOUNDS.pack(0, 0, 0, 0))

                if 'atlasBounds' in glyph:
                    f.write(TYPEFACE_BOUNDS.pack(
                        glyph['atlasBounds']['left'],
                        glyph['atlasBounds']['top'],
                        glyph['atlasBounds']['right'],
                        glyph['atlasBounds']['bottom'],
                    ))
                else:
                    f.write(TYPEFACE_BOUNDS.pack(0, 0, 0, 0))

        print(f.tell())

        f.write(open(mtsdf_pixels_bin, 'rb').read())
