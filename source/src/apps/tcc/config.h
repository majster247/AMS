#ifndef _CONFIG_H
#define _CONFIG_H

#define TCC_TARGET_X86_64 1
#define TCC_VERSION "0.9.27-ams"
#undef HAVE_SEMAPHORE_H

// Gdzie TCC ma szukać plików nagłówkowych na dysku AMS?
// (Stworzymy ten folder później na obrazie dysku)
#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include"
#define CONFIG_TCC_LIBPATHS "{B}/lib"
#define CONFIG_TCC_CRTPREFIX "{B}/lib"
#define CONFIG_TCC_ELFINTERP "/lib/ld-ams.so"

// Wyłączamy zbędne rzeczy, żeby było lżej
// #define TCC_IS_NATIVE 1 // To włączymy w flagach kompilatora

#endif