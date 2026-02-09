#ifndef _STAB_H
#define _STAB_H

/* Definicje typów STABS dla TCC */
#define __define_stab(code, name, string) name = code,
enum {
    N_GSYM = 0x20,
    N_FNAME = 0x22,
    N_FUN = 0x24,
    N_STSYM = 0x26,
    N_LCSYM = 0x28,
    N_MAIN = 0x2a,
    N_RSYM = 0x40,
    N_SLINE = 0x44,
    N_DSLINE = 0x46,
    N_BSLINE = 0x48,
    N_SSYM = 0x60,
    N_SO = 0x64,
    N_LSYM = 0x80,
    N_BINCL = 0x82,
    N_SOL = 0x84,
    N_PSYM = 0xa0,
    N_EINCL = 0xa2,
    N_LBRAC = 0xc0,
    N_RBRAC = 0xe0,
    N_EXCL = 0xc2
};

#endif
