#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Guid/FileInfo.h>
#include <string.h>

#include <libc_base.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_ONLY_PNG
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_FREE(p) free(p)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) realloc_sized(p,oldsz,newsz)
#include <stb_image.h>

EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

template <class T> void *memcpy(T *dest, const T *src, UINTN n)
{
  T *d = dest;
  T const *s = src;
  while (n--) *d++ = *s++;
  return dest;
}

template <class T> void* memset(T *buf, T val, UINTN size) {
  T *tmp = buf;
  while (size--) *tmp++ = val;
  return buf;
}

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

    void writeNum(INT64 val) {
      CHAR16 str[30];
      itoa(val, str, 10);
      write(str);
    }

    void writeNumLine(INT64 val) {
      writeNum(val);
      write((STRING)L"\r\n");
    }
  };

  namespace FileSystem {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystemProtocol;
    EFI_FILE_PROTOCOL *Root;

    #define MAX_FILE_BUF_SIZE 1024 * 1024 * 10

    auto initFileSystem() {
      SystemTable->BootServices->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, nullptr, (void**)&SimpleFileSystemProtocol);
      SimpleFileSystemProtocol->OpenVolume(SimpleFileSystemProtocol, &Root);
    }

    auto open(CHAR16* filename, UINT64 mode = EFI_FILE_MODE_READ) {
      EFI_FILE_PROTOCOL *file;
      return EFI_SUCCESS == Root->Open(Root, &file, filename, mode, 0) ? file : nullptr;
    }

    auto read(EFI_FILE_PROTOCOL *file, void* buf, UINTN size) {
      file->Read(file, &size, buf);
      return size;
    }

    auto close(EFI_FILE_PROTOCOL *file) {
      file->Close(file);
    }

    #define EFI_FILE_INFO_MAX 1024

    auto readdir(EFI_FILE_PROTOCOL *file) {
      EFI_FILE_INFO *child = (EFI_FILE_INFO*)malloc(EFI_FILE_INFO_MAX);
      auto size = read(file, child, EFI_FILE_INFO_MAX);
      return size ? child : nullptr;
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
      INT32 dx, dy, mdy, rest, start_x, end_x, length;
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

    struct _Image {
      Pixel *pixels;
      UINT8 *alphas;
      int x;
      int y;
      int composition;
      int length;
    };

    typedef struct _Image Image;

    auto loadImageFromMemory(const UINT8 *buf, int len) {
      Image *image = (Image*)malloc(sizeof(Image));

      UINT8* src_pixels = stbi_load_from_memory(buf, len, &image->x, &image->y, &image->composition, 4);
      int length = image->length = image->x * image->y;
      image->pixels = (Pixel*)malloc(sizeof(Pixel) * length);
      image->alphas = (UINT8*)malloc(sizeof(UINT8) * length);
      Pixel *pixel = image->pixels;
      UINT8 *alpha = image->alphas;
      int offset;
      for (int pos = 0; pos < length; ++pos) {
        pixel->Red = *(src_pixels + offset);
        pixel->Green = *(src_pixels + offset + 1);
        pixel->Blue = *(src_pixels + offset + 2);
        pixel->Reserved = 0;
        *alpha = *(src_pixels + offset + 3);
        offset += 4;
        ++pixel;
        ++alpha;
      }
      stbi_image_free(src_pixels);
      return image;
    }

    #define MAX_IMAGE_FILE_SIZE 1024 * 1024 * 20

    Image* loadImageFromFile(CHAR16 *filename, UINTN maxFileSize = MAX_IMAGE_FILE_SIZE) {
      auto file = FileSystem::open(filename);
      if (file == nullptr) return nullptr;
      UINT8 *buf = (UINT8*)malloc(sizeof(UINT8) * maxFileSize);
      auto size = FileSystem::read(file, buf, maxFileSize);
      auto image = loadImageFromMemory(buf, size);
      free(buf);
      return image;
    }

    auto drawImage(Image *image, INT32 x, INT32 y, bool transparent = TRUE) {
      if (image == nullptr) return false;
      Pixel* base = (Pixel *)GraphicsOutputProtocol->Mode->FrameBufferBase;
      Pixel* pixels, *image_pixel, *base_pixel;
      if (transparent) {
        pixels = (Pixel*)malloc(sizeof(Pixel) * image->length);
        memcpy(pixels, image->pixels, image->length);
        INT32 dx, dy, image_offset, image_yoffset, base_offset, base_yoffset, alpha;
        double alpha1;
        for (dy = 0; dy < image->y; ++dy) {
          image_yoffset = dy * image->x;
          base_yoffset = (y + dy) * HorizontalResolution;
          for (dx = 0; dx < image->x; ++dx) {
            image_offset = image_yoffset + dx;
            alpha = image->alphas[image_offset];
            if (alpha != 255) {
              base_offset = base_yoffset + x + dx;
              if (alpha == 0) {
                pixels[image_offset] = base[base_offset];
              } else {
                alpha1 = alpha / 255.0;
                image_pixel = &pixels[image_offset];
                base_pixel = &base[base_offset];
                image_pixel->Red = (image_pixel->Red * alpha1) + base_pixel->Red * (1.0 - alpha1);
                image_pixel->Green = (image_pixel->Green * alpha1) + base_pixel->Green * (1.0 - alpha1);
                image_pixel->Blue = (image_pixel->Blue * alpha1) + base_pixel->Blue * (1.0 - alpha1);
              }
            }
          }
        }
      } else {
        pixels = image->pixels;
      }
      GraphicsOutputProtocol->Blt(GraphicsOutputProtocol, pixels, EfiBltBufferToVideo, 0, 0, x, y, image->x, image->y, 0);
      if (transparent) free(pixels);
      return true;
    }
/*
    void initFont() {
      auto fonts = FileSystem::open((STRING)L"fonts");
      CHAR16 path[50];
      while(TRUE) {
        auto info = FileSystem::readdir(fonts);
        if (info == nullptr) break;
        strcpy(path, (STRING)L"fonts\\");
        strcat(path, info->FileName);
        Console::writeLine(path);
      }
    }
*/

    #define FONT_FILE_MAX_SIZE 1024
    #define FONT_MAX_PIXEL_SIZE 1024
    auto getFontImageName(CHAR16 c, CHAR16 *path) {
      CHAR16 code[10];
      itoa(c, code, 10);
      strcpy(path, (STRING)L"fonts\\");
      strcat(path, code);
      strcat(path, (STRING)L".png");
      return path;
    }

    auto getFontImage(CHAR16 c) {
      CHAR16 path[50];
      getFontImageName(c, path);
      return Graphics::loadImageFromFile(path, FONT_FILE_MAX_SIZE);
    }

    auto drawChar(CHAR16 c, INT32 x, INT32 y, bool transparent = TRUE) {
      auto image = getFontImage(c);
      return drawImage(image, x, y, transparent);
    }

    struct _DrawStrInfo {
      INT32 dx;
      INT32 dy;
      INT32 w;
      INT32 h;
      INT32 lines;
    };

    typedef struct _DrawStrInfo DrawStrInfo;

    auto drawStr(CHAR16 *str, Pixel color, INT32 x, INT32 y, INT32 width = 0, bool transparent = TRUE, DrawStrInfo *info = nullptr) {
      UINTN length = strlen(str);
      INT32 dx = 0, dy = 0, w = 0, h = 0, lines = 1;
      INT32 line_height = 0;
      Pixel pixels[FONT_MAX_PIXEL_SIZE];
      memset(pixels, color, FONT_MAX_PIXEL_SIZE);
      for (INT32 i = 0; i < length; ++i) {
        if (str[i] == L'\r') {
          dx = 0;
          continue;
        } else if (str[i] == L'\n') {
          ++lines;
          dy += line_height;
          line_height = 0;
        }
        auto image = getFontImage(str[i]);
        if (image == nullptr) continue;
        if (width && width < dx + image->x) {
          ++lines;
          dx = 0;
          dy += line_height;
          line_height = 0;
        }
        image->pixels = pixels; // color
        drawImage(image, x + dx, y + dy, transparent);
        dx += image->x;
        if (line_height < image->y) line_height = image->y;
        if (w < dx) w = dx;
      }
      h = dy + line_height;
      if (info != nullptr) {
        info->dx = dx;
        info->dy = dy;
        info->w = w;
        info->h = h;
        info->lines = lines;
      }
      return info;
    }
  };

  void initGame(EFI_SYSTEM_TABLE *SystemTable) {
    libc::init(SystemTable);
    EfiGame::SystemTable = SystemTable;
    Graphics::initGraphics();
    FileSystem::initFileSystem();
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
  Graphics::Pixel black {0, 0, 0, 0};
  Graphics::Pixel color {0, 127, 255, 0};
  // Graphics::Pixel color2 {255, 127, 255, 0};
  // Graphics::Pixel color3 {127, 127, 0, 0};
  Graphics::fillRect(0, 0, 100, 100, color);
  Graphics::fillRect(100, 100, 100, 100, color);
/*  UINT32 offset = 0;

  while(TRUE) {
    //while(a < 1000000) a++;
    Graphics::fillRect(0, offset - 10, Graphics::HorizontalResolution, 10, color2);
    Graphics::fillRect(0, offset, Graphics::HorizontalResolution, 100, color);
    Graphics::fillCircle(100, offset + 150, 50, color3);
    offset++;
    if (offset >= Graphics::VerticalResolution) offset = 0;
  }*/

  while (TRUE) {
    auto info = FileSystem::readdir(FileSystem::Root);
    if (info == nullptr) break;
    Console::write((EfiGame::STRING)L"- ");
    Console::writeLine((EfiGame::STRING)info->FileName);
  }
  Console::write((EfiGame::STRING)L"loading... ");
  auto *image = Graphics::loadImageFromFile((EfiGame::STRING)L"surface0.png");
  Console::writeLine((EfiGame::STRING)L"done");
  Console::writeNumLine(image->x);
  Console::writeNumLine(image->y);
  Console::writeNumLine(image->length);
  Console::writeNumLine(image->composition);
  Console::writeLine((EfiGame::STRING)L"---");
  Graphics::drawImage(image, 10, 10);
  Graphics::drawImage(image, 100, 100);
  Graphics::drawImage(image, 200, 200, false);
  Graphics::drawChar(L'あ', 300, 300);
  Graphics::drawStr((STRING)L"優子「みんながなかよくなりますようにー！！」\r\nEND", black, 300, 400, 200);
  while (TRUE);
  /*CHAR16 str[5];
  while (TRUE) {
    Input::getLine(str, 5);
    Console::writeLine(str);
  }*/
}
