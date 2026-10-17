#!/usr/bin/env python3
"""Export or edit a browser-compatible OLED JSON using only Python's standard library."""
import argparse
import json
import re
import struct
import zlib
from pathlib import Path


def validate(project):
    if any(project.get(k) != v for k, v in {'format':'oled-studio','version':1,'width':128,'height':64}.items()):
        raise ValueError('Expected OLED Studio version 1, 128x64')
    screens = project.get('screens')
    if not isinstance(screens, list) or not 1 <= len(screens) <= 200:
        raise ValueError('Expected 1 to 200 screens')
    ids = set()
    for screen in screens:
        if not isinstance(screen.get('id'), str) or not screen['id'] or screen['id'] in ids:
            raise ValueError('Screen IDs must be unique strings')
        ids.add(screen['id'])
        if not isinstance(screen.get('name'), str) or not screen['name'].strip() or len(screen['name']) > 80:
            raise ValueError('Invalid name')
        pixels = screen.get('pixels')
        if not isinstance(pixels, list) or len(pixels) != 8192 or any(type(v) is not int or v not in (0, 1) for v in pixels):
            raise ValueError('Expected 8192 binary pixels per screen')
    return project


def pack(pixels):
    return bytes(sum(pixels[i+b] << (7-b) for b in range(8)) for i in range(0,8192,8))


def png(pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
    raw = bytearray()
    for y in range(64):
        raw.append(0)
        for x in range(128):
            raw.extend(((255,227,83) if y < 16 else (57,202,255)) if pixels[y*128+x] else (0,0,0))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',128,64,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')


def header(pixels, screen_id):
    symbol = "oled_"+"_".join(f"{ord(c):x}" for c in screen_id)
    data = pack(pixels)
    rows = ['  '+', '.join(f'0x{v:02X}' for v in data[i:i+16])+',' for i in range(0,1024,16)]
    return '#pragma once\n#include <Arduino.h>\n\n// Include after declaring display. Row-major, MSB first.\nstatic const uint8_t '+symbol+'[] PROGMEM = {\n'+'\n'.join(rows)+'\n};\n\nstatic void show_'+symbol+'() {\n  display.clearDisplay();\n  display.drawBitmap(0, 0, '+symbol+', 128, 64, SSD1306_WHITE);\n  display.display();\n}\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('--screen', help='Exact screen ID or name; omit to list screens')
    parser.add_argument('--png', type=Path)
    parser.add_argument('--header', type=Path)
    parser.add_argument('--pixel', nargs=3, type=int, action='append', metavar=('X','Y','VALUE'), default=[])
    parser.add_argument('--output', type=Path, help='New JSON file for edits; original is preserved')
    args=parser.parse_args()
    try:
        project=validate(json.loads(args.project.read_text()))
        if args.screen is None:
            if args.png or args.header or args.pixel or args.output:
                parser.error('Choose --screen for edits or exports')
            for s in project['screens']:
                print(s['id']+'\t'+s['name'])
            return
        matches=[s for s in project['screens'] if args.screen in (s['id'], s['name'])]
        if len(matches)!=1:
            parser.error('Choose one unique screen ID or name')
        screen=matches[0]
        if args.pixel and not args.output:
            parser.error('--pixel requires --output for the edited JSON')
        for x,y,value in args.pixel:
            if not (0 <= x < 128 and 0 <= y < 64 and value in (0,1)):
                parser.error('Pixel must be X=0..127, Y=0..63, VALUE=0|1')
            screen['pixels'][y*128+x]=value
        if args.output:
            if args.output.resolve()==args.project.resolve():
                parser.error('Use a different --output to preserve the original')
            args.output.write_text(json.dumps(project,ensure_ascii=False,separators=(',',':'))+'\n')
        if args.png:
            args.png.write_bytes(png(screen['pixels']))
        if args.header:
            args.header.write_text(header(screen['pixels'], screen['id']))
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__=='__main__':
    main()
