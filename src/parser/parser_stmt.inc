; parser_stmt.inc
; - Statement-level parsing: assignments, calls, if, ptr stores, local var decls

section .text

; -----------------------------------------------------------------------------
; Shared helpers (used by both if/while)
;
; parser_build_label_id
;   rdi = out_buf
;   rsi = prefix_ptr
;   rdx = prefix_len
;   rcx = id (u64)
; returns rax = label_len
parser_build_label_id:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13
	push r14
	push r15

	mov r12, rdi
	mov r13, rsi
	mov r14, rdx
	mov r15, rcx

	mov rdi, r15
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec              ; rax=num_ptr, rdx=num_len
	mov rbx, rax
	mov r11, rdx

	mov rdi, r12
	mov rsi, r13
	mov rcx, r14
	call parser_copy_bytes

	lea rdi, [r12 + r14]
	mov rsi, rbx
	mov rcx, r11
	call parser_copy_bytes

	mov rax, r14
	add rax, r11

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

; parser_parse_cond_emit_jfalse
;   Parses: ( <reg|alias> <cmp> <int|reg|alias|const> )
;   Emits:  cmp + inverted jcc to target
;   rdi = target_label_ptr
;   rsi = target_label_len
parser_parse_cond_emit_jfalse:
	push rbp
	mov rbp, rsp
	push rbx
	push r12
	push r13
	push r14
	push r15

	mov r14, rdi
	mov r15, rsi

	mov rdi, TOK_LPAREN
	call lexer_expect

	; lhs
	call lexer_peek
	cmp rax, TOK_REG
	je .lhs_reg
	cmp rax, TOK_IDENT
	jne parser_err_if_lhs_must_be_reg
	; ident is only allowed here if it's an alias -> reg
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_alias_to_reg_slice
	test rax, rax
	jz parser_err_if_lhs_must_be_reg
	mov r12, rax
	mov r13, rdx
	call lexer_next
	jmp .after_lhs
.lhs_reg:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	call lexer_next
.after_lhs:

	; cmp op
	call lexer_peek
	mov rbx, rax
	cmp rax, TOK_EQEQ
	je .cmp_ok
	cmp rax, TOK_NE
	je .cmp_ok
	cmp rax, TOK_LT
	je .cmp_ok
	cmp rax, TOK_LE
	je .cmp_ok
	cmp rax, TOK_GT
	je .cmp_ok
	cmp rax, TOK_GE
	je .cmp_ok
	jmp parser_err_expected_if_cmp
.cmp_ok:
	call lexer_next

	; rhs
	call lexer_peek
	cmp rax, TOK_INT
	je .rhs_ok
	cmp rax, TOK_REG
	je .rhs_ok
	cmp rax, TOK_IDENT
	je .rhs_ok
	jmp parser_err_expected_if_rhs
.rhs_ok:
	mov r8, [lexer_tok_ptr]
	mov r9, [lexer_tok_len]
	mov r11, rax                      ; rhs kind (TOK_INT/TOK_REG/TOK_IDENT)
	cmp r11, TOK_IDENT
	jne .rhs_have
	; ident can be alias->reg or const->imm
	mov rdi, r8
	mov rsi, r9
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_if_rhs
	; returns: ptr in rax, len in rdx, kind in r8
	mov r11, r8                       ; rhs kind (TOK_INT or TOK_REG)
	mov r8, rax
	mov r9, rdx
.rhs_have:
	call lexer_next
	mov rdi, TOK_RPAREN
	call lexer_expect

	; emit cmp
	mov rdi, r12
	mov rsi, r13
	mov rdx, r8
	mov rcx, r9
	cmp r11, TOK_INT
	je .cmp_imm
	; reg
	call emit_cmp_reg_reg
	jmp .after_cmp
.cmp_imm:
	call emit_cmp_reg_imm
