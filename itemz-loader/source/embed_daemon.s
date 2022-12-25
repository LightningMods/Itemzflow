.intel_syntax noprefix
.section .rodata

# elf
.global daemon_eboot
.type   daemon_eboot, @object
.align  4
daemon_eboot:
    .incbin "../itemz-daemon/bin/eboot.bin"
daemon_eboot_end:
    .global daemon_eboot_size
    .type   daemon_eboot_size, @object
    .align  4
daemon_eboot_size:
    .int    daemon_eboot_end - daemon_eboot