OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    .limine_requests : {
        *(.limine_requests .limine_requests.*)
    }

    .limine_requests_delimiter : {
        *(.limine_requests_delimiter .limine_requests_delimiter.*)
    }

    . = ALIGN(4096);

    .text : {
        *(.text .text.*)
    }

    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