.after_cmp:

	; inverted jcc to target
	cmp rbx, TOK_EQEQ
	je .cc_ne
	cmp rbx, TOK_NE
	je .cc_e
	cmp rbx, TOK_LT
	je .cc_ge
	cmp rbx, TOK_LE
	je .cc_g
	cmp rbx, TOK_GT
	je .cc_le
	; GE
	jmp .cc_l
.cc_ne:
	lea rdi, [rel parser_cc_ne]
	mov rsi, parser_cc_ne_len
	jmp .emit
.cc_e:
	lea rdi, [rel parser_cc_e]
	mov rsi, parser_cc_e_len
	jmp .emit
.cc_ge:
	lea rdi, [rel parser_cc_ge]
	mov rsi, parser_cc_ge_len
	jmp .emit
.cc_g:
	lea rdi, [rel parser_cc_g]
	mov rsi, parser_cc_g_len
	jmp .emit
.cc_le:
	lea rdi, [rel parser_cc_le]
	mov rsi, parser_cc_le_len
	jmp .emit
.cc_l:
	lea rdi, [rel parser_cc_l]
	mov rsi, parser_cc_l_len
.emit:
	mov rdx, r14
	mov rcx, r15
	call emit_jcc

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret

parser_parse_stmt:
	; supports:
	;   <reg> = <int> ;
	;   <reg> = <reg> ;
	;   <reg> = ptr8[addr] ;
	;   <reg> = ptr16[addr] ;
	;   <reg> = ptr32[addr] ;
	;   <reg> = ptr64[addr] ;
	;   ptr8[addr]  = <reg|int> ;
	;   ptr16[addr] = <reg|int> ;
	;   ptr32[addr] = <reg|int> ;
	;   ptr64[addr] = <reg|int> ;
	push rbx
	push r12
	push r13
	push r14
	push r15

	call lexer_peek
	cmp rax, TOK_KW_ALIAS
	je .stmt_alias
	cmp rax, TOK_REG
	je .stmt_reg
	cmp rax, TOK_IDENT
	je .stmt_ident
	cmp rax, TOK_KW_IF
	je .stmt_if
	cmp rax, TOK_KW_WHILE
	je .stmt_while
	cmp rax, TOK_KW_PTR8
	je .stmt_store8
	cmp rax, TOK_KW_PTR16
	je .stmt_store16
	cmp rax, TOK_KW_PTR32
	je .stmt_store32
	cmp rax, TOK_KW_PTR64
	je .stmt_store64
	jmp parser_err_expected_stmt

.stmt_alias:
	; alias <reg> : <ident> ;
	call lexer_next
	call lexer_peek
	cmp rax, TOK_REG
	jne parser_err_expected_stmt
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	mov rdi, r12
	mov rsi, r13
	call parser_reg64_id_from_slice
	mov rbx, rax
	call lexer_next

	mov rdi, TOK_COLON
	call lexer_expect

	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next

	mov rdi, r14
	mov rsi, r15
	mov rdx, SYM_ALIAS
	mov rcx, rbx
	call parser_sym_add

	mov rdi, TOK_SEMI
	call lexer_expect
	jmp .done

.stmt_ident:
	; Either a function call, or an alias-used-as-register statement.
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_alias_to_reg_slice
	test rax, rax
	jz .stmt_call
	; treat as reg statement: rax/rdx = canonical reg slice
	mov r14, rax
	mov r15, rdx
	call lexer_next
	jmp .stmt_reg_after_name

.stmt_reg:
	; dst reg slice
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next

.stmt_reg_after_name:

	; operator: '=' or compound-assignment
	call lexer_peek
	cmp rax, TOK_EQ
	je .op_eq
	cmp rax, TOK_PLUSEQ
	je .op_pluseq
	cmp rax, TOK_MINUSEQ
	je .op_minuseq
	cmp rax, TOK_STAREQ
	je .op_stareq
	cmp rax, TOK_ANDEQ
	je .op_andeq
	cmp rax, TOK_OREQ
	je .op_oreq
	cmp rax, TOK_XOREQ
	je .op_xoreq
	cmp rax, TOK_SHLEQ
	je .op_shleq
	cmp rax, TOK_SHREQ
	je .op_shreq
	jmp parser_err_expected_stmt

