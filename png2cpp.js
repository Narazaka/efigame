const fs = require('fs');
const { FileSystemObject } = require('fso');
const { PNG } = require('pngjs');

const fonts = new FileSystemObject("fonts");

let cppstr = "";

cppstr += `
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

typedef EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;

struct _Image {
    Pixel *pixels;
    UINT8 *alphas;
    int x;
    int y;
    int composition;
    int length;
};

typedef struct _Image Image;

static Pixel cfont_empty_pixel {0, 0, 0, 0};
`;

const lengths = {};

const empty_x = 11;
const empty_y = 22;
const empty_length = empty_x * empty_y;
let fontsStr = "";
fontsStr += `static UINT8 cfont_empty_alpha[] {`;
for (let i = 0; i < empty_length; ++i) fontsStr += `0,`;
fontsStr += `};\n`;
fontsStr += `
static Image cfont_empty {
  cfont_pixels_${empty_length},
  cfont_empty_alpha,
  ${empty_x},
  ${empty_y},
  4,
  ${empty_length},
};
`;
lengths[empty_length] = true;

const images = fonts.childrenSync();
const codes = [];
for (const image of images) {
    /** @type {PNG} */
    const png = PNG.sync.read(image.readFileSync());
    const length = png.width * png.height;
    if (length > 5000) continue;
    lengths[length] = true;
    const code = Number(image.basename().toString().replace(".png", ""));
    codes.push(code);
    fontsStr += `static UINT8 cfont_${code}_alpha[] {`;
    for (let i = 0; i < length; ++i) {
        fontsStr += `${png.data[i * 4 + 3]},`;
    }
    fontsStr += `};\n`;
    fontsStr += `static Image cfont_${code} {\n`;
    fontsStr += `  cfont_pixels_${length},\n`;
    fontsStr += `  cfont_${code}_alpha,\n`;
    fontsStr += `  ${png.width}, ${png.height}, 4, ${length}\n`;
    fontsStr += `};\n`;
}


fontsStr += `static Image cfonts[] {\n`;
const existCodes = {};
for (const code of codes) existCodes[code] = true;
const maxCode = Math.max(...codes);
for (let code = 0; code <= maxCode; ++code) {
    if (existCodes[code]) {
        fontsStr += `  cfont_${code},\n`;
    } else {
        fontsStr += `  cfont_empty,\n`;
    }
}
fontsStr += `};\n`;

function pixels(length) {
    let str = "";
    str += `static Pixel cfont_pixels_${length}[] {\n`;
    for (let i = 0; i < length; ++i) {
        str += `  cfont_empty_pixel,\n`;
    }
    str += `};\n`;
    return str;
}

for (const length of Object.keys(lengths)) {
    cppstr += pixels(Number(length));
}

cppstr += fontsStr;

const cpp = new FileSystemObject("cfonts.cpp");
cpp.writeFileSync(cppstr);
