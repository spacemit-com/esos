/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020/12/12     bernard      The first version
 */

INCLUDE "./platform/rt24/os0_rcpu/link_stacksize.lds"

OUTPUT_ARCH( "riscv" )

ENTRY(_start)

MEMORY
{
   RAM : ORIGIN = ENTRY_POINT, LENGTH = 2M
   RESOURCE_TABLE (rwx) : ORIGIN = 0x1006fc000, LENGTH = 16K
   DTB_TABLE (rwx) : ORIGIN = 0xc087c000, LENGTH = 16k
}

SECTIONS
{
    . = ENTRY_POINT ;
    __text_start = .;
    .text :
    {
	KEEP(*startup_gcc.o(*.text*))
        *(.text)                        /* remaining code */
        *(.text.*)                      /* remaining code */
        *(.rodata)                      /* read-only data (constants) */
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t*)

        /* section information for finsh shell */
        . = ALIGN(8);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;
        . = ALIGN(8);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;
        . = ALIGN(8);

        /* section information for initial. */
        . = ALIGN(8);
        __rt_init_start = .;
        KEEP(*(SORT(.rti_fn*)))
        __rt_init_end = .;
        . = ALIGN(8);

        __rt_utest_tc_tab_start = .;
        KEEP(*(UtestTcTab))
        __rt_utest_tc_tab_end = .;

        . = ALIGN(8);
        _etext = .;
    } > RAM

    .eh_frame_hdr :
    {
         *(.eh_frame_hdr)
         *(.eh_frame_entry)
    } > RAM
    .eh_frame : ONLY_IF_RO { KEEP (*(.eh_frame)) } > RAM

    . = ALIGN(8);
    __text_end = .;
    __text_size = __text_end - __text_start;

    .data :
    {
        *(.data)
        *(.data.*)

        *(.data1)
        *(.data1.*)

        . = ALIGN(8);
        PROVIDE( __global_pointer$ = . + 0x800 );

        *(.sdata)
        *(.sdata.*)
    } > RAM

    . = ALIGN(8);
    .ctors :
    {
        PROVIDE(__ctors_start__ = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE(__ctors_end__ = .);
    } > RAM

    .dtors :
    {
        PROVIDE(__dtors_start__ = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE(__dtors_end__ = .);
    } > RAM

    /* stack for dual core */
    .stack :
    {
        . = ALIGN(8);
	__StackLimit = .;
        . += __STACKSIZE__;
	__StackTop = .;
	PROVIDE( __rt_rvstack = .);
    } > RAM

    PROVIDE(__stack = __StackTop);
    PROVIDE( __rt_rvstack = .);

    .sbss :
    {
    __bss_start__ = .;
        *(.sbss)
        *(.sbss.*)
        *(.dynsbss)
        *(.scommon)
    } > RAM

    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(.dynbss)
        *(COMMON)
    __bss_end__ = .;
    } > RAM

    .version_id_table :
    {
	. = ALIGN(4);
	__version_id_table_start__ = .;
	*(.version_id_table)
    } > RAM
    
    _end = .;

    .resource_table :
    {
        . = ALIGN(4);
        __resource_table_start__ = .;
        *(.resource_table)
    } > RESOURCE_TABLE

    .dtb_table :
    {
        . = ALIGN(4);
        __dtb_table_start__ = .;
       *(.dtb_table)
    } > DTB_TABLE

    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    /* DWARF debug sections.
     * Symbols in the DWARF debugging sections are relative to the beginning
     * of the section so we begin them at 0.  */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
}