.op_eq:
	call lexer_next

	call lexer_peek
	cmp rax, TOK_INT
	je .eq_first
	cmp rax, TOK_REG
	je .eq_first
	cmp rax, TOK_IDENT
	je .eq_first
	cmp rax, TOK_KW_PTR8
	je .rv_ptr8
	cmp rax, TOK_KW_PTR16
	je .rv_ptr16
	cmp rax, TOK_KW_PTR32
	je .rv_ptr32
	cmp rax, TOK_KW_PTR64
	je .rv_ptr64
	jmp parser_err_expected_stmt

.eq_first:
	; read first token slice (int/reg). Could be a plain assignment, or a compare expr.
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	mov r11, rax                ; kind of first token
	cmp r11, TOK_IDENT
	jne .eq_tok_ok
	; ident can be alias->reg or const->imm
	mov rdi, r12
	mov rsi, r13
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	mov r12, rax
	mov r13, rdx
	mov r11, r8
.eq_tok_ok:
	call lexer_next

	; comparisons are not allowed in assignments (Stage1 rule)
	call lexer_peek
	cmp rax, TOK_EQEQ
	je parser_err_cmp_only_in_if
	cmp rax, TOK_NE
	je parser_err_cmp_only_in_if
	cmp rax, TOK_LT
	je parser_err_cmp_only_in_if
	cmp rax, TOK_LE
	je parser_err_cmp_only_in_if
	cmp rax, TOK_GT
	je parser_err_cmp_only_in_if
	cmp rax, TOK_GE
	je parser_err_cmp_only_in_if

	; plain assignment: dst = first;
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	cmp r11, TOK_INT
	je .emit_mov_imm
	; reg
	call emit_mov_reg_reg
	jmp .done
.emit_mov_imm:
	call emit_mov_reg_imm
	jmp .done


.op_pluseq:
	call lexer_next
	mov rbx, TOK_PLUSEQ
	jmp .op_bin_kind
.op_minuseq:
	call lexer_next
	mov rbx, TOK_MINUSEQ
	jmp .op_bin_kind
.op_stareq:
	call lexer_next
	mov rbx, TOK_STAREQ
	jmp .op_bin_kind
.op_andeq:
	call lexer_next
	mov rbx, TOK_ANDEQ
	jmp .op_bin_kind
.op_oreq:
	call lexer_next
	mov rbx, TOK_OREQ
	jmp .op_bin_kind
.op_xoreq:
	call lexer_next
	mov rbx, TOK_XOREQ
	jmp .op_bin_kind
.op_shleq:
	call lexer_next
	mov rbx, TOK_SHLEQ
	jmp .op_shift
.op_shreq:
	call lexer_next
	mov rbx, TOK_SHREQ
	jmp .op_shift
.op_bin_kind:
	; dst <op>= (int|reg);
	call lexer_peek
	cmp rax, TOK_INT
	je .bin_rhs
	cmp rax, TOK_REG
	je .bin_rhs
	cmp rax, TOK_IDENT
	je .bin_rhs
	jmp parser_err_expected_stmt
.bin_rhs:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	cmp rax, TOK_IDENT
	jne .bin_push
	; ident can be alias->reg or const->imm
	mov rdi, r12
	mov rsi, r13
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	mov r12, rax
	mov r13, rdx
	mov rax, r8
.bin_push:
	push rax                    ; save rhs kind across lexer calls
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	pop r11

	; emit op
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	cmp rbx, TOK_PLUSEQ
	je .emit_add
	cmp rbx, TOK_MINUSEQ
	je .emit_sub
	cmp rbx, TOK_STAREQ
	je .emit_imul
	cmp rbx, TOK_ANDEQ
	je .emit_and
	cmp rbx, TOK_OREQ
	je .emit_or
	cmp rbx, TOK_XOREQ
	je .emit_xor
	jmp parser_err_expected_stmt
