; Stage 1 emitter (no IR)
;
; Owns an in-memory output buffer so we can flush the exact same
; generated NASM text to stdout and optional -o file.

%ifndef EMIT_BUF_MAX
%define EMIT_BUF_MAX 1048576
%endif

section .bss
emit_buf:      resb EMIT_BUF_MAX
emit_cur:      resq 1
emit_data_emitted: resq 1

section .text

; resets output buffer and emits the standard header + _start shim
emit_reset:
	lea rax, [rel emit_buf]
	mov [emit_cur], rax
	mov qword [emit_data_emitted], 0

	; header (includes ABI note for print_dec)
	lea rsi, [rel emit_hdr]
	mov rdx, emit_hdr_len
	call emit_append

	; _start shim emitted later (after global .data)
	ret

emit_start_shim:
	lea rsi, [rel emit_start]
	mov rdx, emit_start_len
	call emit_append
	ret

emit_data_begin_once:
	mov rax, [emit_data_emitted]
	test rax, rax
	jnz .emit
	mov qword [emit_data_emitted], 1
.emit:
	lea rsi, [rel emit_data_section]
	mov rdx, emit_data_section_len
	call emit_append
	ret

emit_text_section:
	lea rsi, [rel emit_text_section_txt]
	mov rdx, emit_text_section_txt_len
	call emit_append
	ret

; rdi=name_ptr, rsi=name_len
emit_global_dq0:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rdi
	mov r13, rsi

	call emit_data_begin_once

	; <name>
	mov rsi, r12
	mov rdx, r13
	call emit_append

	; ": dq 0\n"
	lea rsi, [rel emit_dq0_suffix]
	mov rdx, emit_dq0_suffix_len
	call emit_append

	pop r13
	pop r12
	pop rbp
	ret

; rsi = ptr, rdx = len
emit_append:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rsi
	mov r13, rdx

	mov rax, [emit_cur]
	lea rcx, [rel emit_buf + EMIT_BUF_MAX]
	lea r8, [rax + r13]
	cmp r8, rcx
	ja .overflow

	; copy bytes
	mov rdi, rax
	mov rsi, r12
	mov rcx, r13
	rep movsb
	mov [emit_cur], rdi

	pop r13
	pop r12
	pop rbp
	ret

.overflow:
	lea rdi, [rel emit_msg_overflow]
	call util_fatal

; rdi = fd
emit_flush:
	push rbp
	mov rbp, rsp
	push r12

	mov r12, rdi

	lea rsi, [rel emit_buf]
	mov rdx, [emit_cur]
	sub rdx, rsi

	mov rdi, r12
	call util_write_all

	pop r12
	pop rbp
	ret

; convenience helpers used by the parser
; rdi=name_ptr, rsi=name_len
emit_func_label:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rdi
	mov r13, rsi

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_label_suffix]
	mov rdx, emit_label_suffix_len
	call emit_append

	pop r13
	pop r12
	pop rbp
	ret

emit_main_label:
	lea rsi, [rel emit_main_label_txt]
	mov rdx, emit_main_label_txt_len
	call emit_append
	ret

emit_main_prologue:
	lea rsi, [rel emit_main_prologue_txt]
	mov rdx, emit_main_prologue_txt_len
	call emit_append
	ret

emit_push0:
	lea rsi, [rel emit_push0_txt]
	mov rdx, emit_push0_txt_len
	call emit_append
	ret

emit_align_pad:
	lea rsi, [rel emit_align_pad_txt]
	mov rdx, emit_align_pad_txt_len
	call emit_append
	ret

emit_main_epilogue:
	lea rsi, [rel emit_main_epilogue_txt]
	mov rdx, emit_main_epilogue_txt_len
	call emit_append
	ret

; rdi=reg_ptr, rsi=reg_len, rdx=imm_ptr, rcx=imm_len
emit_mov_reg_imm:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_mov_prefix]
	mov rdx, emit_mov_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_mov_mid]
	mov rdx, emit_mov_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
	ret

; rdi=dst_ptr, rsi=dst_len, rdx=src_ptr, rcx=src_len
emit_mov_reg_reg:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_mov_prefix]
	mov rdx, emit_mov_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_mov_mid]
	mov rdx, emit_mov_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
	ret

