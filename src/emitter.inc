%ifndef EMIT_BUF_MAX
%define EMIT_BUF_MAX 1048576
%endif

; emitter.inc (Core & I/O)
; - output buffering (emit_buf/emit_append/emit_flush)
; - section management helpers
; - includes instruction emitters + runtime blob

section .bss
emit_buf:      resb EMIT_BUF_MAX
emit_cur:      resq 1
emit_data_emitted: resq 1
emit_bss_emitted:  resq 1

section .text

; resets output buffer and emits the standard header + _start shim
emit_reset:
	lea rax, [rel emit_buf]
	mov [emit_cur], rax
	mov qword [emit_data_emitted], 0
	mov qword [emit_bss_emitted], 0

	; header (includes ABI note for print_dec)
	lea rsi, [rel emit_hdr]
	mov rdx, emit_hdr_len
	call emit_append

	; _start shim emitted later (after global .data)
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

emit_bss_begin_once:
	mov rax, [emit_bss_emitted]
	test rax, rax
	jnz .emit
	mov qword [emit_bss_emitted], 1
.emit:
	lea rsi, [rel emit_bss_section]
	mov rdx, emit_bss_section_len
	call emit_append
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

; rdi=str_ptr, rsi=str_len
; Raw string pass-through helper (semantic alias of emit_append)
emit_raw:
	mov rdx, rsi
	mov rsi, rdi
	jmp emit_append

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

%include "src/emitter/emitter_instr.inc"
%include "src/emitter/emitter_runtime.inc"

section .rodata
emit_data_section: db "section .data",10
emit_data_section_len: equ $-emit_data_section

emit_text_section_txt: db "section .text",10
emit_text_section_txt_len: equ $-emit_text_section_txt

emit_bss_section: db "section .bss",10
emit_bss_section_len: equ $-emit_bss_section

emit_msg_overflow: db "error: emitter output buffer overflow", 10, 0
