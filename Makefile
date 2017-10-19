all: fs/EFI/BOOT/BOOTX64.EFI fs/surface0.png

fs/EFI/BOOT/BOOTX64.EFI: main.cpp include/ProcessorBind.h
	mkdir -p fs/EFI/BOOT
	x86_64-w64-mingw32-g++ -std=c++14 -Wall -Wextra -e efi_main -Iuefi-headers/Include -Istb -Iinclude -Ilibc -nostdlib \
	-fno-builtin -Wl,--subsystem,10 -mno-stack-arg-probe -o $@ $<

include/ProcessorBind.h:
	mkdir -p include
	cp uefi-headers/Include/X64/ProcessorBind.h include/ProcessorBind.h

run: fs/EFI/BOOT/BOOTX64.EFI OVMF/OVMF.fd
	qemu-system-x86_64 -bios ./OVMF/OVMF.fd -hda fat:fs

OVMF/OVMF.fd:
	curl -L https://downloads.sourceforge.net/project/edk2/OVMF/OVMF-X64-r15214.zip -o ovmf_tmp.zip
	unzip ovmf_tmp.zip -d OVMF
	rm ovmf_tmp.zip

clean:
	rm -rf fs/EFI/BOOT/BOOTX64.EFI
	rm -rf include/ProcessorBind.h

.PHONY: clean

fs/surface0.png: surface0.png
	cp surface0.png fs/surface0.png
