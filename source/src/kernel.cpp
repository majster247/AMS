extern "C" void kernel_entry() {
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    const char* msg = "Hello x64 Terry-style kernel!";
    for(int i=0; msg[i]; ++i)
        vga[i] = (0x0F << 8) | msg[i];

    while(1) __asm__ __volatile__("hlt");
}
