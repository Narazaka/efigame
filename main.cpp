#include "efigame.h"

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
