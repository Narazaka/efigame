#include <stdint.h>
typedef int8_t INT8;
typedef uint8_t UINT8;
typedef int16_t INT16;
typedef uint16_t UINT16;
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef int64_t INT64;
typedef uint64_t UINT64;
typedef int8_t CHAR8;
typedef int16_t CHAR16;
typedef void VOID;
// if 64bit
typedef UINT64 UINTN;
typedef UINTN EFI_STATUS;

struct EFI_SYSTEM_TABLE {
  char _buf[60];
  struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    uint64_t _buf;
    uint64_t (*OutputString)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
        uint16_t *String);
    uint64_t _buf2[4];
    uint64_t (*ClearScreen)(
        struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
  } *ConOut;
};

void efi_main(void *ImageHandle __attribute__ ((unused)),
    struct EFI_SYSTEM_TABLE *SystemTable) {
  SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
  SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello!\r\nUEFI!\r\n");
  while (1);
}
