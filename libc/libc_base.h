#include <Uefi.h>

#ifndef LIBC_BASE_H
#define LIBC_BASE_H

namespace libc {
  EFI_SYSTEM_TABLE *SystemTable;
  EFI_BOOT_SERVICES *BootServices;

  void init(EFI_SYSTEM_TABLE *SystemTable) {
    libc::SystemTable = SystemTable;
    libc::BootServices = libc::SystemTable->BootServices;
  }
};

#endif
