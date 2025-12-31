; parser_util.inc
; - Low-level helpers: bytes copy, string interning, addr parsing, reg name helpers

section .text

; rdi=ident_ptr, rsi=ident_len
; If ident is a SYM_ALIAS, returns rax=reg_ptr, rdx=reg_len, r8=TOK_REG.
; If ident is a SYM_CONST, returns rax=imm_ptr, rdx=imm_len, r8=TOK_INT.
; Otherwise returns rax=0, rdx=0, r8=0.
; NOTE: For SYM_CONST, the returned slice points into parser_num_buf (temporary).
parser_ident_to_reg_or_imm:
	push rbp
	mov rbp, rsp
	push rbx
	push r12

	call parser_sym_find
	test rax, rax
	jz .no

	mov rbx, [rax + 16]          ; kind
	cmp rbx, SYM_ALIAS
	je .alias
	cmp rbx, SYM_CONST
	je .const
	jmp .no

.alias:
	mov r12, [rax + 24]          ; payload = reg_id
	mov rdi, r12
	call parser_reg64_from_id
	mov r8, TOK_REG
	jmp .done

.const:
	mov r12, [rax + 24]          ; payload = u64
	mov rdi, r12
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	mov r8, TOK_INT
	jmp .done

.no:
	xor rax, rax
	xor rdx, rdx
	xor r8,  r8

.done:
	pop r12
	pop rbx
	pop rbp
	ret

; rdi=ident_ptr, rsi=ident_len
; If ident is a SYM_ALIAS, returns rax=reg_ptr, rdx=reg_len.
; Otherwise returns rax=0, rdx=0.
parser_alias_to_reg_slice:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13

	call parser_sym_find
	test rax, rax
	jz .no

	mov rbx, [rax + 16]          ; kind
	cmp rbx, SYM_ALIAS
	jne .no

	mov r12, [rax + 24]          ; payload = reg_id
	mov rdi, r12
	call parser_reg64_from_id
	jmp .done

.no:
	xor rax, rax
	xor rdx, rdx
.done:
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

; rcx = bytes to reserve
; returns rax = pointer into parser_namepool
parser_namepool_alloc:
	push rbx
	mov rbx, [parser_namepool_idx]
	mov rax, rbx
	add rax, rcx
	cmp rax, parser_namepool_cap
	ja parser_err_expected_stmt
	lea rax, [rel parser_namepool]
	add rax, rbx
	add rbx, rcx
	mov [parser_namepool_idx], rbx
	pop rbx
	ret

; rdi=layout_ptr, rsi=layout_len, rdx=member_ptr, rcx=member_len
; returns rax=name_ptr, rdx=name_len
parser_make_layout_member_name:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13

	mov r12, rdi                 ; layout_ptr
	mov r13, rsi                 ; layout_len
	mov rbx, rcx                 ; member_len

	; total = layout_len + 1 + member_len
	mov rcx, r13
	inc rcx
	add rcx, rbx
	call parser_namepool_alloc

	; copy layout
	mov rdi, rax
	mov rsi, r12
	mov rcx, r13
	call parser_copy_bytes

	; '.'
	mov byte [rdi], '.'
	inc rdi

	; copy member
	mov rsi, rdx
	mov rcx, rbx
	call parser_copy_bytes

	; return ptr/len (rax already start)
	mov rdx, r13
	inc rdx
	add rdx, rbx

	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

; rdi=layout_ptr, rsi=layout_len
; returns rax=name_ptr, rdx=name_len
parser_make_layout_size_name:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13

	mov r12, rdi                 ; layout_ptr
	mov r13, rsi                 ; layout_len

	; total = layout_len + suffix_len
	mov rcx, r13
	add rcx, parser_layout_size_suffix_len
	call parser_namepool_alloc

	; copy layout
	mov rdi, rax
	mov rsi, r12
	mov rcx, r13
	call parser_copy_bytes

	; copy suffix
	lea rsi, [rel parser_layout_size_suffix]
	mov rcx, parser_layout_size_suffix_len
	call parser_copy_bytes

	; rax already start
	mov rdx, r13
	add rdx, parser_layout_size_suffix_len

	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

; rdi=reg_ptr, rsi=reg_len
; returns rax=reg_id (0..15)
parser_reg64_id_from_slice:
	; len2: r8/r9
	mov rbx, rdi
	mov rcx, rsi
	cmp rcx, 2
	je .len2
	cmp rcx, 3
	je .len3
	jmp parser_err_expected_stmt

