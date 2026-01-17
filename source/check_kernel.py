import struct
import os

def verify_elf(filename):
    if not os.path.exists(filename):
        print(f"Błąd: Brak pliku {filename}")
        return

    with open(filename, 'rb') as f:
        content = f.read()

    # Sprawdzenie Magica Multiboot 2
    magic_idx = content.find(b'\xd6\x50\x52\xe8')
    if magic_idx != -1:
        print(f"[*] Multiboot2 Header: Znaleziony na offset {hex(magic_idx)}")
        if magic_idx > 32768:
            print("[!] BŁĄD: Nagłówek zbyt daleko! GRUB go nie zobaczy.")
    else:
        print("[!] BŁĄD: Brak nagłówka Multiboot2!")

    # Sprawdzenie Entry Point
    entry = struct.unpack('<Q', content[24:32])[0]
    print(f"[*] Entry Point: {hex(entry)}")

verify_elf('kernel.elf')