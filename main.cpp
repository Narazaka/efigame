#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <cmath>

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

namespace EfiGame {
  typedef CHAR16* STRING;

  static EFI_SYSTEM_TABLE *SystemTable;

  namespace Input {
    /** キー入力1つを読み込む キー入力まで待機する */
    auto getChar() {
      EFI_INPUT_KEY key;
      UINT64 waitIdx;
      SystemTable->BootServices->WaitForEvent(1, &(SystemTable->ConIn->WaitForKey), &waitIdx);
      SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
      return key.UnicodeChar;
    }

    /** 改行が来るまでキー入力を読み込む(改行を含まない) バッファの終端がくるか改行キー入力まで待機する */
    auto getLine(STRING str, INT32 size) {
      CHAR16 key;
      INT32 offset = 0;
      size--; // \0の分
      while(TRUE) {
        key = getChar();
        str[offset] = key;
        offset++;
        if (offset == size || key == '\r') {
          str[offset] = '\0';
          break;
        }
      }
      return offset;
    }

    auto onKeyEvent() {

    }
  };

  namespace Console {
    auto clear() {
      SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    }

    auto write(STRING str) {
      SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
    }

    auto writeLine(STRING str) {
      write(str);
      write((STRING)L"\r\n");
    }
  };

  namespace Graphics {
    static EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutputProtocol;
    static UINT32 HorizontalResolution;
    static UINT32 VerticalResolution;
    static UINT32 TotalResolution;

    auto initGraphics() {
      SystemTable->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, nullptr, (void**)&GraphicsOutputProtocol);
      HorizontalResolution = GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
      VerticalResolution = GraphicsOutputProtocol->Mode->Info->VerticalResolution;
      TotalResolution = HorizontalResolution * VerticalResolution;
    }

    auto getMaxResolutionMode(BOOLEAN horizontal = TRUE) {
      // cf. http://segfo-ctflog.blogspot.jp/2015/06/uefios.html
      EFI_STATUS status;
      UINTN sizeOfInfo;
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *gopInfo;
      UINT32 mode = 0;
      for (UINT32 i = 0; i < GraphicsOutputProtocol->Mode->MaxMode; ++i) {
        status = GraphicsOutputProtocol->QueryMode(GraphicsOutputProtocol, i, &sizeOfInfo, &gopInfo);
        if (status != EFI_SUCCESS) break;
        if (gopInfo->PixelFormat != PixelBltOnly &&
          ((gopInfo->HorizontalResolution >= gopInfo->VerticalResolution) == horizontal)) {
          mode = i;
        }
      }
      return mode;
    }

    auto setMode(UINT32 mode) {
      GraphicsOutputProtocol->SetMode(GraphicsOutputProtocol, mode);
      HorizontalResolution = GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
      VerticalResolution = GraphicsOutputProtocol->Mode->Info->VerticalResolution;
      TotalResolution = HorizontalResolution * VerticalResolution;
    }

    auto maximizeResolution(BOOLEAN hosizontal = TRUE) {
      setMode(getMaxResolutionMode(hosizontal));
    }

    typedef EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;

    struct _Point {
      INT32 x;
      INT32 y;
    };

    typedef struct _Point Point;

    struct _Rect {
      UINT32 w;
      UINT32 h;
    };

    typedef struct _Rect Rect;

    struct _Circle {
      UINT32 r;
    };

    typedef struct _Circle Circle;

    void drawPoint(INT32 offset, const Pixel &pixel, Pixel *basePixel) {
      if (offset < 0 || offset >= (INT32)TotalResolution) return;
      Pixel *pointPixel = basePixel + offset;
      pointPixel->Blue = pixel.Blue;
      pointPixel->Green = pixel.Green;
      pointPixel->Red = pixel.Red;
      pointPixel->Reserved = pixel.Reserved;
    }

