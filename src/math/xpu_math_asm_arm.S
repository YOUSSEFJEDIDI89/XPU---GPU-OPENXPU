/*
 * XPU - src/math/xpu_math_asm_arm.S
 *
 * Hand-written ARM NEON Assembly optimizations.
 * Works on both ARMv7-A (with NEON) and AArch64.
 *
 * Functions exported:
 *   xpu_asm_vec4_dot_arm   - 4-float dot product using vmlaq + vpadd
 *   xpu_asm_vec4_length_arm - vector length
 *   xpu_asm_memzero_arm    - SIMD memset using vst1q
 *   xpu_asm_memcpy_arm     - SIMD memcpy using vld1q/vst1q
 *
 * AAPCS (ARM Procedure Call Standard):
 *   - first 4 int/ptr args: r0, r1, r2, r3
 *   - float return in s0 (ARMv7) or s0 (AArch64)
 *
 * Build: gcc -c xpu_math_asm_arm.S -o xpu_math_asm_arm.o -mfpu=neon
 */

#if defined(__aarch64__)

    .text

    .global xpu_asm_vec4_dot_arm
    .type   xpu_asm_vec4_dot_arm, %function
    .align 4
xpu_asm_vec4_dot_arm:
    /* x0 = a, x1 = b */
    ldp     q0, q1, [x0]            /* load a into q0 (only need q0) */
    ldp     q2, q3, [x1]            /* load b into q2 (only need q2) */
    ldr     q0, [x0]
    ldr     q2, [x1]
    fmul    v0.4s, v0.4s, v2.4s
    faddp   v0.4s, v0.4s            /* pairwise add -> 2 elements */
    faddp   v0.4s, v0.4s            /* pairwise add -> 1 element */
    /* result in v0.s[0] */
    ret
    .size xpu_asm_vec4_dot_arm, .-xpu_asm_vec4_dot_arm

    .global xpu_asm_vec4_length_arm
    .type   xpu_asm_vec4_length_arm, %function
    .align 4
xpu_asm_vec4_length_arm:
    /* x0 = a */
    ldr     q0, [x0]
    fmul    v0.4s, v0.4s, v0.4s
    faddp   v0.4s, v0.4s
    faddp   v0.4s, v0.4s
    fsqrt   s0, s0
    ret
    .size xpu_asm_vec4_length_arm, .-xpu_asm_vec4_length_arm

    .global xpu_asm_memzero_arm
    .type   xpu_asm_memzero_arm, %function
    .align 4
xpu_asm_memzero_arm:
    /* x0 = dst, x1 = n */
    movi    v0.16b, #0
    mov     x2, x1
    lsr     x2, x2, #4
    cbz     x2, .Ltail_zero
.Lloop_zero:
    str     q0, [x0]
    add     x0, x0, #16
    subs    x2, x2, #1
    bne     .Lloop_zero
.Ltail_zero:
    and     x2, x1, #15
    cbz     x2, .Ldone_zero
.Lbyte_zero:
    strb    wzr, [x0], #1
    subs    x2, x2, #1
    bne     .Lbyte_zero
.Ldone_zero:
    ret
    .size xpu_asm_memzero_arm, .-xpu_asm_memzero_arm

    .global xpu_asm_memcpy_arm
    .type   xpu_asm_memcpy_arm, %function
    .align 4
xpu_asm_memcpy_arm:
    /* x0 = dst, x1 = src, x2 = n */
    mov     x3, x2
    lsr     x3, x3, #4
    cbz     x3, .Ltail_cpy
.Lloop_cpy:
    ldr     q0, [x1]
    str     q0, [x0]
    add     x0, x0, #16
    add     x1, x1, #16
    subs    x3, x3, #1
    bne     .Lloop_cpy
.Ltail_cpy:
    and     x3, x2, #15
    cbz     x3, .Ldone_cpy
.Lbyte_cpy:
    ldrb    w4, [x1], #1
    strb    w4, [x0], #1
    subs    x3, x3, #1
    bne     .Lbyte_cpy
.Ldone_cpy:
    ret
    .size xpu_asm_memcpy_arm, .-xpu_asm_memcpy_arm

#elif defined(__arm__) && defined(__ARM_NEON__)

    .syntax unified
    .text

    .global xpu_asm_vec4_dot_arm
    .type   xpu_asm_vec4_dot_arm, %function
    .align 4
xpu_asm_vec4_dot_arm:
    /* r0 = a, r1 = b */
    vld1.32 {q0}, [r0]
    vld1.32 {q1}, [r1]
    vmul.f32 q0, q0, q1
    vadd.f32 d0, d0, d1
    vpadd.f32 d0, d0, d0
    vmov    r0, s0
    bx      lr
    .size xpu_asm_vec4_dot_arm, .-xpu_asm_vec4_dot_arm

    .global xpu_asm_vec4_length_arm
    .type   xpu_asm_vec4_length_arm, %function
    .align 4
xpu_asm_vec4_length_arm:
    /* r0 = a */
    vld1.32 {q0}, [r0]
    vmul.f32 q0, q0, q0
    vadd.f32 d0, d0, d1
    vpadd.f32 d0, d0, d0
    vsqrt.f32 s0, s0
    vmov    r0, s0
    bx      lr
    .size xpu_asm_vec4_length_arm, .-xpu_asm_vec4_length_arm

    .global xpu_asm_memzero_arm
    .type   xpu_asm_memzero_arm, %function
    .align 4
xpu_asm_memzero_arm:
    /* r0 = dst, r1 = n */
    vmov.i32 q0, #0
    mov     r2, r1, lsr #4
    cmp     r2, #0
    beq     .Ltail_zero
.Lloop_zero:
    vst1.32 {q0}, [r0]!
    subs    r2, r2, #1
    bne     .Lloop_zero
.Ltail_zero:
    and     r2, r1, #15
    cmp     r2, #0
    beq     .Ldone_zero
.Lbyte_zero:
    strb    r3, [r0], #1
    subs    r2, r2, #1
    bne     .Lbyte_zero
.Ldone_zero:
    bx      lr
    .size xpu_asm_memzero_arm, .-xpu_asm_memzero_arm

#endif
