section .text

parse_program:
	call lexer_init

	mov qword [parser_seen_main], 0
.glob_loop:
	call lexer_peek
	cmp rax, TOK_KW_VAR
	jne .after_globs
	call parser_parse_global_var_decl
	jmp .glob_loop
.after_globs:
	call emit_start_shim

.func_loop:
	call lexer_peek
	cmp rax, TOK_KW_FUNC
	jne .after_funcs
	call parser_parse_func
	jmp .func_loop

.after_funcs:
	mov rax, [parser_seen_main]
	test rax, rax
	jnz .have_main
	lea rdi, [rel parser_msg_expected_main]
	mov rsi, 1
	call util_fatal_line
.have_main:
	call lexer_peek
	cmp rax, TOK_EOF
	jne parser_err_expected_eof
	ret

parser_parse_func:
	push rbp
	mov rbp, rsp
	push r12
	push r13
	push r14
	push r15

	; reset locals per function
	mov qword [parser_local_count], 0
	mov qword [parser_local_bytes], 0
	; reset per-function if label counter
	mov qword [parser_if_count], 0

	; func
	mov rdi, TOK_KW_FUNC
	call lexer_expect

	; name ident
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]

	; track whether this is main
	mov rax, r15
	cmp rax, 4
	jne .not_main
	mov rbx, r14
	cmp byte [rbx + 0], 'm'
	jne .not_main
	cmp byte [rbx + 1], 'a'
	jne .not_main
	cmp byte [rbx + 2], 'i'
	jne .not_main
	cmp byte [rbx + 3], 'n'
	jne .not_main
	mov qword [parser_seen_main], 1