.emit_add:
	cmp r11, TOK_INT
	je .add_imm
	call emit_add_reg_reg
	jmp .done
.add_imm:
	call emit_add_reg_imm
	jmp .done
.emit_sub:
	cmp r11, TOK_INT
	je .sub_imm
	call emit_sub_reg_reg
	jmp .done
.sub_imm:
	call emit_sub_reg_imm
	jmp .done
.emit_imul:
	cmp r11, TOK_INT
	je .imul_imm
	call emit_imul_reg_reg
	jmp .done
.imul_imm:
	call emit_imul_reg_imm
	jmp .done
.emit_and:
	cmp r11, TOK_INT
	je .and_imm
	call emit_and_reg_reg
	jmp .done
.and_imm:
	call emit_and_reg_imm
	jmp .done
.emit_or:
	cmp r11, TOK_INT
	je .or_imm
	call emit_or_reg_reg
	jmp .done
.or_imm:
	call emit_or_reg_imm
	jmp .done
.emit_xor:
	cmp r11, TOK_INT
	je .xor_imm
	call emit_xor_reg_reg
	jmp .done
.xor_imm:
	call emit_xor_reg_imm
	jmp .done

.op_shift:
	; shifts: only allow imm for now
	call lexer_peek
	cmp rax, TOK_INT
	je .shift_take_int
	cmp rax, TOK_IDENT
	jne parser_err_expected_stmt
	; allow const as shift amount
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_ident_to_reg_or_imm
	cmp r8, TOK_INT
	jne parser_err_expected_stmt
	mov r12, rax
	mov r13, rdx
	call lexer_next
	jmp .shift_after
.shift_take_int:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	call lexer_next
.shift_after:
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	cmp rbx, TOK_SHLEQ
	je .do_shl
	call emit_shr_reg_imm
	jmp .done

.stmt_if:
	; if ( <reg> <cmp> <int|reg> ) { <stmts> }
	call lexer_next

	; allocate id
	mov rax, [parser_if_count]
	inc rax
	mov [parser_if_count], rax
	mov r14, rax                     ; if-id (nested if-safe)

	; build end label into parser_if_label_buf
	lea rdi, [rel parser_if_label_buf]
	lea rsi, [rel parser_if_end_prefix]
	mov rdx, parser_if_end_prefix_len
	mov rcx, r14
	call parser_build_label_id         ; rax=label_len
	mov r10, rax

	; parse condition and emit jcc -> end
	lea rdi, [rel parser_if_label_buf]
	mov rsi, r10
	call parser_parse_cond_emit_jfalse

	push r14                          ; save if-id for emit_label after body (nested if-safe)

	; parse body
	mov rdi, TOK_LBRACE
	call lexer_expect
.if_body_loop:
	call lexer_peek
	cmp rax, TOK_RBRACE
	je .if_body_done
	call parser_parse_stmt
	jmp .if_body_loop
.if_body_done:
	mov rdi, TOK_RBRACE
	call lexer_expect

	; emit end label
	pop r14
	; rebuild end label and emit
	lea rdi, [rel parser_if_label_buf]
	lea rsi, [rel parser_if_end_prefix]
	mov rdx, parser_if_end_prefix_len
	mov rcx, r14
	call parser_build_label_id         ; rax=label_len
	lea rdi, [rel parser_if_label_buf]
	mov rsi, rax
	call emit_label
	jmp .done

