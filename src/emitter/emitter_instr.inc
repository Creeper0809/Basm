; emitter_instr.inc
; - instruction emission helpers
; - small structured snippets (main prologue/epilogue)

section .text

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

; rdi=ident_ptr, rsi=ident_len
emit_global_dq0:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rdi
	mov r13, rsi

	call emit_data_begin_once

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_dq0_suffix]
	mov rdx, emit_dq0_suffix_len
	call emit_append

	pop r13
	pop r12
	pop rbp
	ret

; rdi=ident_ptr, rsi=ident_len, rdx=size_ptr, rcx=size_len
emit_global_resb:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	call emit_bss_begin_once

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_resb_mid]
	mov rdx, emit_resb_mid_len
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

; rdi=imm_ptr, rsi=imm_len
emit_sub_rsp_imm:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rdi
	mov r13, rsi

	lea rsi, [rel emit_sub_rsp_prefix]
	mov rdx, emit_sub_rsp_prefix_len
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

; rdi=label_ptr, rsi=label_len
emit_label:
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

; rdi=cc_ptr, rsi=cc_len, rdx=label_ptr, rcx=label_len
; emits: "    j" + cc + " " + label + "\n"
emit_jcc:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_j_prefix]
	mov rdx, emit_j_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_set_mid]
	mov rdx, emit_set_mid_len
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

; rdi=label_ptr, rsi=label_len
; emits: "    jmp " + label + "\n"
emit_jmp:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi

	lea rsi, [rel emit_jmp_prefix]
	mov rdx, emit_jmp_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	POP_R15_R12
	pop rbp
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

; rdi=dst_ptr, rsi=dst_len, rdx=src_ptr, rcx=src_len
; r8=op_ptr, r9=op_len (e.g. "    add ")
emit_binop_reg_src:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15
	push rbx

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx
	mov rbx, r8

	; op
	mov rsi, rbx
	mov rdx, r9
	call emit_append

	; dst
	mov rsi, r12
	mov rdx, r13
	call emit_append

	; ", "
	lea rsi, [rel emit_mov_mid]
	mov rdx, emit_mov_mid_len
	call emit_append

	; src
	mov rsi, r14
	mov rdx, r15
	call emit_append

	; "\n"
	lea rsi, [rel emit_nl]
	mov rdx, 1
	call emit_append

	pop rbx
	POP_R15_R12
	pop rbp
	ret

; wrappers: dst op reg
; rdi=dst_ptr,rsi=dst_len,rdx=src_ptr,rcx=src_len
emit_add_reg_reg:
	lea r8, [rel emit_add_prefix]
	mov r9, emit_add_prefix_len
	jmp emit_binop_reg_src

emit_sub_reg_reg:
	lea r8, [rel emit_sub_prefix]
	mov r9, emit_sub_prefix_len
	jmp emit_binop_reg_src

emit_imul_reg_reg:
	lea r8, [rel emit_imul_prefix]
	mov r9, emit_imul_prefix_len
	jmp emit_binop_reg_src

emit_and_reg_reg:
	lea r8, [rel emit_and_prefix]
	mov r9, emit_and_prefix_len
	jmp emit_binop_reg_src

emit_or_reg_reg:
	lea r8, [rel emit_or_prefix]
	mov r9, emit_or_prefix_len
	jmp emit_binop_reg_src

emit_xor_reg_reg:
	lea r8, [rel emit_xor_prefix]
	mov r9, emit_xor_prefix_len
	jmp emit_binop_reg_src

emit_shl_reg_imm:
	lea r8, [rel emit_shl_prefix]
	mov r9, emit_shl_prefix_len
	jmp emit_binop_reg_src

emit_shr_reg_imm:
	lea r8, [rel emit_shr_prefix]
	mov r9, emit_shr_prefix_len
	jmp emit_binop_reg_src

emit_cmp_reg_reg:
	lea r8, [rel emit_cmp_prefix]
	mov r9, emit_cmp_prefix_len
	jmp emit_binop_reg_src

; wrappers: dst op imm (imm is also passed as slice)
; reuse same helper
emit_add_reg_imm:
	lea r8, [rel emit_add_prefix]
	mov r9, emit_add_prefix_len
	jmp emit_binop_reg_src

emit_sub_reg_imm:
	lea r8, [rel emit_sub_prefix]
	mov r9, emit_sub_prefix_len
	jmp emit_binop_reg_src

emit_imul_reg_imm:
	lea r8, [rel emit_imul_prefix]
	mov r9, emit_imul_prefix_len
	jmp emit_binop_reg_src

emit_and_reg_imm:
	lea r8, [rel emit_and_prefix]
	mov r9, emit_and_prefix_len
	jmp emit_binop_reg_src

