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

#endif