.len2:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], '8'
	je .id8
	cmp byte [rbx + 1], '9'
	je .id9
	jmp parser_err_expected_stmt
.id8:
	mov rax, 8
	ret
.id9:
	mov rax, 9
	ret

.len3:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], 'a'
	je .rax
	cmp byte [rbx + 1], 'b'
	je .rbx_or_rbp
	cmp byte [rbx + 1], 'c'
	je .rcx
	cmp byte [rbx + 1], 'd'
	je .rdx_or_rdi
	cmp byte [rbx + 1], 's'
	je .rsi_or_rsp
	cmp byte [rbx + 1], '1'
	je .r10_15
	jmp parser_err_expected_stmt

.rax:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	xor rax, rax                 ; 0
	ret

.rbx_or_rbp:
	cmp byte [rbx + 2], 'x'
	je .rbx
	cmp byte [rbx + 2], 'p'
	je .rbp
	jmp parser_err_expected_stmt
.rbx:
	mov rax, 1
	ret
.rbp:
	mov rax, 7
	ret

.rcx:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	mov rax, 2
	ret

.rdx_or_rdi:
	cmp byte [rbx + 2], 'x'
	je .rdx
	cmp byte [rbx + 2], 'i'
	je .rdi
	jmp parser_err_expected_stmt
.rdx:
	mov rax, 3
	ret
.rdi:
	mov rax, 5
	ret

.rsi_or_rsp:
	cmp byte [rbx + 2], 'i'
	je .rsi
	cmp byte [rbx + 2], 'p'
	je .rsp
	jmp parser_err_expected_stmt
.rsi:
	mov rax, 4
	ret
.rsp:
	mov rax, 6
	ret

.r10_15:
	mov al, [rbx + 2]
	cmp al, '0'
	je .r10
	cmp al, '1'
	je .r11
	cmp al, '2'
	je .r12
	cmp al, '3'
	je .r13
	cmp al, '4'
	je .r14
	cmp al, '5'
	je .r15
	jmp parser_err_expected_stmt
.r10:
	mov rax, 10
	ret
.r11:
	mov rax, 11
	ret
.r12:
	mov rax, 12
	ret
.r13:
	mov rax, 13
	ret
.r14:
	mov rax, 14
	ret
.r15:
	mov rax, 15
	ret

; rdi=reg_id
; returns rax=reg_ptr, rdx=reg_len
parser_reg64_from_id:
	cmp rdi, 15
	jbe .ok
	jmp parser_err_expected_stmt
.ok:
	lea r8, [rel parser_reg64_ptrs]
	mov rax, [r8 + rdi*8]
	lea r9, [rel parser_reg64_lens]
	mov rdx, [r9 + rdi*8]
	ret

; copies rcx bytes from rsi to rdi, returns rdi advanced
parser_copy_bytes:
	push rax
	test rcx, rcx
	jz .done
.l:
	mov al, [rsi]
	mov [rdi], al
	inc rsi
	inc rdi
	dec rcx
	jnz .l

.done:
	pop rax
	ret

; rdi=raw_ptr, rsi=raw_len (may contain escapes)
; returns rax=label_ptr, rdx=label_len
parser_intern_string:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13
	push r14
	push r15

	mov r14, rdi
	mov r15, rsi

	; id = ++counter
	mov r12, [parser_str_count]
	inc r12
	mov [parser_str_count], r12

	; build label "str<id>" into parser_str_label_buf
	lea rdi, [rel parser_str_label_buf]
	mov byte [rdi + 0], 's'
	mov byte [rdi + 1], 't'
	mov byte [rdi + 2], 'r'

	mov rdi, r12
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	mov rsi, rax
	lea rdi, [rel parser_str_label_buf + 3]
	mov rcx, rdx
	call parser_copy_bytes

	mov r13, 3
	add r13, rdx                  ; label_len

	; build end label into parser_str_end_label_buf: "str<id>_end"
	lea rdi, [rel parser_str_end_label_buf]
	lea rsi, [rel parser_str_label_buf]
	mov rcx, r13
	call parser_copy_bytes
	lea rdi, [rel parser_str_end_label_buf]
	add rdi, r13
	lea rsi, [rel parser_str_end_suffix]
	mov rcx, parser_str_end_suffix_len
	call parser_copy_bytes
	mov rbx, r13
	add rbx, parser_str_end_suffix_len

	; emit into .data:
	;   <label>: db <bytes>, 0\n
	;   <end>:\n
	call emit_data_begin_once
	; <label>
	lea rsi, [rel parser_str_label_buf]
	mov rdx, r13
	call emit_append
	; ": db "
	lea rsi, [rel parser_str_db_suffix]
	mov rdx, parser_str_db_suffix_len
	call emit_append

	; write bytes
	xor r12d, r12d                ; out_count
	xor r11d, r11d                ; raw_index