emit_or_reg_imm:
	lea r8, [rel emit_or_prefix]
	mov r9, emit_or_prefix_len
	jmp emit_binop_reg_src

emit_xor_reg_imm:
	lea r8, [rel emit_xor_prefix]
	mov r9, emit_xor_prefix_len
	jmp emit_binop_reg_src

emit_cmp_reg_imm:
	lea r8, [rel emit_cmp_prefix]
	mov r9, emit_cmp_prefix_len
	jmp emit_binop_reg_src

; rdi=cc_ptr, rsi=cc_len, rdx=reg8_ptr, rcx=reg8_len
; emits: "    set" + cc + " " + reg8 + "\n"
emit_setcc_reg8:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_set_prefix]
	mov rdx, emit_set_prefix_len
	call emit_append

	mov rsi, r12
	mov rdx, r13
	call emit_append

	lea rsi, [rel emit_set_mid]
	mov rdx, emit_set_mid_len
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
emit_load_ptr16:
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

	lea rsi, [rel emit_movzx_word_mid]
	mov rdx, emit_movzx_word_mid_len
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

; rdi=dst32_ptr, rsi=dst32_len, rdx=mem_ptr, rcx=mem_len
emit_load_ptr32:
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

	lea rsi, [rel emit_mov_dword_mid]
	mov rdx, emit_mov_dword_mid_len
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
emit_store_ptr16:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_store_word_prefix]
	mov rdx, emit_store_word_prefix_len
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
emit_store_ptr32:
	push rbp
	mov rbp, rsp
	PUSH_R12_R15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	lea rsi, [rel emit_store_dword_prefix]
	mov rdx, emit_store_dword_prefix_len
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

emit_movzx_word_mid: db ", word "
emit_movzx_word_mid_len: equ $-emit_movzx_word_mid

emit_mov_qword_mid: db ", qword "
emit_mov_qword_mid_len: equ $-emit_mov_qword_mid

emit_mov_dword_mid: db ", dword "
emit_mov_dword_mid_len: equ $-emit_mov_dword_mid

emit_store_byte_prefix: db "    mov byte "
emit_store_byte_prefix_len: equ $-emit_store_byte_prefix
emit_store_qword_prefix: db "    mov qword "
emit_store_qword_prefix_len: equ $-emit_store_qword_prefix

emit_store_word_prefix: db "    mov word "
emit_store_word_prefix_len: equ $-emit_store_word_prefix

emit_store_dword_prefix: db "    mov dword "
emit_store_dword_prefix_len: equ $-emit_store_dword_prefix

emit_dq0_suffix: db ": dq 0",10
emit_dq0_suffix_len: equ $-emit_dq0_suffix

emit_resb_mid: db ": resb "
emit_resb_mid_len: equ $-emit_resb_mid

emit_sub_rsp_prefix: db "    sub rsp, "
emit_sub_rsp_prefix_len: equ $-emit_sub_rsp_prefix

emit_add_prefix:  db "    add "
emit_add_prefix_len: equ $-emit_add_prefix
emit_sub_prefix:  db "    sub "
emit_sub_prefix_len: equ $-emit_sub_prefix
emit_imul_prefix: db "    imul "
emit_imul_prefix_len: equ $-emit_imul_prefix
emit_and_prefix:  db "    and "
emit_and_prefix_len: equ $-emit_and_prefix
emit_or_prefix:   db "    or "
emit_or_prefix_len: equ $-emit_or_prefix
emit_xor_prefix:  db "    xor "
emit_xor_prefix_len: equ $-emit_xor_prefix
emit_shl_prefix:  db "    shl "
emit_shl_prefix_len: equ $-emit_shl_prefix
emit_shr_prefix:  db "    shr "
emit_shr_prefix_len: equ $-emit_shr_prefix
emit_cmp_prefix:  db "    cmp "
emit_cmp_prefix_len: equ $-emit_cmp_prefix

emit_set_prefix: db "    set"
emit_set_prefix_len: equ $-emit_set_prefix
emit_set_mid: db " "
emit_set_mid_len: equ $-emit_set_mid

emit_j_prefix: db "    j"
emit_j_prefix_len: equ $-emit_j_prefix

emit_jmp_prefix: db "    jmp "
emit_jmp_prefix_len: equ $-emit_jmp_prefix

emit_call_prefix: db "    call "
emit_call_prefix_len: equ $-emit_call_prefix

emit_lea_prefix: db "    lea "
emit_lea_prefix_len: equ $-emit_lea_prefix
emit_lea_mid: db ", [rel "
emit_lea_mid_len: equ $-emit_lea_mid
emit_lea_end: db "]",10
emit_lea_end_len: equ $-emit_lea_end
