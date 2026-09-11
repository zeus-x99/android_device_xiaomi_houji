// Entry replaces ldr w11, [x19, #0x10] in DestroyPipelineDescriptor.
// x19 is PipelineDescriptor; output wrappers start at +0x38, stride 0x28.
// Only caller-saved registers are used. No calls, allocation, or stack changes.
.section .text.fix,"ax",%progbits
.global houji_dedup_outputs
houji_dedup_outputs:
    ldr w11, [x19, #0x10]
    mov x8, #0
    mov x9, #0x28
    add x10, x19, #0x38
1:
    cmp w8, w11
    b.hs 5f
    madd x12, x8, x9, x10
    ldr x13, [x12]
    cbz x13, 4f
    add x14, x8, #1
2:
    cmp w14, w11
    b.hs 4f
    madd x15, x14, x9, x10
    ldr x16, [x15]
    cmp x16, x13
    b.ne 3f
    str xzr, [x15]
3:
    add x14, x14, #1
    b 2b
4:
    add x8, x8, #1
    b 1b
5:
    ldr w11, [x19, #0x10]
    b houji_original_resume