.str_loop:
	cmp r11, r15
	jae .str_done
	mov al, [r14 + r11]
	cmp al, 92
	je .esc
	; normal byte
	movzx rdi, al
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	mov rsi, rax
	call emit_append
	add r11, 1
	inc r12
	jmp .maybe_comma

.esc:
	; escape sequence
	cmp r11, r15
	jae parser_err_expected_stmt
	mov al, [r14 + r11 + 1]
	cmp al, 'n'
	je .esc_n
	cmp al, 't'
	je .esc_t
	cmp al, '"'
	je .esc_q
	cmp al, 92
	je .esc_bs
	jmp parser_err_expected_stmt
.esc_n:
	mov rdi, 10
	jmp .emit_esc
.esc_t:
	mov rdi, 9
	jmp .emit_esc
.esc_q:
	mov rdi, 34
	jmp .emit_esc
.esc_bs:
	mov rdi, 92
.emit_esc:
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec
	mov rsi, rax
	call emit_append
	add r11, 2
	inc r12

.maybe_comma:
	; comma between bytes
	cmp r11, r15
	jae .str_loop
	lea rsi, [rel parser_str_comma]
	mov rdx, parser_str_comma_len
	call emit_append
	jmp .str_loop

.str_done:
	; add terminator
	cmp r12, 0
	je .term_only
	lea rsi, [rel parser_str_comma]
	mov rdx, parser_str_comma_len
	call emit_append
.term_only:
	lea rsi, [rel parser_str_zero]
	mov rdx, parser_str_zero_len
	call emit_append
	lea rsi, [rel parser_str_nl]
	mov rdx, 1
	call emit_append

	; end label
	lea rsi, [rel parser_str_end_label_buf]
	mov rdx, rbx
	call emit_append
	lea rsi, [rel parser_str_colon_nl]
	mov rdx, parser_str_colon_nl_len
	call emit_append

	; resume emitting code in .text
	call emit_text_section

	; return label
	lea rax, [rel parser_str_label_buf]
	mov rdx, r13

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

; Parses an address expression and builds a bracketed mem operand into parser_mem_buf.
; addr := ident | reg | reg (+|-) int
; Expects the opening '[' already consumed.
; returns rax=mem_ptr, rdx=mem_len
parser_parse_mem_operand_after_lbrack:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13
	push r14
	push r15

	call lexer_peek
	cmp rax, TOK_IDENT
	je .base_ident
	cmp rax, TOK_REG
	je .base_reg
	jmp parser_err_expected_addr

.base_ident:
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	; If ident is an alias, treat it as a register base.
	mov rdi, r14
	mov rsi, r15
	call parser_alias_to_reg_slice
	test rax, rax
	jz .base_ident_real
	mov r14, rax
	mov r15, rdx
	call lexer_next
	jmp .base_reg_after_consumed
.base_ident_real:
	call lexer_next

	; optional suffix for ident base (plus-only):
	;   ident
	;   ident + reg
	;   ident + int
	;   ident + const
	;
	; Encode state in r12d:
	;   bit0: has_index
	;   bit1: has_disp
	;   bit2: disp_is_value
	xor r12d, r12d

	call lexer_peek
	cmp rax, TOK_PLUS
	jne .after_ident_suffix
	call lexer_next
	call lexer_peek
	cmp rax, TOK_REG
	je .ident_take_index_reg
	cmp rax, TOK_INT
	je .ident_take_disp_int
	cmp rax, TOK_IDENT
	je .ident_take_ident
	jmp parser_err_expected_addr

.ident_take_index_reg:
	mov r13, [lexer_tok_ptr]
	mov r11, [lexer_tok_len]
	push r13
	push r11
	call lexer_next
	or r12d, 1
	jmp .after_ident_suffix

