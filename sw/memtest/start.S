/* Minimal MicroBlaze startup: vectors, stack, jump to main. No BSP. */

    .section .vectors.reset, "ax"
    .global _start
_start:
    brai    _crt

    .text
_crt:
    la      r1, r0, _stack_top
    brlid   r15, main
    nop
1:  bri     1b
