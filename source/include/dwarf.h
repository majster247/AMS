#ifndef _DWARF_H
#define _DWARF_H

/* Absolutne minimum tagów DWARF, żeby TCC się odczepił */
#define DW_TAG_compile_unit     0x11
#define DW_TAG_variable         0x34
#define DW_TAG_base_type        0x24
#define DW_TAG_pointer_type     0x0f
#define DW_TAG_subprogram       0x2e
#define DW_TAG_formal_parameter 0x05

#define DW_AT_name              0x03
#define DW_AT_low_pc            0x11
#define DW_AT_high_pc           0x12
#define DW_AT_decl_file         0x3a
#define DW_AT_decl_line         0x3b

#define DW_FORM_addr            0x01
#define DW_FORM_data1           0x0b
#define DW_FORM_strp            0x0e

#endif
