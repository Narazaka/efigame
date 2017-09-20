#include <Uefi.h>
#include <Protocol/SimplePointer.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Guid/FileInfo.h>

#include <libc_base.h>
#include <stdlib.h>

#ifdef EFIGAME_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION

#endif

#define STBI_ASSERT(x)
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_ONLY_PNG
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_FREE(p) free(p)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) realloc_sized(p,oldsz,newsz)
#include <stb_image.h>

#ifdef EFIGAME_IMPLEMENTATION

#define STDLIB_H_IMPLEMENTATION

#include <stdlib.h>

#define STRING_H_IMPLEMENTATION

#include <string.h>

#endif

#if !defined(EFIGAME_H) || defined(EFIGAME_IMPLEMENTATION)
#ifndef EFIGAME_H

#define EFIGAME_H

#endif

#ifdef EFIGAME_IMPLEMENTATION
#define IMPL(x) x
#define DEF(x)

#define EXTERN

#else
#define IMPL(x) ;
#define DEF(x) x
#define EXTERN extern
#endif

EXTERN EFI_GUID gEfiSimplePointerProtocolGuid IMPL(= EFI_SIMPLE_POINTER_PROTOCOL_GUID;)
EXTERN EFI_GUID gEfiSimpleFileSystemProtocolGuid IMPL(= EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;)
EXTERN EFI_GUID gEfiGraphicsOutputProtocolGuid IMPL(= EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;)

template <class T> void *memcpy(T *dest, const T *src, UINTN n) IMPL({
  T *d = dest;
  T const *s = src;
  while (n--) *d++ = *s++;
  return dest;
})

template <class T> void* memset(T *buf, T val, UINTN size) IMPL({
  T *tmp = buf;
  while (size--) *tmp++ = val;
  return buf;
})

namespace EfiGame {
  EXTERN EFI_SYSTEM_TABLE *SystemTable;

  namespace Console {
    void clear() IMPL({
      SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    })

    void write(EFI_STRING str) IMPL({
      SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
    })

    void writeLine(EFI_STRING str) IMPL({
      write(str);
      write((EFI_STRING)L"\r\n");
    })

    void writeNum(INT64 val) IMPL({
      CHAR16 str[30];
      itoa(val, str, 10);
      write(str);
    })

    void writeNumLine(INT64 val) IMPL({
      writeNum(val);
      write((EFI_STRING)L"\r\n");
    })
  };

  namespace Input {
    EXTERN EFI_SIMPLE_POINTER_PROTOCOL *SimplePointerProtocol;

    void initInput() IMPL({
      SystemTable->BootServices->LocateProtocol(&gEfiSimplePointerProtocolGuid, nullptr, (void**)&SimplePointerProtocol);
    })

    // keyboard

    EXTERN bool triggerKeyEvent IMPL(= true;)
    EXTERN void (*onKeyPress)(CHAR16 key);

    /** キー入力1つを読み込む キー入力まで待機する */
    CHAR16 getChar() IMPL({
      EFI_INPUT_KEY key;
      UINT64 waitIdx;
      SystemTable->BootServices->WaitForEvent(1, &(SystemTable->ConIn->WaitForKey), &waitIdx);
      SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
      if (triggerKeyEvent && onKeyPress != nullptr) onKeyPress(key.UnicodeChar);
      return key.UnicodeChar;
    })

    /** 改行が来るまでキー入力を読み込む(改行を含まない) バッファの終端がくるか改行キー入力まで待機する */
    INT32 getLine(EFI_STRING str, INT32 size) IMPL({
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
    })

    /**
     * 現在たまっているキー入力を取得する
     *
     * たまっていなければ-1を返しなにも読み取らない
     */
    CHAR16 readKeyStroke() IMPL({
      EFI_INPUT_KEY key;
      if (EFI_SUCCESS == SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key)) {
        if (triggerKeyEvent && onKeyPress != nullptr) onKeyPress(key.UnicodeChar);
        return key.UnicodeChar;
      } else {
        return (CHAR16)-1;
      }
    })

    // mouse

    DEF(typedef struct {
      /** マウスポインタ移動分をスクリーン座標として追従するか */
      bool tracking = false;
      /** マウスのスクリーンX座標 -1なら無効 */
      INT32 x = -1;
      /** マウスのスクリーンY座標 -1なら無効 */
      INT32 y = -1;
    } MouseScreenCoordinateState;)

    EXTERN MouseScreenCoordinateState mouse;

    DEF(typedef struct {
      /** マウスの左ボタン押下状態 */
      bool leftPress = false;
      /** マウスの右ボタン押下状態 */
      bool rightPress = false;
    } MouseButtonState;)

    EXTERN MouseButtonState mouseButton;

