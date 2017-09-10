#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <cmath>

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

namespace EfiGame {
  typedef CHAR16* STRING;

  static EFI_SYSTEM_TABLE *SystemTable;

  template <class T> void memset(T *buf, T val, UINTN size) {
    T *tmp = buf;
    while (size--) *tmp++ = val;
  }

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

    void writeNum(INT64 val) {
      CHAR16 digit;
      CHAR16 reverse_str[30];
      CHAR16 str[30];
      bool negative = val < 0;
      if (negative) val = -val;
      INT32 i = 0;
      do {
        digit = val % 10;
        reverse_str[i] = L'0' + digit;
        val /= 10;
        ++i;
      } while (val);
      INT32 length = i;
      if (negative) str[0] = L'-';
      for (i = 0; i < length; ++i) {
        str[i + negative] = reverse_str[length - i - 1];
      }
      str[length + negative] = L'\0';
      write(str);
    }

    void writeNumLine(INT64 val) {
      writeNum(val);
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
      UINT32 py;
      if (w > HorizontalResolution) w = HorizontalResolution;
      Pixel pixels[w];
      memset(pixels, color, w);
      for (py = y; py < y + h; ++py) {
        GraphicsOutputProtocol->Blt(GraphicsOutputProtocol, pixels, EfiBltBufferToVideo, 0, 0, x, py, w, 1, 0);
      }
    }

    void fillRect(INT32 x, INT32 y, const Rect &rect, const Pixel &color) {
      fillRect(x, y, rect.w, rect.h, color);
    }

    void fillRect(const Point &point, const Rect &rect, const Pixel &color) {
      fillRect(point.x, point.y, rect, color);
    }

    void fillCircle(INT32 x, INT32 y, UINT32 r, const Pixel &color) {
      Pixel pixels[HorizontalResolution];
      memset(pixels, color, HorizontalResolution);

      UINT32 r2 = r * r;
      INT32 dx, dy, mdx, mdy, rest, start_x, end_x, length;
      dy = -r;
      if (y + dy < 0) dy = -y;
      mdy = r;
      if (y + mdy > (INT32)VerticalResolution) mdy = VerticalResolution - y;
      for (; dy < mdy; ++dy) {
        rest = r2 - dy * dy;
        dx = -r;
        while (dx * dx > rest) ++dx;
        start_x = x + dx;
        end_x = x - dx;
        if (!dx || end_x <= 0 || start_x >= (INT32)HorizontalResolution) continue;
        if (start_x < 0) start_x = 0;
        if (end_x > (INT32)HorizontalResolution) end_x = HorizontalResolution;
        length = end_x - start_x;
        GraphicsOutputProtocol->Blt(GraphicsOutputProtocol, pixels, EfiBltBufferToVideo, 0, 0, start_x, y + dy, length, 1, 0);
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
  Graphics::Pixel white {255, 255, 255, 0};
  Graphics::fillRect(0, 0, Graphics::HorizontalResolution, Graphics::VerticalResolution, white);
  Graphics::Pixel color {0, 127, 255, 0};
  Graphics::Pixel color2 {255, 127, 255, 0};
  Graphics::Pixel color3 {127, 127, 0, 0};
  Graphics::fillRect(0, 0, 100, 100, color);
  Graphics::fillRect(100, 100, 100, 100, color);
  UINT32 offset = 0;
  UINT32 a;
  while(TRUE) {
    a = 0;
    //while(a < 1000000) a++;
    Graphics::fillRect(0, offset - 10, Graphics::HorizontalResolution, 10, color2);
    Graphics::fillRect(0, offset, Graphics::HorizontalResolution, 100, color);
    Graphics::fillCircle(100, offset + 150, 50, color3);
    offset++;
    if (offset >= Graphics::VerticalResolution) offset = 0;
  }
  /*CHAR16 str[5];
  while (TRUE) {
    Input::getLine(str, 5);
    Console::writeLine(str);
  }*/
}
