; parser_sym.inc
; - Unified symbol table schema + linear find/add

; -----------------------------------------------------------------------------
; Unified symbol table schema (bootstrapping-friendly)
; Entry layout (32 bytes, 4 qwords):
;   +0  name_ptr
;   +8  name_len
;  +16  kind
;  +24  payload
;
; Kinds (enum)
%define SYM_NONE       0
%define SYM_VAR_LOCAL  1
%define SYM_VAR_GLOBAL 2
%define SYM_ALIAS      3
%define SYM_CONST      4
%define SYM_LABEL      5

%define SYM_ENTRY_QWORDS 4
%define SYM_ENTRY_BYTES  32

section .text

; rdi=name_ptr, rsi=name_len
; returns rax=entry_ptr if found else 0
parser_sym_find:
	push rbp
	mov rbp, rsp
	push r12
	push r13
	push r14
	push r15

	mov r14, rdi
	mov r15, rsi

	mov r12, [parser_sym_count]
	test r12, r12
	jz .not_found

	dec r12
.loop:
	; load entry name_len
	lea rbx, [rel parser_sym_table]
	mov rax, r12
	shl rax, 5
	add rbx, rax
	mov r13, [rbx + 8]
	cmp r13, r15
	jne .next

	mov r8, [rbx + 0]
	xor rcx, rcx
.cmp_loop:
	cmp rcx, r15
	je .match
	mov al, [r8 + rcx]
	mov dl, [r14 + rcx]
	cmp al, dl
	jne .next
	inc rcx
	jmp .cmp_loop

.match:
	mov rax, rbx
	jmp .done

.next:
	test r12, r12
	jz .not_found
	dec r12
	jmp .loop

.not_found:
	xor rax, rax
.done:
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	ret

; rdi=name_ptr, rsi=name_len, rdx=kind, rcx=payload
; returns rax=index
parser_sym_add:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, [parser_sym_count]
	cmp r12, 256
	jb .ok
	lea rdi, [rel parser_msg_too_many_locals]
	mov rsi, [lexer_tok_line]
	call util_fatal_line
.ok:

	lea r13, [rel parser_sym_table]
	mov rax, r12
	shl rax, 5
	add r13, rax
	mov [r13 + 0], rdi
	mov [r13 + 8], rsi
	mov [r13 + 16], rdx
	mov [r13 + 24], rcx

	inc r12
	mov [parser_sym_count], r12
	dec r12
	mov rax, r12

	pop r13
	pop r12
	pop rbp
	ret

section .bss
; number of global (cross-function) entries at bottom of sym_table
parser_sym_global_count: resq 1
parser_sym_count:   resq 1
parser_sym_table:   resq (SYM_ENTRY_QWORDS * 256)
