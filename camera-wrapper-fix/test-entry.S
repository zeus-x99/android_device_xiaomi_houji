.text
.global run_stub
run_stub:
    stp x19, x30, [sp, #-16]!
    mov x19, x0
    bl houji_dedup_outputs
    ldp x19, x30, [sp], #16
    ret
.section .text.resume,"ax",%progbits
.global houji_original_resume
houji_original_resume:
    ret
