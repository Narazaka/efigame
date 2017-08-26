#include <Uefi.h>

extern "C" void efi_main(void *ImageHandle __attribute__ ((unused)),
    EFI_SYSTEM_TABLE *SystemTable) {
  SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
  SystemTable->ConOut->OutputString(SystemTable->ConOut, (CHAR16*)L"Hello!\r\nUEFI!\r\n");
  while (1);
}