.ident_take_disp_int:
	mov r13, [lexer_tok_ptr]
	mov r11, [lexer_tok_len]
	call lexer_next
	or r12d, 2
	jmp .after_ident_suffix

.ident_take_ident:
	; prefer alias->reg (index); otherwise const->disp value
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_alias_to_reg_slice
	test rax, rax
	jz .ident_take_ident_const
	; alias -> reg index
	push rax
	push rdx
	call lexer_next
	or r12d, 1
	jmp .after_ident_suffix

.ident_take_ident_const:
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_sym_find
	test rax, rax
	jz parser_err_expected_addr
	mov rcx, [rax + 16]
	cmp rcx, SYM_CONST
	jne parser_err_expected_addr
	mov r13, [rax + 24]              ; disp_value (u64)
	call lexer_next
	or r12d, 2
	or r12d, 4
	jmp .after_ident_suffix

.after_ident_suffix:
	; '-' is still not allowed for ident base
	call lexer_peek
	cmp rax, TOK_MINUS
	jne .after_ident_suffix_checks
	test r12d, 1
	jz parser_err_expected_addr
	add rsp, 16
	jmp parser_err_expected_addr
.after_ident_suffix_checks:
	; disallow a second '+' (only one suffix)
	call lexer_peek
	cmp rax, TOK_PLUS
	jne .after_ident_kind
	test r12d, 1
	jz parser_err_expected_addr
	add rsp, 16
	jmp parser_err_expected_addr

.after_ident_kind:
	mov rdi, r14
	mov rsi, r15
	call parser_sym_find
	test rax, rax
	jz .global_ident
	mov rcx, [rax + 16]
	cmp rcx, SYM_VAR_LOCAL
	jne .global_ident

	; local: payload is signed offset (usually negative): [rbp-<dec>]
	mov rbx, [rax + 24]
	lea rdi, [rel parser_mem_buf]
	lea rsi, [rel parser_mem_local_prefix]
	mov rcx, parser_mem_local_prefix_len
	call parser_copy_bytes

	; emit sign and absolute value
	test rbx, rbx
	js .local_neg
	mov byte [rdi], '+'
	inc rdi
	jmp .local_abs
.local_neg:
	mov byte [rdi], '-'
	inc rdi
	neg rbx
.local_abs:

	push rdi
	mov rdi, rbx
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	pop rdi
	mov rsi, rax
	mov rcx, rdx
	call parser_copy_bytes

	test r12d, 1
	jz .local_after_index
	mov byte [rdi], '+'
	inc rdi
	pop rcx                        ; index_len
	pop rsi                        ; index_ptr
	call parser_copy_bytes
.local_after_index:
	test r12d, 2
	jz .local_close
	mov byte [rdi], '+'
	inc rdi
	test r12d, 4
	jz .local_disp_copy_slice
	; disp is u64 value in r13
	push rdi
	mov rdi, r13
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec
	pop rdi
	mov rsi, rax
	mov rcx, rdx
	call parser_copy_bytes
	jmp .local_close
.local_disp_copy_slice:
	mov rsi, r13
	mov rcx, r11
	call parser_copy_bytes
.local_close:
	mov byte [rdi], ']'
	inc rdi

	lea rax, [rel parser_mem_buf]
	mov rdx, rdi
	sub rdx, rax
	jmp .done

.global_ident:
	; global: [rel <name>]
	lea rdi, [rel parser_mem_buf]
	lea rsi, [rel parser_mem_global_prefix]
	mov rcx, parser_mem_global_prefix_len
	call parser_copy_bytes

	mov rsi, r14
	mov rcx, r15
	call parser_copy_bytes

	test r12d, 1
	jz .global_after_index
	mov byte [rdi], '+'
	inc rdi
	pop rcx                        ; index_len
	pop rsi                        ; index_ptr
	call parser_copy_bytes
.global_after_index:
	test r12d, 2
	jz .global_close
	mov byte [rdi], '+'
	inc rdi
	test r12d, 4
	jz .global_disp_copy_slice
	; disp is u64 value in r13
	push rdi
	mov rdi, r13
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec
	pop rdi
	mov rsi, rax
	mov rcx, rdx
	call parser_copy_bytes
	jmp .global_close
.global_disp_copy_slice:
	mov rsi, r13
	mov rcx, r11
	call parser_copy_bytes