; rdi=dst_ptr, rsi=dst_len, rdx=mem_ptr, rcx=mem_len
emit_load_ptr8:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_movzx_prefix]
	mov rdx, emit_movzx_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_movzx_mid]
	mov rdx, emit_movzx_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
	ret

; rdi=dst_ptr, rsi=dst_len, rdx=mem_ptr, rcx=mem_len
emit_load_ptr64:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_mov_prefix]
	mov rdx, emit_mov_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_mov_qword_mid]
	mov rdx, emit_mov_qword_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
	ret

; rdi=mem_ptr, rsi=mem_len, rdx=src_ptr, rcx=src_len
emit_store_ptr8:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_store_byte_prefix]
	mov rdx, emit_store_byte_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_mov_mid]
	mov rdx, emit_mov_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
	ret

; rdi=mem_ptr, rsi=mem_len, rdx=src_ptr, rcx=src_len
emit_store_ptr64:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_store_qword_prefix]
	mov rdx, emit_store_qword_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_mov_mid]
	mov rdx, emit_mov_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
	ret

; rdi=name_ptr, rsi=name_len
emit_call_ident:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rdi
	mov r13, rsi

	lea rsi, [rel emit_call_prefix]
	mov rdx, emit_call_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	pop r13
	pop r12
	pop rbp
	ret

; rdi=dst_ptr, rsi=dst_len, rdx=label_ptr, rcx=label_len
emit_lea_reg_label:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_lea_prefix]
	mov rdx, emit_lea_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_lea_mid]
	mov rdx, emit_lea_mid_len
	call emit_append

	mov rsi, r14
	mov rdx, r15
	call emit_append

	lea rsi, [rel emit_lea_end]
	mov rdx, emit_lea_end_len
	call emit_append

	POP_R15_R12
	pop rbp
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

emit_main_label_txt:
	db "main:", 10
emit_main_label_txt_len: equ $-emit_main_label_txt

emit_label_suffix:
	db ":", 10
emit_label_suffix_len: equ $-emit_label_suffix

emit_main_prologue_txt:
	db "    push rbp", 10
	db "    mov rbp, rsp", 10
emit_main_prologue_txt_len: equ $-emit_main_prologue_txt

emit_push0_txt:
	db "    push 0", 10
emit_push0_txt_len: equ $-emit_push0_txt

emit_align_pad_txt:
	db "    push 0 ; align padding", 10
emit_align_pad_txt_len: equ $-emit_align_pad_txt

emit_main_epilogue_txt:
	db "    mov rsp, rbp", 10
	db "    pop rbp", 10
	db "    ret", 10
emit_main_epilogue_txt_len: equ $-emit_main_epilogue_txt

emit_mov_prefix: db "    mov "
emit_mov_prefix_len: equ $-emit_mov_prefix
emit_mov_mid: db ", "
emit_mov_mid_len: equ $-emit_mov_mid
emit_nl: db 10

emit_movzx_prefix: db "    movzx "
emit_movzx_prefix_len: equ $-emit_movzx_prefix
emit_movzx_mid: db ", byte "
emit_movzx_mid_len: equ $-emit_movzx_mid

emit_mov_qword_mid: db ", qword "
emit_mov_qword_mid_len: equ $-emit_mov_qword_mid

emit_store_byte_prefix: db "    mov byte "
emit_store_byte_prefix_len: equ $-emit_store_byte_prefix
emit_store_qword_prefix: db "    mov qword "
emit_store_qword_prefix_len: equ $-emit_store_qword_prefix

emit_data_section: db "section .data",10
emit_data_section_len: equ $-emit_data_section

emit_text_section_txt: db "section .text",10
emit_text_section_txt_len: equ $-emit_text_section_txt
emit_dq0_suffix: db ": dq 0",10
emit_dq0_suffix_len: equ $-emit_dq0_suffix

emit_call_prefix: db "    call "
emit_call_prefix_len: equ $-emit_call_prefix

emit_lea_prefix: db "    lea "
emit_lea_prefix_len: equ $-emit_lea_prefix
emit_lea_mid: db ", [rel "
emit_lea_mid_len: equ $-emit_lea_mid
emit_lea_end: db "]",10
emit_lea_end_len: equ $-emit_lea_end

emit_msg_overflow: db "error: emitter output buffer overflow", 10, 0