.stmt_while:
	; while ( <reg> <cmp> <int|reg> ) { <stmts> }
	call lexer_next

	; allocate id
	mov rax, [parser_while_count]
	inc rax
	mov [parser_while_count], rax
	mov r14, rax                     ; while-id (nested while-safe)

	; emit start label
	lea rdi, [rel parser_while_label_start_buf]
	lea rsi, [rel parser_while_start_prefix]
	mov rdx, parser_while_start_prefix_len
	mov rcx, r14
	call parser_build_label_id         ; rax=len
	lea rdi, [rel parser_while_label_start_buf]
	mov rsi, rax
	call emit_label

	; build end label + parse condition + emit jcc -> end
	lea rdi, [rel parser_while_label_end_buf]
	lea rsi, [rel parser_while_end_prefix]
	mov rdx, parser_while_end_prefix_len
	mov rcx, r14
	call parser_build_label_id         ; rax=end_len
	mov r10, rax
	lea rdi, [rel parser_while_label_end_buf]
	mov rsi, r10
	call parser_parse_cond_emit_jfalse

	push r14                          ; save while-id for rebuilds after body

	; body
	mov rdi, TOK_LBRACE
	call lexer_expect
.while_body_loop:
	call lexer_peek
	cmp rax, TOK_RBRACE
	je .while_body_done
	call parser_parse_stmt
	jmp .while_body_loop
.while_body_done:
	mov rdi, TOK_RBRACE
	call lexer_expect

	; jump back to start (rebuild start label)
	pop r14
	push r14
	lea rdi, [rel parser_while_label_start_buf]
	lea rsi, [rel parser_while_start_prefix]
	mov rdx, parser_while_start_prefix_len
	mov rcx, r14
	call parser_build_label_id         ; rax=start_len
	lea rdi, [rel parser_while_label_start_buf]
	mov rsi, rax
	call emit_jmp

	; emit end label (rebuild end label)
	pop r14
	lea rdi, [rel parser_while_label_end_buf]
	lea rsi, [rel parser_while_end_prefix]
	mov rdx, parser_while_end_prefix_len
	mov rcx, r14
	call parser_build_label_id         ; rax=end_len
	lea rdi, [rel parser_while_label_end_buf]
	mov rsi, rax
	call emit_label
	jmp .done
.do_shl:
	call emit_shl_reg_imm
	jmp .done

.stmt_call:
	; func call: ident '(' [args] ')' ';'
	; save callee name slice
	push r10
	push r11
	push r8
	push r9
	push rsi
	push rdi
	
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next
	
	mov rdi, TOK_LPAREN
	call lexer_expect

	; args
	xor r12d, r12d          ; arg_index
	call lexer_peek
	cmp rax, TOK_RPAREN
	je .call_args_done

.call_arg_loop:
	cmp r12, 6
	jb .call_arg_ok
	lea rdi, [rel parser_msg_too_many_args]
	mov rsi, [lexer_tok_line]
	call util_fatal_line
.call_arg_ok:
	; load arg into ABI register
	lea r10, [rel parser_argreg_ptrs]
	lea r11, [rel parser_argreg_lens]
	mov rax, [r10 + r12*8]
	mov [parser_call_dst_ptr], rax
	mov rax, [r11 + r12*8]
	mov [parser_call_dst_len], rax

	call lexer_peek
	cmp rax, TOK_INT
	je .arg_int
	cmp rax, TOK_REG
	je .arg_reglike
	cmp rax, TOK_IDENT
	je .arg_reglike
	cmp rax, TOK_STR
	je .arg_str
	cmp rax, TOK_KW_PTR8
	je .arg_ptr8
	cmp rax, TOK_KW_PTR16
	je .arg_ptr16
	cmp rax, TOK_KW_PTR32
	je .arg_ptr32
	cmp rax, TOK_KW_PTR64
	je .arg_ptr64
	jmp parser_err_expected_stmt

.arg_int:
	mov r8, [lexer_tok_ptr]
	mov r9, [lexer_tok_len]
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_mov_reg_imm
	call lexer_next
	jmp .arg_after


