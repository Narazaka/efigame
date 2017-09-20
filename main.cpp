//#include "scenes.cpp"

#include "efigame.h"
#define EFIGAME_IMPLEMENTATION
#include "efigame.h"

using namespace EfiGame;

extern "C" void efi_main(void *ImageHandle __attribute__ ((unused)), EFI_SYSTEM_TABLE *SystemTable) {
  initGame(SystemTable);
  // Graphics::maximizeResolution();
  //Game::start();
  //return;

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