    void drawPoint(INT32 offset, const Pixel &pixel) {
      drawPoint(offset, pixel, (Pixel *)GraphicsOutputProtocol->Mode->FrameBufferBase);
    }

    void drawPoint(INT32 x, INT32 y, const Pixel &pixel) {
      drawPoint(y * HorizontalResolution + x, pixel);
    }

    void drawPoint(const Point &point, const Pixel &pixel) {
      drawPoint(point.x, point.y, pixel);
    }

    void fillRect(INT32 x, INT32 y, UINT32 w, UINT32 h, const Pixel &color) {
      Pixel *basePixel = (Pixel *)GraphicsOutputProtocol->Mode->FrameBufferBase;
      Pixel *pointPixel;

      INT32 px, py;
      INT32 yoffset;
      for (py = y; py < y + (INT32)h; ++py) {
        if (py < 0) continue;
        if (py >= (INT32)VerticalResolution) break;
        yoffset = py * (INT32)HorizontalResolution;
        for (px = x; px < x + (INT32)w; ++px) {
          if (px < 0 || px >= (INT32)HorizontalResolution) continue;
          pointPixel = basePixel + yoffset + px;
          pointPixel->Blue = color.Blue;
          pointPixel->Green = color.Green;
          pointPixel->Red = color.Red;
          pointPixel->Reserved = color.Reserved;
        }
      }
    }

    void fillRect(INT32 x, INT32 y, const Rect &rect, const Pixel &color) {
      fillRect(x, y, rect.w, rect.h, color);
    }

    void fillRect(const Point &point, const Rect &rect, const Pixel &color) {
      fillRect(point.x, point.y, rect, color);
    }

    void fillCircle(INT32 x, INT32 y, UINT32 r, const Pixel &color) {
      Pixel *basePixel = (Pixel *)GraphicsOutputProtocol->Mode->FrameBufferBase;
      Pixel *pointPixel;

      UINT32 r2 = r * r;
      INT32 px, py, dx, dy;
      INT32 yoffset;
      for (py = y - r; py < y + (INT32)r; ++py) {
        if (py < 0) continue;
        if (py >= (INT32)VerticalResolution) break;
        yoffset = py * (INT32)HorizontalResolution;
        for (px = x - r; px < x + (INT32)r; ++px) {
          if (px < 0 || px >= (INT32)HorizontalResolution) continue;
          dx = px - x;
          dy = py - y;
          if ((dx * dx) + (dy * dy) > (INT32)r2) continue;

          pointPixel = basePixel + yoffset + px;
          pointPixel->Blue = color.Blue;
          pointPixel->Green = color.Green;
          pointPixel->Red = color.Red;
          pointPixel->Reserved = color.Reserved;
        }
      }
    }

    void fillCircle(INT32 x, INT32 y, const Circle &circle, const Pixel &color) {
      fillCircle(x, y, circle.r, color);
    }

    void fillCircle(const Point &point, const Circle &circle, const Pixel &color) {
      fillCircle(point.x, point.y, circle, color);
    }
  };

  void initGame(EFI_SYSTEM_TABLE *SystemTable) {
    EfiGame::SystemTable = SystemTable;
    Graphics::initGraphics();
  }
};

using namespace EfiGame;

extern "C" void efi_main(void *ImageHandle __attribute__ ((unused)), EFI_SYSTEM_TABLE *SystemTable) {
  initGame(SystemTable);
  // Graphics::maximizeResolution();

  Console::clear();
  Console::write((EfiGame::STRING)L"Hello!\r\nUEFI!\r\n");
  Graphics::Pixel color {0, 127, 255, 0};
  Graphics::fillRect(0, 0, 100, 100, color);
  Graphics::fillRect(100, 100, 100, 100, color);
  Graphics::fillCircle(300, 300, 100, color);
  CHAR16 str[5];
  while (TRUE) {
    Input::getLine(str, 5);
    Console::writeLine(str);
  }
}