.arg_reglike:
	; reg token or ident(alias/const)
	mov r8, [lexer_tok_ptr]
	mov r9, [lexer_tok_len]
	cmp rax, TOK_IDENT
	jne .arg_reg
	mov rdi, r8
	mov rsi, r9
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	mov r10, r8                     ; kind
	mov r8, rax                      ; slice ptr
	mov r9, rdx                      ; slice len
	cmp r10, TOK_INT
	je .arg_imm
.arg_reg:
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_mov_reg_reg
	call lexer_next
	jmp .arg_after
.arg_imm:
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_mov_reg_imm
	call lexer_next
	jmp .arg_after

.arg_ptr8:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r8, rax
	mov r9, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_load_ptr8
	jmp .arg_after

.arg_ptr16:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r8, rax
	mov r9, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_load_ptr16
	jmp .arg_after

.arg_ptr32:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r8, rax
	mov r9, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	; use 32-bit subreg for zero-extend
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	call parser_reg_to_reg32
	mov r10, rax
	mov r11, rdx
	mov rdi, r10
	mov rsi, r11
	mov rdx, r8
	mov rcx, r9
	call emit_load_ptr32
	jmp .arg_after

.arg_ptr64:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r8, rax
	mov r9, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_load_ptr64

.arg_after:
	inc r12
	call lexer_peek
	cmp rax, TOK_COMMA
	jne .call_args_done
	call lexer_next
	jmp .call_arg_loop

.arg_str:
	; string literal becomes pointer to interned, 0-terminated data
	mov r8, [lexer_tok_ptr]
	mov r9, [lexer_tok_len]
	call lexer_next
	mov rdi, r8
	mov rsi, r9
	call parser_intern_string
	; rax=label_ptr, rdx=label_len
	mov r8, rax
	mov r9, rdx
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_lea_reg_label
	jmp .arg_after

.call_args_done:
	mov rdi, TOK_RPAREN
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect

	; emit call <ident>
	mov rdi, r14
	mov rsi, r15
	call emit_call_ident

	pop rdi
	pop rsi
	pop r9
	pop r8
	pop r11
	pop r10
	jmp .done

.rv_ptr8:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r12, rax
	mov r13, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_load_ptr8
	jmp .done

.rv_ptr16:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r12, rax
	mov r13, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_load_ptr16
	jmp .done

.rv_ptr32:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r12, rax
	mov r13, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect
	; dst must be 32-bit reg name (eax/r8d/..)
	mov rdi, r14
	mov rsi, r15
	call parser_reg_to_reg32
	mov r10, rax
	mov r11, rdx
	mov rdi, r10
	mov rsi, r11
	mov rdx, r12
	mov rcx, r13
	call emit_load_ptr32
	jmp .done

.rv_ptr64:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r12, rax
	mov r13, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_load_ptr64
	jmp .done

.stmt_store8:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r14, rax
	mov r15, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_EQ
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	je .s8_int
	cmp rax, TOK_REG
	je .s8_reg
	cmp rax, TOK_IDENT
	je .s8_ident
	jmp parser_err_expected_stmt
.s8_int:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr8
	jmp .done

.s8_ident:
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	cmp r8, TOK_INT
	je .s8_ident_imm
	; reg (alias)
	mov rdi, rax
	mov rsi, rdx
	jmp .s8_have_reg64
.s8_ident_imm:
	; treat as immediate
	mov r12, rax
	mov r13, rdx
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr8
	jmp .done

.s8_reg:
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]


.s8_have_reg64:
	call parser_reg_to_reg8
	; rax=reg8_ptr, rdx=reg8_len
	mov r12, rax
	mov r13, rdx
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr8
	jmp .done

.stmt_store16:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r14, rax
	mov r15, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_EQ
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	je .s16_intlike
	cmp rax, TOK_REG
	je .s16_reglike
	cmp rax, TOK_IDENT
	je .s16_ident
	jmp parser_err_expected_stmt
.s16_ident:
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	cmp r8, TOK_INT
	je .s16_intlike_take
	; alias -> reg
	mov r12, rax
	mov r13, rdx
	jmp .s16_reglike_after_slice
