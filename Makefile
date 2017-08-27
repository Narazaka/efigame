all: fs/EFI/BOOT/BOOTX64.EFI

fs/EFI/BOOT/BOOTX64.EFI: main.cpp include/ProcessorBind.h
	mkdir -p fs/EFI/BOOT
	x86_64-w64-mingw32-g++ -std=c++14 -Wall -Wextra -e efi_main -Iuefi-headers/Include -Iinclude -nostdlib \
	-fno-builtin -Wl,--subsystem,10 -o $@ $<

include/ProcessorBind.h:
	mkdir -p include
	cp uefi-headers/Include/X64/ProcessorBind.h include/ProcessorBind.h

run: fs/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 -bios C:/usr/OVMF/OVMF.fd -hda fat:fs

clean:
	rm fs/EFI/BOOT/BOOTX64.EFI
	rm include/ProcessorBind.h

.PHONY: clean
