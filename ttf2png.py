#!/usr/bin/env python
import sys
import os
import subprocess

from fontTools.ttLib import TTFont

TEXTS_DIR = "texts"

TTF_PATH = "JF-Dot-Kappa20-0213.ttf"
FONT_SIZE = 20
TTF_NAME, TTF_EXT = os.path.splitext(os.path.basename(TTF_PATH))

ttf = TTFont(TTF_PATH, 0, verbose=0, allowVID=0, ignoreDecompileErrors=True, fontNumber=-1)

if not os.path.isdir(TEXTS_DIR):
    os.mkdir(TEXTS_DIR)

for x in ttf["cmap"].tables:
    for y in x.cmap.items():
        try:
            char_unicode = unichr(y[0])
        except ValueError:
            continue
        char_utf8 = char_unicode.encode('utf_8')
        char_name = str(y[0])
        f = open(os.path.join(TEXTS_DIR, char_name), 'w')
        f.write(char_utf8)
        f.close()
ttf.close()