.s16_intlike_take:
	mov r12, rax
	mov r13, rdx
	jmp .s16_intlike_after_slice
.s16_intlike:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
.s16_intlike_after_slice:
	; range check 16-bit
	mov rdi, r12
	mov rsi, r13
	call util_parse_u64
	cmp rax, 0xffff
	ja parser_err_expected_stmt
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr16
	jmp .done
.s16_reglike:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
.s16_reglike_after_slice:
	; convert reg64 -> reg16
	mov rdi, r12
	mov rsi, r13
	call parser_reg_to_reg16
	mov r12, rax
	mov r13, rdx
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr16
	jmp .done

.stmt_store32:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r14, rax
	mov r15, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_EQ
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	je .s32_intlike
	cmp rax, TOK_REG
	je .s32_reglike
	cmp rax, TOK_IDENT
	je .s32_ident
	jmp parser_err_expected_stmt
.s32_ident:
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	cmp r8, TOK_INT
	je .s32_intlike_take
	; alias -> reg
	mov r12, rax
	mov r13, rdx
	jmp .s32_reglike_after_slice
.s32_intlike_take:
	mov r12, rax
	mov r13, rdx
	jmp .s32_intlike_after_slice
.s32_intlike:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
.s32_intlike_after_slice:
	; range check 32-bit
	mov rdi, r12
	mov rsi, r13
	call util_parse_u64
	mov rbx, 0xffffffff
	cmp rax, rbx
	ja parser_err_expected_stmt
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr32
	jmp .done
.s32_reglike:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
.s32_reglike_after_slice:
	; convert reg64 -> reg32
	mov rdi, r12
	mov rsi, r13
	call parser_reg_to_reg32
	mov r12, rax
	mov r13, rdx
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr32
	jmp .done

.stmt_store64:
	call lexer_next
	mov rdi, TOK_LBRACK
	call lexer_expect
	call parser_parse_mem_operand_after_lbrack
	mov r14, rax
	mov r15, rdx
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_EQ
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	je .s64_int
	cmp rax, TOK_REG
	je .s64_reglike
	cmp rax, TOK_IDENT
	je .s64_ident
	jmp parser_err_expected_stmt
.s64_int:
	; x86-64 has no encoding for `mov qword [mem], imm64`.
	; NASM will encode imm32 with sign-extension (and warn/truncate).
	; Require the user to load large immediates into a register first.
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	mov rdi, r12
	mov rsi, r13
	call util_parse_u64
	cmp rax, 0x7fffffff
	ja parser_err_store64_imm_too_big
	jmp .s64_after_val

.s64_ident:
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call parser_ident_to_reg_or_imm
	test rax, rax
	jz parser_err_expected_stmt
	cmp r8, TOK_INT
	je .s64_ident_imm
	; alias -> reg
	mov r12, rax
	mov r13, rdx
	jmp .s64_after_val
.s64_ident_imm:
	; enforce signed-32 range like normal imm branch
	mov r12, rax
	mov r13, rdx
	mov rdi, r12
	mov rsi, r13
	call util_parse_u64
	cmp rax, 0x7fffffff
	ja parser_err_store64_imm_too_big
	jmp .s64_after_val

.s64_reglike:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	; TOK_IDENT handled in .s64_ident
.s64_after_val:
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_store_ptr64
	jmp .done

.done:
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	ret

