/*
 * XPU - src/math/xpu_math_asm_x86.S
 *
 * GNU as syntax version of the x86_64 Assembly optimizations.
 * This is the version that gets compiled into libxpu by default on
 * Linux (gcc -c). The .asm version is for NASM users (Windows mostly).
 *
 * Functions exported:
 *   xpu_asm_vec4_dot_x86    - 4-float dot product using dpps (SSE4.1)
 *   xpu_asm_vec4_length_x86 - vector length using dpps + sqrtss
 *   xpu_asm_memzero_x86     - SIMD memset(p, 0, n)
 *   xpu_asm_memcpy_x86      - SIMD memcpy using movdqu (SSE2)
 *
 * All functions follow the System V AMD64 ABI:
 *   - first 6 int/ptr args: rdi, rsi, rdx, rcx, r8, r9
 *   - float return value in xmm0
 *   - preserved registers: rbx, rbp, r12-r15
 *
 * Build: gcc -c xpu_math_asm_x86.S -o xpu_math_asm_x86.o
 */

    .text

    .global xpu_asm_vec4_dot_x86
    .type   xpu_asm_vec4_dot_x86, @function
    .align 16
xpu_asm_vec4_dot_x86:
    /* rdi = a (const float*), rsi = b (const float*) */
    movaps  (%rdi), %xmm0        /* load a */
    movaps  (%rsi), %xmm1        /* load b */
    dpps    $0xF1, %xmm1, %xmm0  /* dot product, result in lane 0 */
    ret
    .size xpu_asm_vec4_dot_x86, .-xpu_asm_vec4_dot_x86

    .global xpu_asm_vec4_length_x86
    .type   xpu_asm_vec4_length_x86, @function
    .align 16
xpu_asm_vec4_length_x86:
    /* rdi = a */
    movaps  (%rdi), %xmm0
    dpps    $0xF1, %xmm0, %xmm0
    sqrtss  %xmm0, %xmm0
    ret
    .size xpu_asm_vec4_length_x86, .-xpu_asm_vec4_length_x86

    .global xpu_asm_memzero_x86
    .type   xpu_asm_memzero_x86, @function
    .align 16
xpu_asm_memzero_x86:
    /* rdi = dst, rsi = n */
    xorps   %xmm0, %xmm0         /* zero register */
    mov     %rdi, %rax
    mov     %rsi, %rcx
    shr     $4, %rcx             /* n / 16 */
    jz      .tail_zero
.loop_zero:
    movdqu  %xmm0, (%rax)        /* store 16 zero bytes */
    add     $16, %rax
    dec     %rcx
    jnz     .loop_zero
.tail_zero:
    mov     %rsi, %rcx
    and     $15, %rcx
    jz      .done_zero
.byte_zero:
    movb    $0, (%rax)
    inc     %rax
    dec     %rcx
    jnz     .byte_zero
.done_zero:
    ret
    .size xpu_asm_memzero_x86, .-xpu_asm_memzero_x86

    .global xpu_asm_memcpy_x86
    .type   xpu_asm_memcpy_x86, @function
    .align 16
xpu_asm_memcpy_x86:
    /* rdi = dst, rsi = src, rdx = n */
    mov     %rdi, %rax
    mov     %rsi, %r8
    mov     %rdx, %rcx
    shr     $4, %rcx
    jz      .tail_cpy
.loop_cpy:
    movdqu  (%r8), %xmm0
    movdqu  %xmm0, (%rax)
    add     $16, %rax
    add     $16, %r8
    dec     %rcx
    jnz     .loop_cpy
.tail_cpy:
    mov     %rdx, %rcx
    and     $15, %rcx
    jz      .done_cpy
.byte_cpy:
    movzbl  (%r8), %r9d
    movb    %r9b, (%rax)
    inc     %rax
    inc     %r8
    dec     %rcx
    jnz     .byte_cpy
.done_cpy:
    ret
    .size xpu_asm_memcpy_x86, .-xpu_asm_memcpy_x86

    .section .note.GNU-stack,"",@progbits
