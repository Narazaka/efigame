#include <Uefi.h>
#include <Protocol/SimplePointer.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Guid/FileInfo.h>
#include <string.h>

#include <libc_base.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#define STBI_NO_STDIO
// #define STBI_NO_LINEAR
// #define STBI_NO_HDR
#define STBI_ONLY_PNG
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_FREE(p) free(p)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) realloc_sized(p,oldsz,newsz)
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_ASSERT(x)
#include <stb_image_resize.h>

EFI_GUID gEfiSimplePointerProtocolGuid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
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
  static EFI_SYSTEM_TABLE *SystemTable;

  namespace Console {
    auto clear() {
      SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    }

    auto write(EFI_STRING str) {
      SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
    }

    auto writeLine(EFI_STRING str) {
      write(str);
      write((EFI_STRING)L"\r\n");
    }

    void writeNum(INT64 val) {
      CHAR16 str[30];
      itoa(val, str, 10);
      write(str);
    }

    void writeNumLine(INT64 val) {
      writeNum(val);
      write((EFI_STRING)L"\r\n");
    }
  };

  namespace Input {
    EFI_SIMPLE_POINTER_PROTOCOL *SimplePointerProtocol;

    auto initInput() {
      SystemTable->BootServices->LocateProtocol(&gEfiSimplePointerProtocolGuid, nullptr, (void**)&SimplePointerProtocol);
    }

    // keyboard

    static bool triggerKeyEvent = true;
    static void (*onKeyPress)(CHAR16 key);

    /** キー入力1つを読み込む キー入力まで待機する */
    auto getChar() {
      EFI_INPUT_KEY key;
      UINT64 waitIdx;
      SystemTable->BootServices->WaitForEvent(1, &(SystemTable->ConIn->WaitForKey), &waitIdx);
      SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
      if (triggerKeyEvent && onKeyPress != nullptr) onKeyPress(key.UnicodeChar);
      return key.UnicodeChar;
    }

    /** 改行が来るまでキー入力を読み込む(改行を含まない) バッファの終端がくるか改行キー入力まで待機する */
    auto getLine(EFI_STRING str, INT32 size) {
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

    /**
     * 現在たまっているキー入力を取得する
     *
     * たまっていなければ-1を返しなにも読み取らない
     */
    auto readKeyStroke() {
      EFI_INPUT_KEY key;
      if (EFI_SUCCESS == SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key)) {
        if (triggerKeyEvent && onKeyPress != nullptr) onKeyPress(key.UnicodeChar);
        return key.UnicodeChar;
      } else {
        return (CHAR16)-1;
      }
    }

    // mouse

    struct _MouseScreenCoordinateState {
      /** マウスポインタ移動分をスクリーン座標として追従するか */
      bool tracking = false;
      /** マウスのスクリーンX座標 -1なら無効 */
      INT32 x = -1;
      /** マウスのスクリーンY座標 -1なら無効 */
      INT32 y = -1;
    };

    typedef struct _MouseScreenCoordinateState MouseScreenCoordinateState;

    static MouseScreenCoordinateState mouse;

    struct _MouseButtonState {
      /** マウスの左ボタン押下状態 */
      bool leftPress = false;
      /** マウスの右ボタン押下状態 */
      bool rightPress = false;
    };

    typedef struct _MouseButtonState MouseButtonState;

    static MouseButtonState mouseButton;

    /** マウスポインタ移動分をスクリーン座標として追従するか設定する */
    auto setTrackMouseScreenCoordinate(bool track, INT32 initial_x = -1, INT32 initial_y = -1) {
      mouse.tracking = track;
      if (mouse.tracking) {
        mouse.x = initial_x >= 0 ? initial_x : 0;
        mouse.y = initial_y >= 0 ? initial_y : 0;
      } else {
        mouse.x = -1;
        mouse.y = -1;
      }
    }

    static bool triggerMouseEvent = true;
    static void (*onMouseEvent)(EFI_SIMPLE_POINTER_STATE state);
    static void (*onMouseMove)(INT32 RelativeMovementX, INT32 RelativeMovementY);
    static void (*onMouseWheelMove)(INT32 RelativeMovementZ);
    static void (*onMouseLeftDown)();
    static void (*onMouseLeftUp)();
    static void (*onMouseLeftClick)();
    static void (*onMouseRightDown)();
    static void (*onMouseRightUp)();
    static void (*onMouseRightClick)();

    EFI_SIMPLE_POINTER_STATE getPointerState() {
      EFI_SIMPLE_POINTER_STATE state;
      if (EFI_SUCCESS != SimplePointerProtocol->GetState(SimplePointerProtocol, &state)) return state; // 応急処置
      if (mouse.tracking) {
        mouse.x += state.RelativeMovementX;
        mouse.y += state.RelativeMovementY;
      }
      bool leftPressChange, rightPressChange;
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

    auto getRectImage(UINT32 w, UINT32 h, const Pixel &color) {
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
    }

    auto getCircleImage(UINT32 r, const Pixel &color) {
      auto R = r * 2;
      auto length = R * R;
      Image *image = (Image*)malloc(sizeof(Image));
      image->pixels = (Pixel*)malloc(sizeof(Pixel) * length);
      memset(image->pixels, color, length);
      image->alphas = (UINT8*)malloc(sizeof(UINT8) * length);
      memset(image->alphas, 0, length);
      UINT32 r2 = r * r;
      INT32 dx, dy, mdy, rest, yoffset;
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
    }

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
      FileSystem::close(file);
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
    auto getFontImageName(CHAR16 c, CHAR16 *path) {
      CHAR16 code[10];
      itoa(c, code, 10);
      strcpy(path, (EFI_STRING)L"fonts\\");
      strcat(path, code);
      strcat(path, (EFI_STRING)L".png");
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

  namespace Main {
    static void (*onUpdate)();

    void _onKeyPress() {
      Input::readKeyStroke();
    }

    void _onTick() {
      Input::getPointerState();
      if (onUpdate) onUpdate();
    }

    void start(UINT64 tick_interval = 333'300) {
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
    }
  };

  void initGame(EFI_SYSTEM_TABLE *SystemTable) {
    libc::init(SystemTable);
    EfiGame::SystemTable = SystemTable;
    Input::initInput();
    Graphics::initGraphics();
    FileSystem::initFileSystem();
  }
};

using namespace EfiGame;

void* operator new(UINTN size) {
	return malloc(size);
}

void operator delete(void* p) {
  free(p);
}

extern "C" void __cxa_pure_virtual() { }

#define sceneId(Class) Class ## Id
#define delrareScene(Class) static Class * Class ## _obj
#define initScene(Class) Class ## _obj = new Class ()
#define sceneCase(Class) case Class ## Id : Class ## _obj -> update(); ++ Class ## _obj -> tick; break;
#define changeScene(Class) currentSceneId = Class ## Id

enum SceneId {
  sceneId(StartScene),
  sceneId(OpeningScene),
  sceneId(NovelScene),
  SceneIdMax,
};

static SceneId currentSceneId;

class Scene {
public:
  UINT64 tick;
};

class StartScene : public Scene {
public:
  void update() {
    if (tick == 1 || tick == 75) {
      Graphics::Pixel unitybgc {55, 44, 33, 0};
      Graphics::fillRect(0, 0, Graphics::HorizontalResolution, Graphics::VerticalResolution, unitybgc);
    }
    if (tick == 5) {
      auto *image = Graphics::loadImageFromFile((EFI_STRING)L"Uefi_logo_s_bg.png");
      Graphics::drawImage(image, (Graphics::HorizontalResolution - image->x) / 2, (Graphics::VerticalResolution - image->y) / 2, false);
      free(image);
      // Graphics::drawImage(image, 0, 0, false);
    }
    if (tick == 80) changeScene(OpeningScene);
  }
};

static INT32 prevX = -1;
static INT32 prevY = -1;
static Graphics::Image* backupPx;
static Graphics::Image* cursorImage;

class OpeningScene : public Scene {
public:
  void update() {
    if (tick == 0) setEventHandlers();
    if (tick == 1) {
      Graphics::Pixel white {255, 255, 255, 0};
      Graphics::fillRect(0, 0, Graphics::HorizontalResolution, Graphics::VerticalResolution, white);
      auto *image = Graphics::loadImageFromFile((EFI_STRING)L"title_logo.png");
      Graphics::drawImage(image, (Graphics::HorizontalResolution - image->x) / 2, (Graphics::VerticalResolution - image->y) / 2 - 50);
      free(image);
    }
    if (tick % 30 == 1) {
      Graphics::Pixel black {0, 0, 0, 0};
      Graphics::drawStr((EFI_STRING)L"PUSH ANY KEY", black, (Graphics::HorizontalResolution - 150) / 2, Graphics::VerticalResolution/ 2 + 100);
    }
    if (tick % 30 == 16) {
      Graphics::Pixel white {255, 255, 255, 0};
      Graphics::fillRect((Graphics::HorizontalResolution - 150) / 2, Graphics::VerticalResolution/ 2 + 100, 150, 20, white);
    }
  }

  static void setEventHandlers() {
    Input::onMouseLeftClick = &onMouseLeftClick;
    Input::setTrackMouseScreenCoordinate(true, Graphics::HorizontalResolution / 2, Graphics::VerticalResolution / 2);
    Input::onMouseMove = &onMouseMove;
    Input::onKeyPress = &onKeyPress;
  }

  static void clearEventHandlers() {
    Input::onMouseLeftClick = nullptr;
    Input::onKeyPress = nullptr;
  }

  static void backupCursorPixel() {
    Graphics::Pixel *base = (Graphics::Pixel *)Graphics::GraphicsOutputProtocol->Mode->FrameBufferBase;
    for (INT32 dy = 0; dy < backupPx->y; ++dy) {
      INT32 base_yoffset = Graphics::HorizontalResolution * (Input::mouse.y + dy);
      INT32 px_yoffset = backupPx->x * dy;
      for (INT32 dx = 0; dx < backupPx->x; ++dx) {
        auto px_index = px_yoffset + dx;
        auto base_index = base_yoffset + Input::mouse.x + dx;
        backupPx->pixels[px_index] = base[base_index];
      }
    }
  }

  static void onMouseMove(INT32 rx, INT32 ry) {
    // if (!backupPx) backupPx = Graphics::loadImageFromFile((EFI_STRING)L"cursor.png");
    // if (!cursorImage) cursorImage = Graphics::loadImageFromFile((EFI_STRING)L"cursor.png");
    if (prevX >= 0) Graphics::drawImage(backupPx, prevX, prevY);
    backupCursorPixel();
    Graphics::drawImage(cursorImage, Input::mouse.x, Input::mouse.y);
    prevX = Input::mouse.x;
    prevY = Input::mouse.y;
  }

  static void onMouseLeftClick() {
    changeToNextScene();
  }

  static void onKeyPress(CHAR16 c) {
    changeToNextScene();
  }

  static void changeToNextScene() {
    clearEventHandlers();
    changeScene(NovelScene);
  }
};

static BOOLEAN novelToNext;
class NovelScene : public Scene {
  #define MAX_SCENARIO_SIZE 4096
  #define WIDTH 800
  #define HEIGHT 600
  #define MSGBOX_PAD 20
  #define MSGBOX_TOP 450
  #define MSGBOX_BORDER 10
  #define NAMEBOX_WIDTH 160
  #define NAMEBOX_HEIGHT 40
  #define TEXT_PAD 10
  #define CHARA_PAD 100
  #define CHARA0_TOP 0
  #define CHARA1_TOP 250
public:
  CHAR16 bg_filename[50];
  Graphics::Image* bg_image;
  Graphics::Image* chara[2];
  CHAR16 *scenario;
  CHAR16 *scenarioPos;
  CHAR16 name[12];
  CHAR16 text[128];
  CHAR16 leftChara;
  CHAR16 rightChara;
  BOOLEAN textanim;
  INT32 x0;
  INT32 y0;

  void update() {
    if (tick > 3 && novelToNext) {
      next();
    } else if (tick == 0) {
      Console::writeLine((EFI_STRING)L"LOADING...");
      init();
      Console::writeLine((EFI_STRING)L"DONE.");
    } else if (tick == 1) {
      Graphics::Pixel black {0, 0, 0, 0};
      Graphics::fillRect(0, 0, Graphics::HorizontalResolution, Graphics::VerticalResolution, black);
    } else if (tick == 2) {
      next();
    }
  }

  void init() {
    bg_filename[0] = L'\0';
    name[0] = L'\0';
    text[0] = L'\0';
    leftChara = L'-';
    rightChara = L'-';
    novelToNext = false;
    textanim = false;
    x0 = (Graphics::HorizontalResolution - WIDTH) / 2;
    y0 = (Graphics::VerticalResolution - HEIGHT) / 2;
    EFI_FILE_PROTOCOL *file;
    file = FileSystem::open((EFI_STRING)L"scenario.txt");
    FileSystem::read(file, scenario, sizeof(CHAR16) * MAX_SCENARIO_SIZE);
    FileSystem::close(file);
    scenarioPos = scenario;
    setEventHandlers();
  }

  void updateBg() {
    drawBg();
    drawLeftChara();
    drawRightChara();
    drawNameBox();
    drawName();
    drawMsgBox();
    drawText();
  }

  void updateChara() {
    updateBg();
  }

  void updateName() {
    drawNameBox();
    drawName();
  }

  void updateText() {
    drawMsgBox();
    drawText();
  }

  void drawBg() {
    // なぜだか分からないがnullになっているのでロード
    if (!bg_image && strlen(bg_filename)) bg_image = Graphics::loadImageFromFile(bg_filename);
    if (bg_image) Graphics::drawImage(bg_image, x0, y0, false);
  }

  void drawLeftChara() {
    if (leftChara == L'0') {
      Graphics::drawImage(chara[0], x0 + CHARA_PAD, y0 + CHARA0_TOP);
    } else if (leftChara == L'1') {
      Graphics::drawImage(chara[1], x0 + CHARA_PAD, y0 + CHARA1_TOP);
    }
  }

  void drawRightChara() {
    if (rightChara == L'0') {
      Graphics::drawImage(chara[0], x0 + WIDTH - CHARA_PAD - chara[0]->x, y0 + CHARA0_TOP);
    } else if (rightChara == L'1') {
      Graphics::drawImage(chara[1], x0 + WIDTH - CHARA_PAD - chara[1]->x, y0 + CHARA1_TOP);
    }
  }

  void drawNameBox() {
    if (!strlen(name)) return;
    Graphics::Pixel pink {220, 120, 255, 0};
    Graphics::fillRect(x0 + MSGBOX_PAD, y0 + MSGBOX_TOP - NAMEBOX_HEIGHT, NAMEBOX_WIDTH, NAMEBOX_HEIGHT, pink);
  }

  void drawName() {
    Graphics::Pixel white {220, 255, 255, 0};
    Graphics::drawStr(name, white, x0 + MSGBOX_PAD + TEXT_PAD, y0 + MSGBOX_TOP - NAMEBOX_HEIGHT + TEXT_PAD);
  }

  void drawMsgBox() {
    Graphics::Pixel pink {220, 120, 255, 0};
    Graphics::Pixel white {255, 255, 255, 0};
    // まんなか
    Graphics::fillRect(x0 + MSGBOX_PAD + MSGBOX_BORDER, y0 + MSGBOX_TOP + MSGBOX_BORDER, WIDTH - (MSGBOX_PAD + MSGBOX_BORDER) * 2, HEIGHT - MSGBOX_TOP - MSGBOX_PAD - MSGBOX_BORDER * 2, pink);
    // |
    Graphics::fillRect(x0 + MSGBOX_PAD, y0 + MSGBOX_TOP, MSGBOX_BORDER, HEIGHT - MSGBOX_TOP - MSGBOX_PAD, white);
    //  ^
    Graphics::fillRect(x0 + MSGBOX_PAD, y0 + MSGBOX_TOP, WIDTH - MSGBOX_PAD * 2, MSGBOX_BORDER, white);
    //  _
    Graphics::fillRect(x0 + MSGBOX_PAD, y0 + HEIGHT - MSGBOX_PAD - MSGBOX_BORDER, WIDTH - MSGBOX_PAD * 2, MSGBOX_BORDER, white);
    //   |
    Graphics::fillRect(x0 + WIDTH - MSGBOX_PAD - MSGBOX_BORDER, y0 + MSGBOX_TOP, MSGBOX_BORDER, HEIGHT - MSGBOX_TOP - MSGBOX_PAD, white);
  }

  void drawText() {
    Graphics::Pixel white {255, 255, 255, 0};
    Graphics::drawStr(text, white, x0 + MSGBOX_PAD + MSGBOX_BORDER + TEXT_PAD, y0 + MSGBOX_TOP + MSGBOX_BORDER + TEXT_PAD, WIDTH - (MSGBOX_PAD + MSGBOX_BORDER + TEXT_PAD) * 2);
  }

  void next() {
    novelToNext = false;
    bool bgChanged = false;
    bool charaChanged = false;
    bool nameChanged = false;
    bool textChanged = false;
    CHAR16 debug[10];
    debug[9] = '\0';
    INT32 text_index = 0;
    while(TRUE) {
      if (*scenarioPos == L'-') {
        // Console::write((EFI_STRING)L"-");
        while (*scenarioPos++ != L'\n');
        break;
      } else if (*scenarioPos == L'#') {
        // Console::write((EFI_STRING)L"#");
        bgChanged = true;
        ++scenarioPos;
        INT32 i = 0;
        while (TRUE) {
          if (*scenarioPos == L'\r') {
            bg_filename[i] = L'\0';
            break;
          }
          bg_filename[i] = *scenarioPos;
          ++scenarioPos;
          ++i;
        }
        scenarioPos += 2;
        bg_image = Graphics::loadImageFromFile(bg_filename);
      } else if (*scenarioPos == L':') {
        // Console::write((EFI_STRING)L":");
        charaChanged = true;
        ++scenarioPos;
        CHAR16 lr = *scenarioPos;
        ++scenarioPos;
        CHAR16 charaId = *scenarioPos;
        ++scenarioPos;
        ++scenarioPos;
        CHAR16 fileName[100];
        INT32 i = 0;
        while (TRUE) {
          if (*scenarioPos == L'\r') {
            fileName[i] = L'\0';
            break;
          }
          fileName[i] = *scenarioPos;
          ++scenarioPos;
          ++i;
        }
        scenarioPos += 2;
        if (lr == L'L') {
          leftChara = charaId;
        } else {
          rightChara = charaId;
        }
        INT32 charaIdNum = -1;
        if (charaId == L'0') {
          charaIdNum = 0;
        } else if (charaId == L'1') {
          charaIdNum = 1;
        }
        chara[charaIdNum] = Graphics::loadImageFromFile(fileName);
      } else if (*scenarioPos == L'@') {
        // Console::write((EFI_STRING)L"@");
        nameChanged = true;
        ++scenarioPos;
        INT32 i = 0;
        while (TRUE) {
          if (*scenarioPos == L'\r') {
            name[i] = L'\0';
            break;
          }
          name[i] = *scenarioPos;
          ++scenarioPos;
          ++i;
        }
        scenarioPos += 2;
      } else if (*scenarioPos == L'>') {
        // Console::write((EFI_STRING)L">");
        textChanged = true;
        ++scenarioPos;
        while (TRUE) {
          if (*scenarioPos == L'\r') {
            text[text_index] = L'\r';
            text[text_index + 1] = L'\n';
            text[text_index + 2] = L'\0';
            text_index += 2;
            break;
          }
          text[text_index] = *scenarioPos;
          ++scenarioPos;
          ++text_index;
        }
        scenarioPos += 2;
      } else {
        textChanged = true;
        *(scenarioPos + 20) = '\0';
        strcpy(text, (EFI_STRING)L"シナリオエラー!:");
        strcat(text, scenarioPos);
        break;
      }
    }
    if (bgChanged || charaChanged) {
      updateBg();
    } else {
      if (nameChanged) updateName();
      if (textChanged) updateText();
    }
  }

  static void setEventHandlers() {
    Input::onMouseLeftClick = &onMouseLeftClick;
    Input::onKeyPress = &onKeyPress;
  }

  static void onMouseLeftClick() {
    novelToNext = true;
  }

  static void onKeyPress(CHAR16 c) {
    novelToNext = true;
  }
};

delrareScene(StartScene);
delrareScene(OpeningScene);
delrareScene(NovelScene);

static UINT64 globalTick;
class Game {
private:
public:
  static void start() {
    initScene(StartScene);
    initScene(OpeningScene);
    initScene(NovelScene);

    changeScene(StartScene);

    backupPx = Graphics::loadImageFromFile((EFI_STRING)L"cursor.png");
    cursorImage = Graphics::loadImageFromFile((EFI_STRING)L"cursor.png");

    Main::onUpdate = &onUpdate;
    Main::start();
  }
private:
  static void onUpdate() {
    switch (currentSceneId) {
      sceneCase(StartScene)
      sceneCase(OpeningScene)
      sceneCase(NovelScene)
    }

    /*if (!(globalTick % 30)) {
      Console::write((EFI_STRING)L"scene:");
      Console::writeNum(currentSceneId);
      Console::write((EFI_STRING)L" ");
      Console::writeNum(globalTick / 30);
      Console::write((EFI_STRING)L"\r");
    }*/
    ++globalTick;
  }
};

extern "C" void efi_main(void *ImageHandle __attribute__ ((unused)), EFI_SYSTEM_TABLE *SystemTable) {
  initGame(SystemTable);
  // Graphics::maximizeResolution();
  Game::start();
  return;

  Console::clear();
  Console::write((EFI_STRING)L"Hello!\r\nUEFI!\r\n");
  Graphics::Pixel white {255, 255, 255, 0};
  Graphics::fillRect(0, 0, Graphics::HorizontalResolution, Graphics::VerticalResolution, white);
  Graphics::Pixel black {0, 0, 0, 0};
  Graphics::Pixel color {0, 127, 255, 0};
  Graphics::Pixel color2 {255, 127, 255, 0};
  Graphics::Pixel color3 {127, 127, 0, 0};
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
    Console::write((EFI_STRING)L"- ");
    Console::writeLine((EFI_STRING)info->FileName);
  }
  Console::write((EFI_STRING)L"loading... ");
  auto *image = Graphics::loadImageFromFile((EFI_STRING)L"surface0.png");
  Console::writeLine((EFI_STRING)L"done");
  Console::writeNumLine(image->x);
  Console::writeNumLine(image->y);
  Console::writeNumLine(image->length);
  Console::writeNumLine(image->composition);
  Console::writeLine((EFI_STRING)L"---");
  Graphics::drawImage(image, 10, 10);
  Graphics::drawImage(image, 100, 100);
  Graphics::drawImage(image, 200, 200, false);
  Graphics::drawChar(L'あ', 300, 300);
  Graphics::drawStr((EFI_STRING)L"優子「みんながなかよくなりますようにー！！」\r\nEND", black, 300, 400, 200);
  // while (TRUE);
  auto rect = Graphics::getRectImage(100, 60, color2);
  auto circle = Graphics::getCircleImage(50, color3);
  EFI_INPUT_KEY key;
  CHAR16 str[2];
  str[1] = L'\0';
  UINT32 offset = 0;
  while (TRUE) {
    if (EFI_SUCCESS == SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key)) {
      str[0] = key.UnicodeChar;
      if (str[0] == L'\r') {
        Console::write((EFI_STRING)L"\r\n");
      } else {
        Console::write(str);
      }
    }

    Graphics::drawImage(rect, 550, offset - 60, false);
    Graphics::drawImage(circle, 550, offset - 50);
    // Graphics::fillRect(550, offset - 60, 100, 60, color2);
    // Graphics::fillCircle(600, offset, 50, color3);
    offset++;
    if (offset >= Graphics::VerticalResolution) offset = 0;
  }
}