.global_close:
	mov byte [rdi], ']'
	inc rdi

	lea rax, [rel parser_mem_buf]
	mov rdx, rdi
	sub rdx, rax
	jmp .done

.base_reg:
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next

.base_reg_after_consumed:

	xor r12d, r12d               ; sign: 0 none, 1 '+', 2 '-'
	call lexer_peek
	cmp rax, TOK_PLUS
	je .take_plus
	cmp rax, TOK_MINUS
	je .take_minus
	jmp .build_reg
.take_plus:
	mov r12, 1
	call lexer_next
	jmp .need_imm
.take_minus:
	mov r12, 2
	call lexer_next
.need_imm:
	call lexer_peek
	cmp rax, TOK_INT
	je .need_imm_take
	cmp rax, TOK_IDENT
	jne parser_err_expected_addr
	; ident is only allowed here if it's a const -> imm
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_ident_to_reg_or_imm
	cmp r8, TOK_INT
	jne parser_err_expected_addr
	mov r13, rax
	mov rbx, rdx
	call lexer_next
	jmp .build_reg
.need_imm_take:
	mov r13, [lexer_tok_ptr]
	mov rbx, [lexer_tok_len]
	call lexer_next

.build_reg:
	lea rdi, [rel parser_mem_buf]
	mov byte [rdi], '['
	inc rdi
	mov rsi, r14
	mov rcx, r15
	call parser_copy_bytes

	cmp r12, 0
	je .finish_reg
	cmp r12, 1
	jne .put_minus
	mov byte [rdi], '+'
	jmp .after_sign
.put_minus:
	mov byte [rdi], '-'
.after_sign:
	inc rdi
	mov rsi, r13
	mov rcx, rbx
	call parser_copy_bytes

.finish_reg:
	mov byte [rdi], ']'
	inc rdi
	lea rax, [rel parser_mem_buf]
	mov rdx, rdi
	sub rdx, rax

.done:
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

; rdi=reg_ptr, rsi=reg_len
; returns rax=reg8_ptr, rdx=reg8_len
parser_reg_to_reg8:
	; len2: r8/r9 -> r8b/r9b
	mov rbx, rdi
	mov rcx, rsi
	cmp rcx, 2
	je .len2
	cmp rcx, 3
	je .len3
	jmp parser_err_expected_stmt

.len2:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], '8'
	je .r8b
	cmp byte [rbx + 1], '9'
	je .r9b
	jmp parser_err_expected_stmt
.r8b:
	lea rax, [rel parser_reg8_r8b]
	mov rdx, 3
	ret

.r9b:
	lea rax, [rel parser_reg8_r9b]
	mov rdx, 3
	ret

.len3:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	; rax/rbx/rcx/rdx
	cmp byte [rbx + 1], 'a'
	je .al
	cmp byte [rbx + 1], 'b'
	je .bl_or_bpl
	cmp byte [rbx + 1], 'c'
	je .cl
	cmp byte [rbx + 1], 'd'
	je .dl_or_dil
	cmp byte [rbx + 1], 's'
	je .sil_or_spl
	; r10..r15
	cmp byte [rbx + 1], '1'
	jne parser_err_expected_stmt
	mov al, [rbx + 2]
	cmp al, '0'
	je .r10b
	cmp al, '1'
	je .r11b
	cmp al, '2'
	je .r12b
	cmp al, '3'
	je .r13b
	cmp al, '4'
	je .r14b
	cmp al, '5'
	je .r15b
	jmp parser_err_expected_stmt

.al:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	lea rax, [rel parser_reg8_al]
	mov rdx, 2
	ret
.cl:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	lea rax, [rel parser_reg8_cl]
	mov rdx, 2
	ret
.bl_or_bpl:
	cmp byte [rbx + 2], 'x'
	je .bl
	cmp byte [rbx + 2], 'p'
	je .bpl
	jmp parser_err_expected_stmt
.bl:
	lea rax, [rel parser_reg8_bl]
	mov rdx, 2
	ret
.bpl:
	lea rax, [rel parser_reg8_bpl]
	mov rdx, 3
	ret
.dl_or_dil:
	cmp byte [rbx + 2], 'x'
	je .dl
	cmp byte [rbx + 2], 'i'
	je .dil
	jmp parser_err_expected_stmt
.dl:
	lea rax, [rel parser_reg8_dl]
	mov rdx, 2
	ret
