; emitter_runtime.inc
; - NASM boilerplate header
; - runtime blob (_start, print_str, print_dec)

section .text

emit_start_shim:
	lea rsi, [rel emit_start]
	mov rdx, emit_start_len
	call emit_append
	ret

section .rodata

emit_hdr:
	db "BITS 64", 10
	db "DEFAULT REL", 10
	db "", 10
	db "; [ABI Contract]", 10
	db "; print_dec: takes RDI, clobbers RAX, RCX, RDX, R8-R11", 10
	db "", 10
	db "global _start", 10
	db "section .text", 10
emit_hdr_len: equ $-emit_hdr

emit_start:
	db "section .text", 10
	db "_start:", 10
	db "    call main", 10
	db "    mov rax, 60", 10
	db "    xor rdi, rdi", 10
	db "    syscall", 10
	db "", 10
	db "; runtime: print_str(rdi=ptr0)", 10
	db "print_str:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rsi, rdi", 10
	db "    xor rdx, rdx", 10
	db ".ps_len:", 10
	db "    cmp byte [rsi+rdx], 0", 10
	db "    je .ps_write", 10
	db "    inc rdx", 10
	db "    jmp .ps_len", 10
	db ".ps_write:", 10
	db "    mov rax, 1", 10
	db "    mov rdi, 1", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: print_dec(rdi)", 10
	db "print_dec:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    sub rsp, 40", 10
	db "    mov rax, rdi", 10
	db "    lea r8, [rbp-1]", 10
	db "    xor rcx, rcx", 10
	db "    cmp rax, 0", 10
	db "    jne .pd_digits", 10
	db "    mov byte [r8], '0'", 10
	db "    mov rcx, 1", 10
	db "    jmp .pd_write", 10
	db ".pd_digits:", 10
	db "    mov r9, 10", 10
	db ".pd_loop:", 10
	db "    xor rdx, rdx", 10
	db "    div r9", 10
	db "    add dl, '0'", 10
	db "    mov byte [r8], dl", 10
	db "    dec r8", 10
	db "    inc rcx", 10
	db "    test rax, rax", 10
	db "    jnz .pd_loop", 10
	db "    inc r8", 10
	db ".pd_write:", 10
	db "    mov rax, 1", 10
	db "    mov rdi, 1", 10
	db "    mov rsi, r8", 10
	db "    mov rdx, rcx", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
emit_start_len: equ $-emit_start
