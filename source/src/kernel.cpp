
#include <common/types.h>
#include <common/universalfunc.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>
#include <drivers/amd_am79c973.h>
#include <common/programs/terminal.h>


//#define GRAPHICSMODE


using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;



void putchar(unsigned char ch, unsigned char forecolor, 
		unsigned char backcolor, uint8_t x, uint8_t y) {

	uint16_t attrib = (backcolor << 4) | (forecolor & 0x0f);
	volatile uint16_t* vidmem;
	vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
	*vidmem = ch | (attrib << 8);
}


void printfTUI(uint8_t forecolor, uint8_t backcolor, 
		uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
		bool shadow) {


	for (uint8_t y = 0; y < 25; y++) {
		
		for (uint8_t x = 0; x < 80; x++) {

			putchar(0xff, 0x00, backcolor, x, y);
		}
	}
	
	uint8_t resetX = x1;


	while (y1 < y2) {
	
		while (x1 < x2) {
		
			putchar(0xff, 0x00, forecolor, x1, y1);
			x1++;
		}
		y1++;
		
		//side shadow
		if (shadow) {
		
			putchar(0xff, 0x00, 0x00, x1, y1);
		}
		x1 = resetX;
	}

	//bottom shadow
	if (shadow) {
		
		for (resetX++; resetX < (x2 + 1); resetX++) {
	
			putchar(0xff, 0x00, 0x00, resetX, y1);
		}
	}
}




void printfColor(char* str, uint8_t forecolor, uint8_t backcolor, uint8_t x, uint8_t y) {

	for (int i = 0; str[i] != '\0'; i++) {

		if (str[i] == '\n') {		
		
			y++;
			x = 0;

		} else {
			
			putchar(str[i], forecolor, backcolor, x, y);
			x++;
		}

		if (x >= 80) {
			
			y++;
			x = 0;
		}

		if (y >= 25) {
		
			y = 0;
		}
	}
}



void printf(char* str) {
	
	static uint8_t x = 0, y = 0;
	static bool cliCursor = false;

	//default gray on black text
	uint16_t attrib = 0x07;

	volatile uint16_t* vidmem;

		
	for (int i = 0; str[i] != '\0'; i++) {
		
		vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
		
		switch (str[i]) {
		
			case '\b':
				vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
				*vidmem = ' ' | (attrib << 8);
				vidmem--; *vidmem = '_' | (attrib << 8);
				x--;

				break;

			case '\n':
				*vidmem = ' ' | (attrib << 8);
				y++;
				x = 0;
					
				break;
			case '\v': //clear screen
				
				for (y = 0; y < 25; y++) {
					for (x = 0; x < 80; x++) {
					
						vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
						*vidmem = 0x00;
					}
				}
				x = 0;
				y = 0;
					
				break;

				
			default:
				*vidmem = str[i] | (attrib << 8);
				x++;
					
				break;
		}
	
		if (x >= 80) {
		
			y++;
			x = 0;
		}

		
		//scrolling	
		if (y >= 25) {
			
			uint16_t scroll_temp;

			for (y = 1; y < 25; y++) {	
				for (x = 0; x < 80; x++) {
					
					vidmem = (volatile uint16_t*)0xb8000 + (80*y+x);
					scroll_temp = *vidmem;
						
					vidmem -= 80;
					*vidmem = scroll_temp;
					
					if (y == 24) {
						
						vidmem = (volatile uint16_t*)0xb8000 + (1920+x);
						*vidmem = ' ' | (attrib << 8);
					}
				}
			}
			x = 0;
			y = 24;
		}
	}
}

void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}
void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}
void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}







class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    
};




void sysprintf(char* str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

void taskA()
{
    while(true)
        sysprintf("A");
}

void taskB()
{
    while(true)
        sysprintf("B");
}






typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}



extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("AMS-OSv0.6.1 developed by Hubert Topolski\n");

    GlobalDescriptorTable gdt;
    
    
    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
    size_t heap = 10*1024*1024;
    MemoryManager memoryManager(heap, (*memupper)*1024 - heap - 10*1024);
    
    printf("heap: 0x");
    printfHex((heap >> 24) & 0xFF);
    printfHex((heap >> 16) & 0xFF);
    printfHex((heap >> 8 ) & 0xFF);
    printfHex((heap      ) & 0xFF);
    
    void* allocated = memoryManager.malloc(1024);
    printf("\nallocated: 0x");
    printfHex(((size_t)allocated >> 24) & 0xFF);
    printfHex(((size_t)allocated >> 16) & 0xFF);
    printfHex(((size_t)allocated >> 8 ) & 0xFF);
    printfHex(((size_t)allocated      ) & 0xFF);
    printf("\n");
    
    TaskManager taskManager;
    /*Task task1(&gdt, taskA);
    Task task2(&gdt, taskB);
    taskManager.AddTask(&task1);
    taskManager.AddTask(&task2);
    */
    InterruptManager interrupts(0x20, &gdt, &taskManager);
    SyscallHandler syscalls(&interrupts, 0x80);
    
    printf("Initializing Hardware, Stage 1\n");
    
    #ifdef GRAPHICSMODE
        Desktop desktop(320,200, 0x00,0x00,0xA8);
    #endif
    
    DriverManager drvManager;
    
        #ifdef GRAPHICSMODE
            KeyboardDriver keyboard(&interrupts, &desktop);
        #else
            myos::drivers::PrintfKeyboardEventHandler kbhandler;
            KeyboardDriver keyboard(&interrupts, &kbhandler);
        #endif
        drvManager.AddDriver(&keyboard);
        
    
        #ifdef GRAPHICSMODE
            MouseDriver mouse(&interrupts, &desktop);
        #else
            MouseToConsole mousehandler;
            MouseDriver mouse(&interrupts, &mousehandler);
        #endif
        drvManager.AddDriver(&mouse);
        
        PeripheralComponentInterconnectController PCIController;
        PCIController.SelectDrivers(&drvManager, &interrupts);

        #ifdef GRAPHICSMODE
            VideoGraphicsArray vga;
        #endif
        
    printf("Initializing Hardware, Stage 2\n");
        drvManager.ActivateAll();
        
    printf("Initializing Hardware, Stage 3\n");

    #ifdef GRAPHICSMODE
        vga.SetMode(320,200,8);
        Window win1(&desktop, 10,10,20,20, 0xA8,0x00,0x00);
        desktop.AddChild(&win1);
        Window win2(&desktop, 40,15,30,30, 0x00,0xA8,0x00);
        desktop.AddChild(&win2);
    #endif



    
    //printf("\nS-ATA primary master: ");
    //AdvancedTechnologyAttachment ata0m(true, 0x1F0);
    //ata0m.Identify();
    /*Filesystem::Initialize();
    if(Filesystem::CreateFile("exmple.txt")){
        int fileHandle = Filesystem::OpenFile("exmple.txt");
        if(fileHandle >= 0){
            char buffer[512];
            Filesystem::CloseFile(fileHandle);
        }
        Filesystem::ListFiles();
    }

    Filesystem::Uninitialize();*/


    /*printf("\nS-ATA primary slave: ");
    AdvancedTechnologyAttachment ata0s(false, 0x1F0);
    ata0s.Identify();
    //ata0s.Write28(5, (uint8_t*)"", 25);
    //ata0s.Flush();
    
    printf("\nS-ATA secondary master: ");
    AdvancedTechnologyAttachment ata1m(true, 0x170);
    ata1m.Identify();
    
    printf("\nS-ATA secondary slave: ");
    AdvancedTechnologyAttachment ata1s(false, 0x170);
    ata1s.Identify();
    // third: 0x1E8
    // fourth: 0x168
    */
    //amd_am79c973* eth0 = (amd_am79c973*)(drvManager.drivers[2]);
    //eth0->Send((uint8_t*)"Hello Network", 13);

    
    interrupts.Activate();
    myos::common::programs::Terminal::Initialize();

    while(1)
    {
        #ifdef GRAPHICSMODE
            desktop.Draw(&vga);
        #endif
    }
    

}