.dil:
	lea rax, [rel parser_reg8_dil]
	mov rdx, 3
	ret
.sil_or_spl:
	cmp byte [rbx + 2], 'i'
	je .sil
	cmp byte [rbx + 2], 'p'
	je .spl
	jmp parser_err_expected_stmt
.sil:
	lea rax, [rel parser_reg8_sil]
	mov rdx, 3
	ret
.spl:
	lea rax, [rel parser_reg8_spl]
	mov rdx, 3
	ret

.r10b:
	lea rax, [rel parser_reg8_r10b]
	mov rdx, 4
	ret
.r11b:
	lea rax, [rel parser_reg8_r11b]
	mov rdx, 4
	ret
.r12b:
	lea rax, [rel parser_reg8_r12b]
	mov rdx, 4
	ret
.r13b:
	lea rax, [rel parser_reg8_r13b]
	mov rdx, 4
	ret
.r14b:
	lea rax, [rel parser_reg8_r14b]
	mov rdx, 4
	ret
.r15b:
	lea rax, [rel parser_reg8_r15b]
	mov rdx, 4
	ret

; rdi=reg_ptr, rsi=reg_len
; returns rax=reg16_ptr, rdx=reg16_len
parser_reg_to_reg16:
	; len2: r8/r9 -> r8w/r9w
	mov rbx, rdi
	mov rcx, rsi
	cmp rcx, 2
	je .len2
	cmp rcx, 3
	je .len3
	jmp parser_err_expected_stmt

.len2:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], '8'
	je .r8w
	cmp byte [rbx + 1], '9'
	je .r9w
	jmp parser_err_expected_stmt
.r8w:
	lea rax, [rel parser_reg16_r8w]
	mov rdx, 3
	ret
.r9w:
	lea rax, [rel parser_reg16_r9w]
	mov rdx, 3
	ret

.len3:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], 'a'
	je .ax
	cmp byte [rbx + 1], 'b'
	je .bx_or_bp
	cmp byte [rbx + 1], 'c'
	je .cx
	cmp byte [rbx + 1], 'd'
	je .dx_or_di
	cmp byte [rbx + 1], 's'
	je .si_or_sp
	cmp byte [rbx + 1], '1'
	jne parser_err_expected_stmt
	mov al, [rbx + 2]
	cmp al, '0'
	je .r10w
	cmp al, '1'
	je .r11w
	cmp al, '2'
	je .r12w
	cmp al, '3'
	je .r13w
	cmp al, '4'
	je .r14w
	cmp al, '5'
	je .r15w
	jmp parser_err_expected_stmt

.ax:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	lea rax, [rel parser_reg16_ax]
	mov rdx, 2
	ret
.cx:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	lea rax, [rel parser_reg16_cx]
	mov rdx, 2
	ret
.bx_or_bp:
	cmp byte [rbx + 2], 'x'
	je .bx
	cmp byte [rbx + 2], 'p'
	je .bp
	jmp parser_err_expected_stmt
.bx:
	lea rax, [rel parser_reg16_bx]
	mov rdx, 2
	ret
.bp:
	lea rax, [rel parser_reg16_bp]
	mov rdx, 2
	ret
.dx_or_di:
	cmp byte [rbx + 2], 'x'
	je .dx
	cmp byte [rbx + 2], 'i'
	je .di
	jmp parser_err_expected_stmt
.dx:
	lea rax, [rel parser_reg16_dx]
	mov rdx, 2
	ret
.di:
	lea rax, [rel parser_reg16_di]
	mov rdx, 2
	ret
.si_or_sp:
	cmp byte [rbx + 2], 'i'
	je .si
	cmp byte [rbx + 2], 'p'
	je .sp
	jmp parser_err_expected_stmt
.si:
	lea rax, [rel parser_reg16_si]
	mov rdx, 2
	ret
.sp:
	lea rax, [rel parser_reg16_sp]
	mov rdx, 2
	ret

.r10w:
	lea rax, [rel parser_reg16_r10w]
	mov rdx, 4
	ret
.r11w:
	lea rax, [rel parser_reg16_r11w]
	mov rdx, 4
	ret
.r12w:
	lea rax, [rel parser_reg16_r12w]
	mov rdx, 4
	ret
.r13w:
	lea rax, [rel parser_reg16_r13w]
	mov rdx, 4
	ret
