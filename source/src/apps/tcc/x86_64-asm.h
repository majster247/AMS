/* Atrapa definicji asemblera dla x86_64 */
#ifdef DEF_ASM_OP0
DEF_ASM_OP0(pushf, 0x9c)
DEF_ASM_OP0(popf, 0x9d)
DEF_ASM_OP0(ret, 0xc3)
#endif