parser_parse_var_decl:
	; function-scope:
	;   var name;     -> 8-byte slot (push 0)
	;   var name[N];  -> raw stack bytes (sub rsp, round_up(N,8))
	mov rdi, TOK_KW_VAR
	call lexer_expect
	push r12

	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	; consume identifier and register local
	push r14
	push r15
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next

	; optional array size: [ INT ]
	call lexer_peek
	cmp rax, TOK_LBRACK
	jne .scalar
	; [
	mov rdi, TOK_LBRACK
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	jne parser_err_expected_int
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call util_parse_u64
	mov r12, rax               ; N
	call lexer_next
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect

	; alloc_bytes = round_up(N, 8)
	mov rax, r12
	add rax, 7
	and rax, -8
	mov r12, rax

	; offset = parser_local_bytes + alloc_bytes
	mov rcx, [parser_local_bytes]
	add rcx, r12
	; register symbol: payload is signed stack offset (negative)
	mov rdi, r14
	mov rsi, r15
	mov rdx, SYM_VAR_LOCAL
	mov r8, rcx
	neg r8
	mov rcx, r8
	call parser_sym_add

	; allocate raw bytes (no init)
	mov rdi, r12
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	mov rdi, rax
	mov rsi, rdx
	call emit_sub_rsp_imm
	add [parser_local_bytes], r12
	pop r15
	pop r14
	pop r12
	ret

.scalar:
	; scalar: ';'
	mov rdi, TOK_SEMI
	call lexer_expect
	; offset = parser_local_bytes + 8
	mov rcx, [parser_local_bytes]
	add rcx, 8
	; register symbol: payload is signed stack offset (negative)
	mov rdi, r14
	mov rsi, r15
	mov rdx, SYM_VAR_LOCAL
	mov r8, rcx
	neg r8
	mov rcx, r8
	call parser_sym_add
	; allocate slot (push 0)
	call emit_push0
	add qword [parser_local_bytes], 8
	pop r15
	pop r14
	pop r12
	ret

parser_parse_args:
	; args are identifiers separated by commas; max 6.
	xor r13d, r13d
	call lexer_peek
	cmp rax, TOK_RPAREN
	je .done
.arg_loop:
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	call lexer_next
	inc r13
	cmp r13, 6
	jbe .maybe_more
	lea rdi, [rel parser_msg_too_many_args]
	mov rsi, [lexer_tok_line]
	call util_fatal_line
.maybe_more:
	call lexer_peek
	cmp rax, TOK_COMMA
	jne .done
	call lexer_next
	jmp .arg_loop
.done:
	ret

section .rodata
parser_if_end_prefix: db "Lif_end_"
parser_if_end_prefix_len: equ $-parser_if_end_prefix

parser_while_start_prefix: db "Lwhile_start_"
parser_while_start_prefix_len: equ $-parser_while_start_prefix
parser_while_end_prefix: db "Lwhile_end_"
parser_while_end_prefix_len: equ $-parser_while_end_prefix

; setcc condition codes (for boolean compares)
parser_cc_e:  db "e"
parser_cc_e_len:  equ $-parser_cc_e
parser_cc_ne: db "ne"
parser_cc_ne_len: equ $-parser_cc_ne
parser_cc_l:  db "l"
parser_cc_l_len:  equ $-parser_cc_l
parser_cc_le: db "le"
parser_cc_le_len: equ $-parser_cc_le
parser_cc_g:  db "g"
parser_cc_g_len:  equ $-parser_cc_g
parser_cc_ge: db "ge"
parser_cc_ge_len: equ $-parser_cc_ge

parser_argreg_rdi: db "rdi"
parser_argreg_rsi: db "rsi"
parser_argreg_rdx: db "rdx"
parser_argreg_rcx: db "rcx"
parser_argreg_r8:  db "r8"
parser_argreg_r9:  db "r9"

parser_argreg_ptrs:
	dq parser_argreg_rdi
	dq parser_argreg_rsi
	dq parser_argreg_rdx
	dq parser_argreg_rcx
	dq parser_argreg_r8
	dq parser_argreg_r9

parser_argreg_lens:
	dq 3
	dq 3
	dq 3
	dq 3
	dq 2
	dq 2

section .bss
parser_call_dst_ptr: resq 1
parser_call_dst_len: resq 1
parser_if_count:    resq 1
parser_if_label_buf: resb 64
parser_while_count: resq 1
parser_while_label_start_buf: resb 64
parser_while_label_end_buf: resb 64
