    .syntax unified
    .cpu arm7tdmi
    .arm
    .section .text.start,"ax",%progbits
    .global _start
_start:
    b _start_code
    .space 188
_start_code:
    mov r0, #0x12
    msr cpsr_c, r0
    ldr sp, =0x03007FA0
    mov r0, #0x1f
    msr cpsr_c, r0
    ldr sp, =0x03007F00
    ldr r0, =__iwram_lma
    ldr r1, =__iwram_start
    ldr r2, =__iwram_end
0:  cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 0b
    ldr r0, =__data_lma
    ldr r1, =__data_start
    ldr r2, =__data_end
1:  cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 1b
    ldr r1, =__bss_start
    ldr r2, =__bss_end
    mov r3, #0
2:  cmp r1, r2
    strlo r3, [r1], #4
    blo 2b
    bl main
3:  b 3b