.r14w:
	lea rax, [rel parser_reg16_r14w]
	mov rdx, 4
	ret
.r15w:
	lea rax, [rel parser_reg16_r15w]
	mov rdx, 4
	ret

; rdi=reg_ptr, rsi=reg_len
; returns rax=reg32_ptr, rdx=reg32_len
parser_reg_to_reg32:
	; len2: r8/r9 -> r8d/r9d
	mov rbx, rdi
	mov rcx, rsi
	cmp rcx, 2
	je .len2
	cmp rcx, 3
	je .len3
	jmp parser_err_expected_stmt

.len2:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], '8'
	je .r8d
	cmp byte [rbx + 1], '9'
	je .r9d
	jmp parser_err_expected_stmt
.r8d:
	lea rax, [rel parser_reg32_r8d]
	mov rdx, 3
	ret
.r9d:
	lea rax, [rel parser_reg32_r9d]
	mov rdx, 3
	ret

.len3:
	cmp byte [rbx + 0], 'r'
	jne parser_err_expected_stmt
	cmp byte [rbx + 1], 'a'
	je .eax
	cmp byte [rbx + 1], 'b'
	je .ebx_or_ebp
	cmp byte [rbx + 1], 'c'
	je .ecx
	cmp byte [rbx + 1], 'd'
	je .edx_or_edi
	cmp byte [rbx + 1], 's'
	je .esi_or_esp
	cmp byte [rbx + 1], '1'
	jne parser_err_expected_stmt
	mov al, [rbx + 2]
	cmp al, '0'
	je .r10d
	cmp al, '1'
	je .r11d
	cmp al, '2'
	je .r12d
	cmp al, '3'
	je .r13d
	cmp al, '4'
	je .r14d
	cmp al, '5'
	je .r15d
	jmp parser_err_expected_stmt

.eax:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	lea rax, [rel parser_reg32_eax]
	mov rdx, 3
	ret
.ecx:
	cmp byte [rbx + 2], 'x'
	jne parser_err_expected_stmt
	lea rax, [rel parser_reg32_ecx]
	mov rdx, 3
	ret
.ebx_or_ebp:
	cmp byte [rbx + 2], 'x'
	je .ebx
	cmp byte [rbx + 2], 'p'
	je .ebp
	jmp parser_err_expected_stmt
.ebx:
	lea rax, [rel parser_reg32_ebx]
	mov rdx, 3
	ret
.ebp:
	lea rax, [rel parser_reg32_ebp]
	mov rdx, 3
	ret
.edx_or_edi:
	cmp byte [rbx + 2], 'x'
	je .edx
	cmp byte [rbx + 2], 'i'
	je .edi
	jmp parser_err_expected_stmt
.edx:
	lea rax, [rel parser_reg32_edx]
	mov rdx, 3
	ret
.edi:
	lea rax, [rel parser_reg32_edi]
	mov rdx, 3
	ret
.esi_or_esp:
	cmp byte [rbx + 2], 'i'
	je .esi
	cmp byte [rbx + 2], 'p'
	je .esp
	jmp parser_err_expected_stmt
.esi:
	lea rax, [rel parser_reg32_esi]
	mov rdx, 3
	ret
.esp:
	lea rax, [rel parser_reg32_esp]
	mov rdx, 3
	ret

.r10d:
	lea rax, [rel parser_reg32_r10d]
	mov rdx, 4
	ret
.r11d:
	lea rax, [rel parser_reg32_r11d]
	mov rdx, 4
	ret
.r12d:
	lea rax, [rel parser_reg32_r12d]
	mov rdx, 4
	ret
.r13d:
	lea rax, [rel parser_reg32_r13d]
	mov rdx, 4
	ret
.r14d:
	lea rax, [rel parser_reg32_r14d]
	mov rdx, 4
	ret
.r15d:
	lea rax, [rel parser_reg32_r15d]
	mov rdx, 4
	ret

section .rodata
parser_mem_local_prefix: db "[rbp"
parser_mem_local_prefix_len: equ $-parser_mem_local_prefix
parser_mem_global_prefix: db "[rel "
parser_mem_global_prefix_len: equ $-parser_mem_global_prefix

