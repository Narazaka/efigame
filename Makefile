all: fs/EFI/BOOT/BOOTX64.EFI

fs/EFI/BOOT/BOOTX64.EFI: main.cpp
	mkdir -p fs/EFI/BOOT
	x86_64-w64-mingw32-g++ -Wall -Wextra -e efi_main -nostdlib \
	-fno-builtin -Wl,--subsystem,10 -o $@ $<

run: fs/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 -bios C:/usr/OVMF/OVMF.fd -hda fat:fs

clean:
	rm fs/EFI/BOOT/BOOTX64.EFI

.PHONY: clean
