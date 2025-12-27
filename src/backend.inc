section .bss
backend_num_buf: resb 32

section .text

backend_emit_program:
	push rbp
	mov rbp, rsp
	push r12

	mov r12, rdi

	; header
	mov rdi, r12
	lea rsi, [rel backend_hdr]
	mov rdx, backend_hdr_len
	call util_write_all

	; if eax imm exists, emit mov eax, <imm>
	cmp byte [ir_has_eax_imm], 0
	je .after_mov

	; prefix
	mov rdi, r12
	lea rsi, [rel backend_mov_prefix]
	mov rdx, backend_mov_prefix_len
	call util_write_all

	; convert imm to decimal
	mov rdi, [ir_eax_imm_u64]
	lea rsi, [rel backend_num_buf + 32]
	call util_u64_to_dec

	; write digits
	mov rdi, r12
	mov rsi, rax
	; rdx already length
	call util_write_all

	; newline
	mov rdi, r12
	lea rsi, [rel backend_nl]
	mov rdx, 1
	call util_write_all

.after_mov:
	; optional print(eax)
	cmp byte [ir_do_print_eax], 0
	je .exit_only

	mov rdi, r12
	lea rsi, [rel backend_print_block]
	mov rdx, backend_print_block_len
	call util_write_all
	jmp .done

.exit_only:
	mov rdi, r12
	lea rsi, [rel backend_exit_block]
	mov rdx, backend_exit_block_len
	call util_write_all

.done:
	pop r12
	pop rbp
	ret

section .rodata
backend_hdr:
	db "BITS 64", 10
	db "DEFAULT REL", 10
	db "global _start", 10
	db "section .text", 10
	db "_start:", 10
backend_hdr_len: equ $-backend_hdr

backend_mov_prefix: db "    mov eax, "
backend_mov_prefix_len: equ $-backend_mov_prefix

backend_nl: db 10

backend_exit_block:
	db "    mov rax, 60", 10
	db "    xor rdi, rdi", 10
	db "    syscall", 10
backend_exit_block_len: equ $-backend_exit_block

backend_print_block:
	db "    ; print eax as unsigned decimal + newline", 10
	db "    lea rsi, [rel num_buf + 31]", 10
	db "    mov byte [rsi], 10", 10
	db "    mov rcx, 1", 10
	db "  .print_loop:", 10
	db "    xor rdx, rdx", 10
	db "    mov rbx, 10", 10
	db "    div rbx", 10
	db "    add dl, '0'", 10
	db "    dec rsi", 10
	db "    mov [rsi], dl", 10
	db "    inc rcx", 10
	db "    test rax, rax", 10
	db "    jnz .print_loop", 10
	db "    mov rax, 1", 10
	db "    mov rdi, 1", 10
	db "    mov rdx, rcx", 10
	db "    syscall", 10
	db "    mov rax, 60", 10
	db "    xor rdi, rdi", 10
	db "    syscall", 10
	db "section .bss", 10
	db "num_buf: resb 32", 10
backend_print_block_len: equ $-backend_print_block