parser_reg8_al:   db "al"
parser_reg8_bl:   db "bl"
parser_reg8_cl:   db "cl"
parser_reg8_dl:   db "dl"
parser_reg8_sil:  db "sil"
parser_reg8_dil:  db "dil"
parser_reg8_spl:  db "spl"
parser_reg8_bpl:  db "bpl"
parser_reg8_r8b:  db "r8b"
parser_reg8_r9b:  db "r9b"
parser_reg8_r10b: db "r10b"
parser_reg8_r11b: db "r11b"
parser_reg8_r12b: db "r12b"
parser_reg8_r13b: db "r13b"
parser_reg8_r14b: db "r14b"
parser_reg8_r15b: db "r15b"

parser_reg16_ax:   db "ax"
parser_reg16_bx:   db "bx"
parser_reg16_cx:   db "cx"
parser_reg16_dx:   db "dx"
parser_reg16_si:   db "si"
parser_reg16_di:   db "di"
parser_reg16_sp:   db "sp"
parser_reg16_bp:   db "bp"
parser_reg16_r8w:  db "r8w"
parser_reg16_r9w:  db "r9w"
parser_reg16_r10w: db "r10w"
parser_reg16_r11w: db "r11w"
parser_reg16_r12w: db "r12w"
parser_reg16_r13w: db "r13w"
parser_reg16_r14w: db "r14w"
parser_reg16_r15w: db "r15w"

parser_reg32_eax:  db "eax"
parser_reg32_ebx:  db "ebx"
parser_reg32_ecx:  db "ecx"
parser_reg32_edx:  db "edx"
parser_reg32_esi:  db "esi"
parser_reg32_edi:  db "edi"
parser_reg32_esp:  db "esp"
parser_reg32_ebp:  db "ebp"
parser_reg32_r8d:  db "r8d"
parser_reg32_r9d:  db "r9d"
parser_reg32_r10d: db "r10d"
parser_reg32_r11d: db "r11d"
parser_reg32_r12d: db "r12d"
parser_reg32_r13d: db "r13d"
parser_reg32_r14d: db "r14d"
parser_reg32_r15d: db "r15d"

parser_layout_size_suffix: db ".SIZE"
parser_layout_size_suffix_len: equ $-parser_layout_size_suffix

parser_msg_namepool_overflow: db "parse error: name pool overflow",10,0

; Canonical 64-bit reg name table (id -> string)
parser_reg64_rax: db "rax"
parser_reg64_rbx: db "rbx"
parser_reg64_rcx: db "rcx"
parser_reg64_rdx: db "rdx"
parser_reg64_rsi: db "rsi"
parser_reg64_rdi: db "rdi"
parser_reg64_rsp: db "rsp"
parser_reg64_rbp: db "rbp"
parser_reg64_r8:  db "r8"
parser_reg64_r9:  db "r9"
parser_reg64_r10: db "r10"
parser_reg64_r11: db "r11"
parser_reg64_r12: db "r12"
parser_reg64_r13: db "r13"
parser_reg64_r14: db "r14"
parser_reg64_r15: db "r15"

parser_reg64_ptrs:
	dq parser_reg64_rax
	dq parser_reg64_rbx
	dq parser_reg64_rcx
	dq parser_reg64_rdx
	dq parser_reg64_rsi
	dq parser_reg64_rdi
	dq parser_reg64_rsp
	dq parser_reg64_rbp
	dq parser_reg64_r8
	dq parser_reg64_r9
	dq parser_reg64_r10
	dq parser_reg64_r11
	dq parser_reg64_r12
	dq parser_reg64_r13
	dq parser_reg64_r14
	dq parser_reg64_r15

parser_reg64_lens:
	dq 3,3,3,3,3,3,3,3
	dq 2,2,3,3,3,3,3,3

parser_str_db_suffix: db ": db "
parser_str_db_suffix_len: equ $-parser_str_db_suffix
parser_str_comma: db ", "
parser_str_comma_len: equ $-parser_str_comma
parser_str_zero: db "0"
parser_str_zero_len: equ $-parser_str_zero
parser_str_nl: db 10
parser_str_end_suffix: db "_end"
parser_str_end_suffix_len: equ $-parser_str_end_suffix
parser_str_colon_nl: db ":",10
parser_str_colon_nl_len: equ $-parser_str_colon_nl

section .bss
parser_mem_buf:     resb 128
parser_str_count:   resq 1
parser_str_label_buf:     resb 64
parser_str_end_label_buf: resb 80

parser_namepool_idx: resq 1
parser_namepool:     resb 8192
parser_namepool_cap equ 8192
