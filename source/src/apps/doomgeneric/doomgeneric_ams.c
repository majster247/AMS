#include <stdint.h>
#include "stdio.h"
#include "unistd.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "linux_syscalls.h"

/*
 * AMS backend for Doomgeneric.
 *
 * Ten backend uruchamia poprawnie pętlę silnika i korzysta z istniejących
 * syscalli AMS (serial + timer). Warstwa wyświetlania i klawiatury jest
 * celowo minimalna, aby nie destabilizować jądra - można ją rozbudować
 * o dedykowane syscall'e graficzne/wejścia.
 */

struct linux_timespec_local {
    long long tv_sec;
    long long tv_nsec;
};

void DG_Init(void) {
    write(1, "[DOOM] DG_Init()\n", 17);
}

void DG_DrawFrame(void) {
    ams_syscall(SYS_AMS_FB_BLIT,
                (uint64_t)DG_ScreenBuffer,
                (uint64_t)DOOMGENERIC_RESX,
                (uint64_t)DOOMGENERIC_RESY,
                0, 0);
}

void DG_SleepMs(uint32_t ms) {
    if (ms == 0) return;
    uint32_t start = DG_GetTicksMs();
    while ((uint32_t)(DG_GetTicksMs() - start) < ms) {
        // busy-wait fallback: kernel currently exposes no blocking sleep yet
    }
}

uint32_t DG_GetTicksMs(void) {
    struct linux_timespec_local ts;
    long rc = (long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0);
    if (rc < 0) {
        static uint32_t fallback = 0;
        fallback += 1;
        return fallback;
    }
    uint32_t ms = (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
    if (ms == 0) {
        static uint32_t fallback0 = 1;
        return fallback0++;
    }
    return ms;
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    int ev = get_key();
    if (ev == 0) return 0;
    int is_pressed = (ev > 0) ? 1 : 0;
    unsigned char c = (unsigned char)(ev > 0 ? ev : -ev);

    if (pressed) *pressed = is_pressed;
    if (!doomKey) return 1;

    switch (c) {
        case 'w': case 'W': *doomKey = KEY_UPARROW; break;
        case 's': case 'S': *doomKey = KEY_DOWNARROW; break;
        case 'a': case 'A': *doomKey = KEY_LEFTARROW; break;
        case 'd': case 'D': *doomKey = KEY_RIGHTARROW; break;
        case 'e': case 'E': *doomKey = KEY_USE; break;
        case 'f': case 'F': *doomKey = KEY_FIRE; break;
        case ' ': *doomKey = KEY_USE; break;
        case '\n': *doomKey = KEY_ENTER; break;
        case '\t': *doomKey = KEY_TAB; break;
        case 27: *doomKey = KEY_ESCAPE; break;
        default: *doomKey = (unsigned char)c; break;
    }
    return 1;
}

void DG_SetWindowTitle(const char* title) {
    (void)title;
}

int main(int argc, char** argv) {
    write(1, "[DOOM] Starting Doomgeneric...\n", 30);
    doomgeneric_Create(argc, argv);
    uint32_t next_tick = DG_GetTicksMs();
    const uint32_t frame_ms = 1000 / 35; // Doom tic rate
    while (1) {
        uint32_t now = DG_GetTicksMs();
        if ((int32_t)(now - next_tick) >= 0) {
            doomgeneric_Tick();
            next_tick += frame_ms;
            if ((int32_t)(now - next_tick) > (int32_t)(frame_ms * 4)) {
                next_tick = now + frame_ms;
            }
        }
    }
    return 0;
}
