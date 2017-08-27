#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

namespace EfiGame {
  typedef CHAR16* STRING;

  static EFI_SYSTEM_TABLE *SystemTable;

  class Input {
  public:
    /** キー入力1つを読み込む キー入力まで待機する */
    static auto getChar() {
      EFI_INPUT_KEY key;
      UINT64 waitIdx;
      SystemTable->BootServices->WaitForEvent(1, &(SystemTable->ConIn->WaitForKey), &waitIdx);
      SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
      return key.UnicodeChar;
    }

    /** 改行が来るまでキー入力を読み込む(改行を含まない) バッファの終端がくるか改行キー入力まで待機する */
    static auto getLine(STRING str, INT32 size) {
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

    static auto onKeyEvent() {

    }
  };

  class Console {
  public:
    static auto clear() {
      SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    }

    static auto write(STRING str) {
      SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
    }

    static auto writeLine(STRING str) {
      write(str);
      write((STRING)L"\r\n");
    }
  };

  static EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutputProtocol;

  class Graphic {
  private:
  public:
    static auto init() {
      SystemTable->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, nullptr, (void**)&GraphicsOutputProtocol);
    }

    static auto getMaxResolutionMode(BOOLEAN horizontal = true) {
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

    static auto setMode(UINT32 mode) {
      GraphicsOutputProtocol->SetMode(GraphicsOutputProtocol, mode);
    }

    static auto maximizeResolution(BOOLEAN hosizontal = true) {
      setMode(getMaxResolutionMode(hosizontal));
    }
  };

  void initGame(EFI_SYSTEM_TABLE *SystemTable) {
    EfiGame::SystemTable = SystemTable;
    Graphic::init();
  }
};


extern "C" void efi_main(void *ImageHandle __attribute__ ((unused)), EFI_SYSTEM_TABLE *SystemTable) {
  EfiGame::initGame(SystemTable);
  EfiGame::Graphic::maximizeResolution();

  EfiGame::Console::clear();
  EfiGame::Console::write((EfiGame::STRING)L"Hello!\r\nUEFI!\r\n");
  CHAR16 str[5];
  while (TRUE) {
    EfiGame::Input::getLine(str, 5);
    EfiGame::Console::writeLine(str);
  }
}