.not_main:

	call lexer_next

	; args: ( ... )
	mov rdi, TOK_LPAREN
	call lexer_expect
	call parser_parse_args
	mov rdi, TOK_RPAREN
	call lexer_expect

	; {
	mov rdi, TOK_LBRACE
	call lexer_expect

	; emit function label + prologue
	mov rdi, r14
	mov rsi, r15
	call emit_func_label
	call emit_main_prologue

	; var decl loop (First Statement Rule)
.var_loop:
	call lexer_peek
	cmp rax, TOK_KW_VAR
	jne .after_vars
	call parser_parse_var_decl
	jmp .var_loop

.after_vars:
	; alignment padding (do not register symbol)
	; Keep Stage1 rule: total local allocation is 16-byte aligned.
	mov rax, [parser_local_bytes]
	test rax, 8
	jz .no_pad
	call emit_align_pad
	add qword [parser_local_bytes], 8
.no_pad:

	; parse statements until end of function
.stmt_loop:
	call lexer_peek
	cmp rax, TOK_RBRACE
	je .close
	call parser_parse_stmt
	jmp .stmt_loop

.close:
	mov rdi, TOK_RBRACE
	call lexer_expect

	call emit_main_epilogue

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	ret

parser_parse_global_var_decl:
	; top-level:
	;   var name;     -> emit: <name>: dq 0
	;   var name[N];  -> emit: <name>: resb <N>
	mov rdi, TOK_KW_VAR
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	push r12
	push r14
	push r15
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next
	call lexer_peek
	cmp rax, TOK_LBRACK
	jne .scalar

	; array size: [ INT ]
	mov rdi, TOK_LBRACK
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	jne parser_err_expected_int
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call util_parse_u64
	mov r12, rax
	call lexer_next
	mov rdi, TOK_RBRACK
	call lexer_expect
	mov rdi, TOK_SEMI
	call lexer_expect

	; emit into .bss
	mov rdi, r12
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	mov rcx, rdx
	mov rdx, rax
	mov rdi, r14
	mov rsi, r15
	call emit_global_resb
	jmp .done

.scalar:
	mov rdi, TOK_SEMI
	call lexer_expect
	; emit into .data
	mov rdi, r14
	mov rsi, r15
	call emit_global_dq0

.done:
	pop r15
	pop r14
	pop r12
	ret

parser_parse_stmt:
	; supports:
	;   <reg> = <int> ;
	;   <reg> = <reg> ;
	;   <reg> = ptr8[addr] ;
	;   <reg> = ptr64[addr] ;
	;   ptr8[addr] = <reg|int> ;
	;   ptr64[addr] = <reg|int> ;
	push rbx
	push r12
	push r13
	push r14
	push r15

	call lexer_peek
	cmp rax, TOK_REG
	je .stmt_reg
	cmp rax, TOK_IDENT
	je .stmt_call
	cmp rax, TOK_KW_IF
	je .stmt_if
	cmp rax, TOK_KW_PTR8
	je .stmt_store8
	cmp rax, TOK_KW_PTR64
	je .stmt_store64
	jmp parser_err_expected_stmt

.stmt_reg:
	; dst reg slice
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next

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
	cmp rax, TOK_KW_PTR8
	je .rv_ptr8
	cmp rax, TOK_KW_PTR64
	je .rv_ptr64
	jmp parser_err_expected_stmt

.eq_first:
	; read first token slice (int/reg). Could be a plain assignment, or a compare expr.
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	mov r11, rax                ; kind of first token
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
	jmp parser_err_expected_stmt
.bin_rhs:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
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
	jne parser_err_expected_stmt
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	call lexer_next
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
	mov rdi, TOK_LPAREN
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_REG
	jne parser_err_if_lhs_must_be_reg
	mov r12, [lexer_tok_ptr]          ; lhs reg slice
	mov r13, [lexer_tok_len]
	call lexer_next
	call lexer_peek
	mov rbx, rax                      ; cmp token
	cmp rax, TOK_EQEQ
	je .if_cmp_ok
	cmp rax, TOK_NE
	je .if_cmp_ok
	cmp rax, TOK_LT
	je .if_cmp_ok
	cmp rax, TOK_LE
	je .if_cmp_ok
	cmp rax, TOK_GT
	je .if_cmp_ok
	cmp rax, TOK_GE
	je .if_cmp_ok
	jmp parser_err_expected_if_cmp
.if_cmp_ok:
	push rbx                          ; save cmp token kind across calls
	call lexer_next
	call lexer_peek
	cmp rax, TOK_INT
	je .if_rhs_ok
	cmp rax, TOK_REG
	je .if_rhs_ok
	jmp parser_err_expected_if_rhs
.if_rhs_ok:
	mov r8, [lexer_tok_ptr]
	mov r9, [lexer_tok_len]
	push rax                          ; rhs kind
	call lexer_next
	mov rdi, TOK_RPAREN
	call lexer_expect

	; build end label: .Lif_end_<n> (local label, per-function)
	mov rax, [parser_if_count]
	inc rax
	mov [parser_if_count], rax
	mov r14, rax                     ; save if-id for later (nested if-safe)
	mov rdi, r14
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec              ; rax=num_ptr, rdx=num_len
	mov r11, rdx                      ; save num_len
	lea rdi, [rel parser_if_label_buf]
	lea rsi, [rel parser_if_end_prefix]
	mov rcx, parser_if_end_prefix_len
	call parser_copy_bytes
	lea rdi, [rel parser_if_label_buf]
	add rdi, parser_if_end_prefix_len
	mov rsi, rax
	mov rcx, r11
	call parser_copy_bytes
	mov r10, parser_if_end_prefix_len
	add r10, r11                      ; r10 = label_len

	; cmp lhs, rhs
	pop r11                           ; rhs kind
	mov rdi, r12
	mov rsi, r13
	mov rdx, r8
	mov rcx, r9
	cmp r11, TOK_INT
	je .if_cmp_imm
	call emit_cmp_reg_reg
	jmp .if_after_cmp
.if_cmp_imm:
	call emit_cmp_reg_imm
.if_after_cmp:
	pop rbx                           ; cmp token kind

	; choose inverted jump condition (jump to end if condition is false)
	cmp rbx, TOK_EQEQ
	je .if_cc_ne
	cmp rbx, TOK_NE
	je .if_cc_e
	cmp rbx, TOK_LT
	je .if_cc_ge
	cmp rbx, TOK_LE
	je .if_cc_g
	cmp rbx, TOK_GT
	je .if_cc_le
	; GE
	jmp .if_cc_l
.if_cc_ne:
	lea rdi, [rel parser_cc_ne]
	mov rsi, parser_cc_ne_len
	jmp .if_emit_jcc
.if_cc_e:
	lea rdi, [rel parser_cc_e]
	mov rsi, parser_cc_e_len
	jmp .if_emit_jcc
.if_cc_ge:
	lea rdi, [rel parser_cc_ge]
	mov rsi, parser_cc_ge_len
	jmp .if_emit_jcc
.if_cc_g:
	lea rdi, [rel parser_cc_g]
	mov rsi, parser_cc_g_len
	jmp .if_emit_jcc
.if_cc_le:
	lea rdi, [rel parser_cc_le]
	mov rsi, parser_cc_le_len
	jmp .if_emit_jcc
.if_cc_l:
	lea rdi, [rel parser_cc_l]
	mov rsi, parser_cc_l_len
.if_emit_jcc:
	lea rdx, [rel parser_if_label_buf]
	mov rcx, r10
	call emit_jcc

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
	; rebuild end label into parser_if_label_buf (nested if-safe)
	mov rdi, r14
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec              ; rax=num_ptr, rdx=num_len
	mov r11, rdx                      ; num_len
	lea rdi, [rel parser_if_label_buf]
	lea rsi, [rel parser_if_end_prefix]
	mov rcx, parser_if_end_prefix_len
	call parser_copy_bytes
	lea rdi, [rel parser_if_label_buf]
	add rdi, parser_if_end_prefix_len
	mov rsi, rax
	mov rcx, r11
	call parser_copy_bytes
	lea rdi, [rel parser_if_label_buf]
	mov rsi, parser_if_end_prefix_len
	add rsi, r11                      ; label_len
	call emit_label
	jmp .done
.do_shl:
	call emit_shl_reg_imm
	jmp .done

.rv_int:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_mov_reg_imm
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
	je .arg_reg
	cmp rax, TOK_STR
	je .arg_str
	cmp rax, TOK_KW_PTR8
	je .arg_ptr8
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

.arg_reg:
	mov r8, [lexer_tok_ptr]
	mov r9, [lexer_tok_len]
	mov rdi, [parser_call_dst_ptr]
	mov rsi, [parser_call_dst_len]
	mov rdx, r8
	mov rcx, r9
	call emit_mov_reg_reg
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

.rv_reg:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
	call lexer_next
	mov rdi, TOK_SEMI
	call lexer_expect
	mov rdi, r14
	mov rsi, r15
	mov rdx, r12
	mov rcx, r13
	call emit_mov_reg_reg
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

.s8_reg:
	; convert reg -> reg8 slice before consuming
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
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
	je .s64_reg
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
.s64_reg:
	mov r12, [lexer_tok_ptr]
	mov r13, [lexer_tok_len]
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

	; local_count++
	mov rax, [parser_local_count]
	inc rax
	mov [parser_local_count], rax
	; offset = parser_local_bytes + alloc_bytes
	mov rcx, [parser_local_bytes]
	add rcx, r12
	; store entry at index (local_count-1)
	dec rax
	lea rbx, [rel parser_local_ptrs]
	mov [rbx + rax*8], r14
	lea rbx, [rel parser_local_lens]
	mov [rbx + rax*8], r15
	lea rbx, [rel parser_local_offs]
	mov [rbx + rax*8], rcx

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
	; local_count++
	mov rax, [parser_local_count]
	inc rax
	mov [parser_local_count], rax
	; offset = parser_local_bytes + 8
	mov rcx, [parser_local_bytes]
	add rcx, 8
	; store entry at index (local_count-1)
	dec rax
	lea rbx, [rel parser_local_ptrs]
	mov [rbx + rax*8], r14
	lea rbx, [rel parser_local_lens]
	mov [rbx + rax*8], r15
	lea rbx, [rel parser_local_offs]
	mov [rbx + rax*8], rcx
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

; rdi=name_ptr, rsi=name_len
; returns rax=offset (bytes) if local found else 0
parser_find_local_offset:
	push rbp
	mov rbp, rsp
	push r12
	push r13
	push r14
	push r15

	mov r14, rdi
	mov r15, rsi

	mov r12, [parser_local_count]
	test r12, r12
	jz .not_found

	dec r12
.loop:
	lea rbx, [rel parser_local_lens]
	mov r13, [rbx + r12*8]
	cmp r13, r15
	jne .next

	lea rbx, [rel parser_local_ptrs]
	mov r8, [rbx + r12*8]
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
	lea rbx, [rel parser_local_offs]
	mov rax, [rbx + r12*8]
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
	call lexer_next

	; optional index: ident + reg
	xor r12d, r12d                  ; has_index
	call lexer_peek
	cmp rax, TOK_PLUS
	jne .after_ident_index
	call lexer_next
	call lexer_peek
	cmp rax, TOK_REG
	jne parser_err_expected_addr
	mov r13, [lexer_tok_ptr]         ; index_ptr
	mov r11, [lexer_tok_len]         ; index_len
	push r13
	push r11
	call lexer_next
	mov r12d, 1
.after_ident_index:
	; '-' is still not allowed for ident base
	call lexer_peek
	cmp rax, TOK_MINUS
	jne .after_ident_checks
	cmp r12d, 0
	je parser_err_expected_addr
	add rsp, 16
	jmp parser_err_expected_addr
.after_ident_checks:
	; disallow a second '+' (only ident or ident+reg)
	call lexer_peek
	cmp rax, TOK_PLUS
	jne .after_ident_checks2
	cmp r12d, 0
	je parser_err_expected_addr
	add rsp, 16
	jmp parser_err_expected_addr
.after_ident_checks2:

	mov rdi, r14
	mov rsi, r15
	call parser_find_local_offset
	test rax, rax
	jz .global_ident

	; local: [rbp-<dec>]
	mov rbx, rax
	lea rdi, [rel parser_mem_buf]
	lea rsi, [rel parser_mem_local_prefix]
	mov rcx, parser_mem_local_prefix_len
	call parser_copy_bytes

	push rdi
	mov rdi, rbx
	lea rsi, [rel parser_num_buf + 63]
	call util_u64_to_dec          ; rax=num_ptr, rdx=num_len
	pop rdi
	mov rsi, rax
	mov rcx, rdx
	call parser_copy_bytes

	cmp r12d, 0
	je .local_close
	mov byte [rdi], '+'
	inc rdi
	pop rcx                        ; index_len
	pop rsi                        ; index_ptr
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

	cmp r12d, 0
	je .global_close
	mov byte [rdi], '+'
	inc rdi
	pop rcx                        ; index_len
	pop rsi                        ; index_ptr
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
	jne parser_err_expected_addr
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

parser_expect_ident_main:
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident

	mov rax, [lexer_tok_len]
	cmp rax, 4
	jne .bad
	mov rbx, [lexer_tok_ptr]
	cmp byte [rbx + 0], 'm'
	jne .bad
	cmp byte [rbx + 1], 'a'
	jne .bad
	cmp byte [rbx + 2], 'i'
	jne .bad
	cmp byte [rbx + 3], 'n'
	jne .bad
	call lexer_next
	ret
.bad:
	lea rdi, [rel parser_msg_expected_main]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

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

parser_err_expected_ident:
	lea rdi, [rel parser_msg_expected_ident]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_expected_eof:
	lea rdi, [rel parser_msg_expected_eof]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_expected_stmt:
	lea rdi, [rel parser_msg_expected_stmt]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_expected_addr:
	lea rdi, [rel parser_msg_expected_addr]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_expected_int:
	lea rdi, [rel parser_msg_expected_int]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_cmp_only_in_if:
	lea rdi, [rel parser_msg_cmp_only_in_if]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_if_lhs_must_be_reg:
	lea rdi, [rel parser_msg_if_lhs_must_be_reg]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_expected_if_cmp:
	lea rdi, [rel parser_msg_expected_if_cmp]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_expected_if_rhs:
	lea rdi, [rel parser_msg_expected_if_rhs]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_store64_imm_too_big:
	lea rdi, [rel parser_msg_store64_imm_too_big]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

section .rodata
parser_msg_expected_main: db "parse error: expected function name 'main'",10,0
parser_msg_expected_ident: db "parse error: expected identifier",10,0
parser_msg_expected_eof: db "parse error: expected end of file",10,0
parser_msg_too_many_args: db "parse error: too many args (max 6)",10,0
parser_msg_expected_stmt: db "parse error: expected statement",10,0

parser_msg_expected_int: db "parse error: expected integer",10,0

parser_msg_cmp_only_in_if: db "parse error: comparisons are only allowed inside if",10,0
parser_msg_if_lhs_must_be_reg: db "parse error: if lhs must be a register",10,0
parser_msg_expected_if_cmp: db "parse error: expected comparison operator in if",10,0
parser_msg_expected_if_rhs: db "parse error: expected int or reg on rhs of if",10,0

parser_msg_store64_imm_too_big: db "parse error: ptr64 store immediate must fit in signed 32-bit; load into a register first",10,0

parser_if_end_prefix: db "Lif_end_"
parser_if_end_prefix_len: equ $-parser_if_end_prefix

parser_msg_expected_addr: db "parse error: expected address inside ptr[]",10,0

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

parser_mem_local_prefix: db "[rbp-"
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
parser_call_dst_ptr: resq 1
parser_call_dst_len: resq 1
parser_seen_main:    resq 1
parser_local_count: resq 1
parser_local_bytes: resq 1
parser_local_ptrs:  resq 256
parser_local_lens:  resq 256
parser_local_offs:  resq 256
parser_mem_buf:     resb 128
parser_num_buf:     resb 64
parser_if_count:    resq 1
parser_if_label_buf: resb 64
parser_str_count:   resq 1
parser_str_label_buf:     resb 64
parser_str_end_label_buf: resb 80
