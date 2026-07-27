; XPU - src/math/xpu_math_asm_x86.asm
;
; Hand-written Assembly optimizations for x86_64.
; This file is OPTIONAL - the same routines are also implemented in
; xpu_math_sse.cpp using intrinsics. Use this file when you need
; absolute maximum control over instruction scheduling (e.g. for
; competitive benchmarking or to work around compiler issues).
;
; Build with NASM:  nasm -f elf64 xpu_math_asm_x86.asm -o xpu_math_asm_x86.o
; Or with GNU as:   as -64 xpu_math_asm_x86.asm -o xpu_math_asm_x86.o
;
; The .asm version uses NASM syntax. A .S version (GNU as) is in
; xpu_math_asm_x86.S for cross-assembler compatibility.
;
; Exported symbols (C calling convention, System V AMD64 ABI):
;   xpu_asm_vec4_dot_x86   - dot product, 4 floats, uses SSE4.1 dpps
;   xpu_asm_vec4_length_x86 - length = sqrt(dot(a,a)), uses SSE4.1 + sqrtss
;   xpu_asm_memzero_x86    - memset(p, 0, n) using non-temporal stores
;   xpu_asm_memcpy_x86     - memcpy using SSE2 movdqa

%ifdef NASM
section .text
global xpu_asm_vec4_dot_x86
global xpu_asm_vec4_length_x86
global xpu_asm_memzero_x86
global xpu_asm_memcpy_x86

; float xpu_asm_vec4_dot_x86(const float* a, const float* b)
; rdi = a, rsi = b
align 16
xpu_asm_vec4_dot_x86:
    movaps  xmm0, [rdi]      ; load a (4 floats)
    movaps  xmm1, [rsi]      ; load b
    dpps    xmm0, xmm1, 0xF1 ; dot product, broadcast to lane 0
    movss   eax, xmm0        ; return as float in xmm0 (System V returns floats in xmm0)
    ret

; float xpu_asm_vec4_length_x86(const float* a)
; rdi = a
align 16
xpu_asm_vec4_length_x86:
    movaps  xmm0, [rdi]
    dpps    xmm0, xmm0, 0xF1
    sqrtss  xmm0, xmm0
    ret

; void xpu_asm_memzero_x86(void* dst, size_t n)
; rdi = dst, rsi = n
align 16
xpu_asm_memzero_x86:
    xorps   xmm0, xmm0       ; zero register
    mov     rax, rdi
    mov     rcx, rsi
    shr     rcx, 4           ; n / 16 (loop count)
    jz      .tail
.loop:
    movdqu  [rax], xmm0
    add     rax, 16
    dec     rcx
    jnz     .loop
.tail:
    ; handle remaining bytes (n & 15)
    mov     rcx, rsi
    and     rcx, 15
    jz      .done
.byte_loop:
    mov     byte [rax], 0
    inc     rax
    dec     rcx
    jnz     .byte_loop
.done:
    ret

; void xpu_asm_memcpy_x86(void* dst, const void* src, size_t n)
; rdi = dst, rsi = src, rdx = n
align 16
xpu_asm_memcpy_x86:
    mov     rax, rdi
    mov     r8,  rsi
    mov     rcx, rdx
    shr     rcx, 4
    jz      .tail
.loop:
    movdqu  xmm0, [r8]
    movdqu  [rax], xmm0
    add     rax, 16
    add     r8,  16
    dec     rcx
    jnz     .loop
.tail:
    mov     rcx, rdx
    and     rcx, 15
    jz      .done
.byte_loop:
    mov     r8b, [r8]
    mov     [rax], r8b
    inc     rax
    inc     r8
    dec     rcx
    jnz     .byte_loop
.done:
    ret
%endif
