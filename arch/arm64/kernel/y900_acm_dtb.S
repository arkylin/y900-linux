.section .init.rodata, "a"
.balign 8
.global y900_acm_dtb_begin
y900_acm_dtb_begin:
.incbin "/home/i/y900-m172-stage/acm.dtb"
.global y900_acm_dtb_end
y900_acm_dtb_end:
