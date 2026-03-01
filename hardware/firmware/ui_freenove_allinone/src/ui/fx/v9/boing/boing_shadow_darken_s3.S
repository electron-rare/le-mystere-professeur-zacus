.section .text
.global boing_shadow_darken_half_s3
.type boing_shadow_darken_half_s3, @function

// a2 = uint16_t* p (16B aligned)
// a3 = n_px (multiple of 8)
boing_shadow_darken_half_s3:
    entry   a1, 16

    srli    a4, a3, 3         // blocks = n_px / 8
    beqz    a4, .Ldone

    movi    a5, 0x7BEF
    s16i    a5, a1, 0
    movi    a5, 1
    s16i    a5, a1, 2

    mov     a6, a1
    ee.vldbc.16 q7, a6        // mask
    addi.n  a6, a1, 2
    ee.vldbc.16 q6, a6        // ones

    mov     a8, a2            // out ptr
    ssai    1                 // SAR = 1 (right shift after multiply)

    loopnez a4, .Lloop
    ee.vld.128.ip  q0, a2, 16
    ee.vmul.s16    q0, q0, q6
    ee.andq        q0, q0, q7
    ee.vst.128.ip  q0, a8, 16
.Lloop:

.Ldone:
    retw.n
