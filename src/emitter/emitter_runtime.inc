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
	db "%define HEAP_SIZE 1048576", 10
	db "", 10
	db "; [ABI Contract]", 10
	db "; print_dec: takes RDI, clobbers RAX, RCX, RDX, R8-R11", 10
	db "; heap_alloc: takes RDI=size, returns RAX=ptr0 (or 0 on OOM)", 10
	db "; memcpy: takes RDI=dst, RSI=src, RDX=len, returns RAX=dst (clobbers RCX)", 10
	db "; streq: takes RDI=a, RSI=b, returns RAX=1 if equal else 0", 10
	db "; strlen: takes RDI=ptr0, returns RAX=len", 10
	db "; sys_read:  (RDI=fd, RSI=buf, RDX=len) -> RAX=bytes (or -errno)", 10
	db "; sys_write: (RDI=fd, RSI=buf, RDX=len) -> RAX=bytes (or -errno)", 10
	db "; sys_open:  (RDI=path, RSI=flags, RDX=mode) -> RAX=fd (or -errno)", 10
	db "; sys_fstat: (RDI=fd, RSI=statbuf) -> RAX=0 (or -errno)", 10
	db "; sys_close: (RDI=fd) -> RAX=0 (or -errno)", 10
	db "; sys_exit:  (RDI=code) -> does not return", 10
	db "", 10
	db "global _start", 10
	db "section .text", 10
emit_hdr_len: equ $-emit_hdr

emit_start:
	db "section .text", 10
	db "_start:", 10
	db "    ; init bump heap", 10
	db "    lea rax, [rel heap_buf]", 10
	db "    mov [rel heap_cur], rax", 10
	db "    ; pass Linux process args to main(argc, argv)", 10
	db "    mov rdi, [rsp]", 10
	db "    lea rsi, [rsp+8]", 10
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
	db "; runtime: heap_alloc(rdi=size) -> rax=ptr0", 10
	db "heap_alloc:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, [rel heap_cur]", 10
	db "    mov rcx, rdi", 10
	db "    add rcx, 7", 10
	db "    and rcx, -8", 10
	db "    lea rdx, [rel heap_buf]", 10
	db "    add rdx, HEAP_SIZE", 10
	db "    lea r8,  [rax + rcx]", 10
	db "    cmp r8, rdx", 10
	db "    ja .ha_oom", 10
	db "    mov [rel heap_cur], r8", 10
	db "    leave", 10
	db "    ret", 10
	db ".ha_oom:", 10
	db "    xor rax, rax", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: memcpy(rdi=dst, rsi=src, rdx=len) -> rax=dst", 10
	db "memcpy:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, rdi", 10
	db "    mov rcx, rdx", 10
	db "    rep movsb", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: streq(rdi=a, rsi=b) -> rax=1/0", 10
	db "streq:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, 1", 10
	db "    xor rcx, rcx", 10
	db ".se_loop:", 10
	db "    mov dl,  [rdi+rcx]", 10
	db "    mov r8b, [rsi+rcx]", 10
	db "    cmp dl, r8b", 10
	db "    jne .se_ne", 10
	db "    test dl, dl", 10
	db "    je .se_done", 10
	db "    inc rcx", 10
	db "    jmp .se_loop", 10
	db ".se_ne:", 10
	db "    xor rax, rax", 10
	db ".se_done:", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: strlen(rdi=ptr0) -> rax=len", 10
	db "strlen:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    xor rax, rax", 10
	db ".sl_loop:", 10
	db "    cmp byte [rdi+rax], 0", 10
	db "    je .sl_done", 10
	db "    inc rax", 10
	db "    jmp .sl_loop", 10
	db ".sl_done:", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: sys_read(rdi=fd, rsi=buf, rdx=len) -> rax", 10
	db "sys_read:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, 0", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: sys_write(rdi=fd, rsi=buf, rdx=len) -> rax", 10
	db "sys_write:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, 1", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: sys_open(rdi=path, rsi=flags, rdx=mode) -> rax", 10
	db "sys_open:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, 2", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: sys_fstat(rdi=fd, rsi=statbuf) -> rax", 10
	db "sys_fstat:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, 5", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: sys_close(rdi=fd) -> rax", 10
	db "sys_close:", 10
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
	db "    mov rax, 3", 10
	db "    syscall", 10
	db "    leave", 10
	db "    ret", 10
	db "", 10
	db "; runtime: sys_exit(rdi=code) -> does not return", 10
	db "sys_exit:", 10
	db "    mov rax, 60", 10
	db "    syscall", 10
	db "", 10
	db "section .bss", 10
	db "heap_buf: resb HEAP_SIZE", 10
	db "heap_cur: resq 1", 10
	db "section .text", 10
	db "", 10
emit_start_len: equ $-emit_start
