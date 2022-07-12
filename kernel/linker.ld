OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

SECTIONS
{
    . = 0x0;
    .cpu_local (NOLOAD) : {
        KEEP(*(.cpu_local*))
    }
    __cpu_local_size = .;

    . = 0xffffffff80000000;

    .text : ALIGN(4096) {
        *(.text*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata*)
    }

    .data : ALIGN(4096) {
        *(.data*)
    }

    .limine_reqs : {
        KEEP(*(.limine_reqs))
    }

    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss*)
    }
}