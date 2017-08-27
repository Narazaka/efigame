#include <Uefi.h>

namespace EfiGame {
  typedef CHAR16* STRING;

  static EFI_SYSTEM_TABLE *SystemTable;

  void initGame(EFI_SYSTEM_TABLE *SystemTable) {
    EfiGame::SystemTable = SystemTable;
  }

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
};


extern "C" void efi_main(void *ImageHandle __attribute__ ((unused)), EFI_SYSTEM_TABLE *SystemTable) {
  EfiGame::initGame(SystemTable);

  EfiGame::Console::clear();
  EfiGame::Console::write((EfiGame::STRING)L"Hello!\r\nUEFI!\r\n");
  CHAR16 str[5];
  while (TRUE) {
    EfiGame::Input::getLine(str, 5);
    EfiGame::Console::writeLine(str);
  }
}
