#include <Uefi.h>
#include <libc_base.h>
#include <string.h>

#ifndef STDLIB_H
#define STDLIB_H

void* malloc(UINTN size) {
  void *ptr;
  EFI_STATUS Status = libc::BootServices->AllocatePool(EfiLoaderData, size, &ptr);
  return Status == EFI_SUCCESS ? ptr : nullptr;
}

void free(void *ptr) {
  libc::BootServices->FreePool(ptr);
}

void* realloc_sized(void *ptr, UINTN old_size, UINTN new_size) {
  if (!new_size) return nullptr;
  void* new_ptr = malloc(new_size);
  UINTN copy_size = old_size < new_size ? old_size : new_size;
  memcpy(new_ptr, ptr, copy_size);
  return new_ptr;
}

auto itoa(INT64 val, CHAR16* str, INT32 radix) {
  CHAR16 digit;
  CHAR16 reverse_str[30];
  bool negative = val < 0;
  if (negative) val = -val;
  INT32 i = 0;
  do {
    digit = val % radix;
    reverse_str[i] = digit < 10 ? L'0' + digit : L'A' + digit - 10;
    val /= radix;
    ++i;
  } while (val);
  INT32 length = i;
  if (negative) str[0] = L'-';
  for (i = 0; i < length; ++i) {
    str[i + negative] = reverse_str[length - i - 1];
  }
  str[length + negative] = L'\0';
  return str;
}

#endif