    /** マウスポインタ移動分をスクリーン座標として追従するか設定する */
    void setTrackMouseScreenCoordinate(bool track, INT32 initial_x DEF(= -1), INT32 initial_y DEF(= -1)) IMPL({
      mouse.tracking = track;
      if (mouse.tracking) {
        mouse.x = initial_x >= 0 ? initial_x : 0;
        mouse.y = initial_y >= 0 ? initial_y : 0;
      } else {
        mouse.x = -1;
        mouse.y = -1;
      }
    })

    EXTERN bool triggerMouseEvent IMPL(= true;)
    EXTERN void (*onMouseEvent)(EFI_SIMPLE_POINTER_STATE state);
    EXTERN void (*onMouseMove)(INT32 RelativeMovementX, INT32 RelativeMovementY);
    EXTERN void (*onMouseWheelMove)(INT32 RelativeMovementZ);
    EXTERN void (*onMouseLeftDown)();
    EXTERN void (*onMouseLeftUp)();
    EXTERN void (*onMouseLeftClick)();
    EXTERN void (*onMouseRightDown)();
    EXTERN void (*onMouseRightUp)();
    EXTERN void (*onMouseRightClick)();

    EFI_SIMPLE_POINTER_STATE getPointerState() IMPL({
      EFI_SIMPLE_POINTER_STATE state;
      if (EFI_SUCCESS != SimplePointerProtocol->GetState(SimplePointerProtocol, &state)) return state; // 応急処置
      if (mouse.tracking) {
        mouse.x += state.RelativeMovementX;
        mouse.y += state.RelativeMovementY;
      }
      bool leftPressChange;
      bool rightPressChange;
      if (triggerMouseEvent) {
        if (onMouseEvent) onMouseEvent(state);
        if ((state.RelativeMovementX || state.RelativeMovementY) && onMouseMove) onMouseMove(state.RelativeMovementX, state.RelativeMovementY);
        if (state.RelativeMovementY && onMouseWheelMove) onMouseWheelMove(state.RelativeMovementY);
        leftPressChange = mouseButton.leftPress != state.LeftButton;
        rightPressChange = mouseButton.rightPress != state.RightButton;
      }
      mouseButton.leftPress = state.LeftButton;
      mouseButton.rightPress = state.RightButton;
      if (triggerMouseEvent) {
        if (leftPressChange) {
          if (state.LeftButton) {
            if (onMouseLeftDown) onMouseLeftDown();
          } else {
            if (onMouseLeftUp) onMouseLeftUp();
            if (onMouseLeftClick) onMouseLeftClick();
          }
        }
        if (rightPressChange) {
          if (state.RightButton) {
            if (onMouseRightDown) onMouseRightDown();
          } else {
            if (onMouseRightUp) onMouseRightUp();
            if (onMouseRightClick) onMouseRightClick();
          }
        }
      }
      return state;
    })
  };

  namespace FileSystem {
    EXTERN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystemProtocol;
    EXTERN EFI_FILE_PROTOCOL *Root;

    #define MAX_FILE_BUF_SIZE 1024 * 1024 * 10

    void initFileSystem() IMPL({
      SystemTable->BootServices->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, nullptr, (void**)&SimpleFileSystemProtocol);
      SimpleFileSystemProtocol->OpenVolume(SimpleFileSystemProtocol, &Root);
    })

    EFI_FILE_PROTOCOL* open(CHAR16* filename, UINT64 mode DEF(= EFI_FILE_MODE_READ)) IMPL({
      EFI_FILE_PROTOCOL *file;
      return EFI_SUCCESS == Root->Open(Root, &file, filename, mode, 0) ? file : nullptr;
    })

    UINTN read(EFI_FILE_PROTOCOL *file, void* buf, UINTN size) IMPL({
      file->Read(file, &size, buf);
      return size;
    })

    void close(EFI_FILE_PROTOCOL *file) IMPL({
      file->Close(file);
    })

    #define EFI_FILE_INFO_MAX 1024

    EFI_FILE_INFO* readdir(EFI_FILE_PROTOCOL *file) IMPL({
      EFI_FILE_INFO *child = (EFI_FILE_INFO*)malloc(EFI_FILE_INFO_MAX);
      auto size = read(file, child, EFI_FILE_INFO_MAX);
      return size ? child : nullptr;
    })
  };

  namespace Graphics {
    EXTERN EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutputProtocol;
    EXTERN UINT32 HorizontalResolution;
    EXTERN UINT32 VerticalResolution;
    EXTERN UINT32 TotalResolution;

    void initGraphics() IMPL({
      SystemTable->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, nullptr, (void**)&GraphicsOutputProtocol);
      HorizontalResolution = GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
      VerticalResolution = GraphicsOutputProtocol->Mode->Info->VerticalResolution;
      TotalResolution = HorizontalResolution * VerticalResolution;
    })

    UINT32 getMaxResolutionMode(BOOLEAN horizontal DEF(= TRUE)) IMPL({
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
    })

    void setMode(UINT32 mode) IMPL({
      GraphicsOutputProtocol->SetMode(GraphicsOutputProtocol, mode);
      HorizontalResolution = GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
      VerticalResolution = GraphicsOutputProtocol->Mode->Info->VerticalResolution;
      TotalResolution = HorizontalResolution * VerticalResolution;
    })

    void maximizeResolution(BOOLEAN hosizontal DEF(= TRUE)) IMPL({
      setMode(getMaxResolutionMode(hosizontal));
    })

    typedef EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;

    DEF(typedef struct {
      INT32 x;
      INT32 y;
    } Point;)

    DEF(typedef struct {
      UINT32 w;
      UINT32 h;
    } Rect;)

    DEF(typedef struct {
      UINT32 r;
    } Circle;)

    void drawPoint(INT32 offset, const Pixel &pixel, Pixel *basePixel) IMPL({
      if (offset < 0 || offset >= (INT32)TotalResolution) return;
      Pixel *pointPixel = basePixel + offset;
      pointPixel->Blue = pixel.Blue;
      pointPixel->Green = pixel.Green;
      pointPixel->Red = pixel.Red;
      pointPixel->Reserved = pixel.Reserved;
    })

    void drawPoint(INT32 offset, const Pixel &pixel) IMPL({
      drawPoint(offset, pixel, (Pixel *)GraphicsOutputProtocol->Mode->FrameBufferBase);
    })

    void drawPoint(INT32 x, INT32 y, const Pixel &pixel) IMPL({
      drawPoint(y * HorizontalResolution + x, pixel);
    })

    void drawPoint(const Point &point, const Pixel &pixel) IMPL({
      drawPoint(point.x, point.y, pixel);
    })

    void fillRect(INT32 x, INT32 y, UINT32 w, UINT32 h, const Pixel &color) IMPL({
      UINT32 py;
      if (w > HorizontalResolution) w = HorizontalResolution;
      Pixel pixels[w];
      memset(pixels, color, w);
      for (py = y; py < y + h; ++py) {
        GraphicsOutputProtocol->Blt(GraphicsOutputProtocol, pixels, EfiBltBufferToVideo, 0, 0, x, py, w, 1, 0);
      }
    })

    void fillRect(INT32 x, INT32 y, const Rect &rect, const Pixel &color) IMPL({
      fillRect(x, y, rect.w, rect.h, color);
    })

    void fillRect(const Point &point, const Rect &rect, const Pixel &color) IMPL({
      fillRect(point.x, point.y, rect, color);
    })

    void fillCircle(INT32 x, INT32 y, UINT32 r, const Pixel &color) IMPL({
      Pixel pixels[HorizontalResolution];
      memset(pixels, color, HorizontalResolution);

      UINT32 r2 = r * r;
      INT32 dx; INT32 dy; INT32 mdy; INT32 rest; INT32 start_x; INT32 end_x; INT32 length;
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
    })

    void fillCircle(INT32 x, INT32 y, const Circle &circle, const Pixel &color) IMPL({
      fillCircle(x, y, circle.r, color);
    })

    void fillCircle(const Point &point, const Circle &circle, const Pixel &color) IMPL({
      fillCircle(point.x, point.y, circle, color);
    })

    DEF(typedef struct {
      Pixel *pixels;
      UINT8 *alphas;
      int x;
      int y;
      int composition;
      int length;
    } Image;)

    Image* getRectImage(UINT32 w, UINT32 h, const Pixel &color) IMPL({
      auto length = w * h;
      Image *image = (Image*)malloc(sizeof(Image));
      image->pixels = (Pixel*)malloc(sizeof(Pixel) * length);
      memset(image->pixels, color, length);
      image->alphas = (UINT8*)malloc(sizeof(UINT8) * length);
      memset(image->alphas, 255, length);
      image->x = w;
      image->y = h;
      image->length = length;
      image->composition = 4;
      return image;
    })

    Image* getCircleImage(UINT32 r, const Pixel &color) IMPL({
      auto R = r * 2;
      auto length = R * R;
      Image *image = (Image*)malloc(sizeof(Image));
      image->pixels = (Pixel*)malloc(sizeof(Pixel) * length);
      memset(image->pixels, color, length);
      image->alphas = (UINT8*)malloc(sizeof(UINT8) * length);
      memset(image->alphas, 0, length);
      UINT32 r2 = r * r;
      INT32 dx; INT32 dy; INT32 mdy; INT32 rest; INT32 yoffset;
      mdy = r;
      for (dy = -r; dy < mdy; ++dy) {
        rest = r2 - dy * dy;
        dx = -r;
        while (dx * dx > rest) ++dx;
        yoffset = (r + dy) * R;
        if (!dx) continue;
        memset(image->alphas + yoffset + r + dx, 255, -2 * dx);
      }
      image->x = R;
      image->y = R;
      image->length = length;
      image->composition = 4;
      return image;
    })

    Image* loadImageFromMemory(const UINT8 *buf, int len) IMPL({
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
    })

    #define MAX_IMAGE_FILE_SIZE 1024 * 1024 * 20

    Image* loadImageFromFile(CHAR16 *filename, UINTN maxFileSize DEF(= MAX_IMAGE_FILE_SIZE)) IMPL({
      auto file = FileSystem::open(filename);
      if (file == nullptr) return nullptr;
      UINT8 *buf = (UINT8*)malloc(sizeof(UINT8) * maxFileSize);
      auto size = FileSystem::read(file, buf, maxFileSize);
      FileSystem::close(file);
      auto image = loadImageFromMemory(buf, size);
      free(buf);
      return image;
    })

    bool drawImage(Image *image, INT32 x, INT32 y, bool transparent DEF(= TRUE)) IMPL({
      if (image == nullptr) return false;
      Pixel* base = (Pixel *)GraphicsOutputProtocol->Mode->FrameBufferBase;
      Pixel* pixels; Pixel* image_pixel; Pixel* base_pixel;
      if (transparent) {
        pixels = (Pixel*)malloc(sizeof(Pixel) * image->length);
        memcpy(pixels, image->pixels, image->length);
        INT32 dx; INT32 dy; INT32 image_offset; INT32 image_yoffset; INT32 base_offset; INT32 base_yoffset; INT32 alpha;
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
    })
/*
    void initFont() {
      auto fonts = FileSystem::open((EFI_STRING)L"fonts");
      CHAR16 path[50];
      while(TRUE) {
        auto info = FileSystem::readdir(fonts);
        if (info == nullptr) break;
        strcpy(path, (EFI_STRING)L"fonts\\");
        strcat(path, info->FileName);
        Console::writeLine(path);
      }
    }
*/

    #define FONT_FILE_MAX_SIZE 1024
    #define FONT_MAX_PIXEL_SIZE 1024
    EFI_STRING getFontImageName(CHAR16 c, CHAR16 *path) IMPL({
      CHAR16 code[10];
      itoa(c, code, 10);
      strcpy(path, (EFI_STRING)L"fonts\\");
      strcat(path, code);
      strcat(path, (EFI_STRING)L".png");
      return path;
    })

    Image* getFontImage(CHAR16 c) IMPL({
      CHAR16 path[50];
      getFontImageName(c, path);
      return Graphics::loadImageFromFile(path, FONT_FILE_MAX_SIZE);
    })

    bool drawChar(CHAR16 c, INT32 x, INT32 y, bool transparent DEF(= TRUE)) IMPL({
      auto image = getFontImage(c);
      return drawImage(image, x, y, transparent);
    })

    DEF(typedef struct {
      INT32 dx;
      INT32 dy;
      INT32 w;
      INT32 h;
      INT32 lines;
    } DrawStrInfo;)

    DrawStrInfo* drawStr(CHAR16 *str, Pixel color, INT32 x, INT32 y, INT32 width DEF(= 0), bool transparent DEF(= TRUE), DrawStrInfo *info DEF(= nullptr)) IMPL({
      UINTN length = strlen(str);
      INT32 dx = 0; INT32 dy = 0; INT32 w = 0; INT32 h = 0; INT32 lines = 1;
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
    })
  };

  namespace Main {
    EXTERN void (*onUpdate)();

    void _onKeyPress() IMPL({
      Input::readKeyStroke();
    })

    void _onTick() IMPL({
      Input::getPointerState();
      if (onUpdate) onUpdate();
    })

    void start(UINT64 tick_interval DEF(= 333'300)) IMPL({
      EFI_EVENT events[2];
      EFI_EVENT timerEvent;
      SystemTable->BootServices->CreateEvent(EVT_TIMER, 0, NULL, NULL, &timerEvent);
      SystemTable->BootServices->SetTimer(timerEvent, TimerPeriodic, tick_interval);
      events[0] = SystemTable->ConIn->WaitForKey;
      events[1] = timerEvent;
      UINTN eventIndex;
      while(TRUE) {
        SystemTable->BootServices->WaitForEvent(2, events, &eventIndex);
        switch (eventIndex) {
          case 0: _onKeyPress(); break;
          case 1: _onTick(); break;
        }
      }
    })
  };

  void initGame(EFI_SYSTEM_TABLE *SystemTable) IMPL({
    libc::init(SystemTable);
    EfiGame::SystemTable = SystemTable;
    Input::initInput();
    Graphics::initGraphics();
    FileSystem::initFileSystem();
  })
};

#endif
