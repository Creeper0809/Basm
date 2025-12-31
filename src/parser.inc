; parser.inc (Control Tower)
; - High-level parse flow: program / function / global var
; - Shared error labels + common messages

%include "src/parser/parser_sym.inc"
%include "src/parser/parser_util.inc"
%include "src/parser/parser_stmt.inc"

section .text

parse_program:
	call lexer_init

	; reset global symbol base + name pool
	mov qword [parser_sym_count], 0
	mov qword [parser_sym_global_count], 0
	mov qword [parser_namepool_idx], 0

	mov qword [parser_seen_main], 0
.glob_loop:
	call lexer_peek
	cmp rax, TOK_KW_VAR
	je .glob_var
	cmp rax, TOK_KW_CONST
	je .glob_const
	cmp rax, TOK_KW_LAYOUT
	je .glob_layout
	jmp .after_globs
.glob_var:
	call parser_parse_global_var_decl
	; globals live at bottom of sym table too
	mov rax, [parser_sym_count]
	mov [parser_sym_global_count], rax
	jmp .glob_loop
.glob_const:
	call parser_parse_const_decl
	mov rax, [parser_sym_count]
	mov [parser_sym_global_count], rax
	jmp .glob_loop
.glob_layout:
	call parser_parse_layout_decl
	mov rax, [parser_sym_count]
	mov [parser_sym_global_count], rax
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
	mov rax, [parser_sym_global_count]
	mov [parser_sym_count], rax
	mov qword [parser_local_bytes], 0
	; reset per-function if label counter
	mov qword [parser_if_count], 0
	; reset per-function while label counter
	mov qword [parser_while_count], 0
	; reset per-function loop stack
	mov qword [parser_loop_sp], 0

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
	;   var name;     -> emit into .data: <name>: dq 0
	;   var name[N];  -> emit into .bss:  <name>: resb <N>
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

parser_parse_const_decl:
	; top-level:
	;   const NAME = INT;
	mov rdi, TOK_KW_CONST
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	push r14
	push r15
	mov r14, [lexer_tok_ptr]
	mov r15, [lexer_tok_len]
	call lexer_next

	mov rdi, TOK_EQ
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_INT
	jne parser_err_expected_int
	mov rdi, [lexer_tok_ptr]
	mov rsi, [lexer_tok_len]
	call util_parse_u64
	mov rcx, rax
	call lexer_next

	mov rdi, TOK_SEMI
	call lexer_expect

	mov rdi, r14
	mov rsi, r15
	mov rdx, SYM_CONST
	call parser_sym_add

	pop r15
	pop r14
	ret

parser_parse_layout_decl:
	; top-level:
	;   layout Name { ptr8 a; ptr16 b; ptr32 c; ptr64 d; }
	mov rdi, TOK_KW_LAYOUT
	call lexer_expect
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	push r14
	push r15
	mov r14, [lexer_tok_ptr]   ; layout name
	mov r15, [lexer_tok_len]
	call lexer_next

	mov rdi, TOK_LBRACE
	call lexer_expect

	xor r12d, r12d             ; offset
.member_loop:
	call lexer_peek
	cmp rax, TOK_RBRACE
	je .done_members

	; type keyword
	mov r13, 0                 ; size
	cmp rax, TOK_KW_PTR8
	je .sz8
	cmp rax, TOK_KW_PTR16
	je .sz16
	cmp rax, TOK_KW_PTR32
	je .sz32
	cmp rax, TOK_KW_PTR64
	je .sz64
	jmp parser_err_expected_stmt
.sz8:
	mov r13, 1
	jmp .after_type
.sz16:
	mov r13, 2
	jmp .after_type
.sz32:
	mov r13, 4
	jmp .after_type
.sz64:
	mov r13, 8
.after_type:
	call lexer_next
	call lexer_peek
	cmp rax, TOK_IDENT
	jne parser_err_expected_ident
	mov r8, [lexer_tok_ptr]    ; member name
	mov r9, [lexer_tok_len]
	call lexer_next
	push r8
	push r9
	mov rdi, TOK_SEMI
	call lexer_expect
	pop r9
	pop r8

	; name = <Layout>_<member>
	mov rdi, r14
	mov rsi, r15
	mov rdx, r8
	mov rcx, r9
	call parser_make_layout_member_name
	; rax=name_ptr, rdx=name_len
	push rdx
	push rax
	pop rdi
	pop rsi
	mov rdx, SYM_CONST
	mov rcx, r12
	call parser_sym_add

	add r12, r13
	jmp .member_loop

.done_members:
	mov rdi, TOK_RBRACE
	call lexer_expect

	; also define <Layout>_SIZE
	mov rdi, r14
	mov rsi, r15
	call parser_make_layout_size_name
	; rax=name_ptr, rdx=name_len
	push rdx
	push rax
	pop rdi
	pop rsi
	mov rdx, SYM_CONST
	mov rcx, r12
	call parser_sym_add

	pop r15
	pop r14
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

parser_err_expected_asm_str:
	lea rdi, [rel parser_msg_expected_asm_str]
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

parser_err_break_outside_loop:
	lea rdi, [rel parser_msg_break_outside_loop]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

parser_err_continue_outside_loop:
	lea rdi, [rel parser_msg_continue_outside_loop]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

section .rodata
parser_msg_expected_main: db "parse error: expected function name 'main'",10,0
parser_msg_expected_ident: db "parse error: expected identifier",10,0
parser_msg_expected_eof: db "parse error: expected end of file",10,0
parser_msg_too_many_args: db "parse error: too many args (max 6)",10,0
parser_msg_expected_stmt: db "parse error: expected statement",10,0
parser_msg_too_many_locals: db "parse error: too many symbols (max 256)",10,0

parser_msg_expected_int: db "parse error: expected integer",10,0

parser_msg_expected_asm_str: db "parse error: expected string literal inside asm{...}",10,0

parser_msg_cmp_only_in_if: db "parse error: comparisons are only allowed inside if/while",10,0
parser_msg_if_lhs_must_be_reg: db "parse error: if lhs must be a register",10,0
parser_msg_expected_if_cmp: db "parse error: expected comparison operator in condition",10,0
parser_msg_expected_if_rhs: db "parse error: expected int or reg on rhs of if",10,0

parser_msg_store64_imm_too_big: db "parse error: ptr64 store immediate must fit in signed 32-bit; load into a register first",10,0
parser_msg_expected_addr: db "parse error: expected address inside ptr[]",10,0
parser_msg_break_outside_loop: db "parse error: break used outside of while",10,0
parser_msg_continue_outside_loop: db "parse error: continue used outside of while",10,0

section .bss
parser_seen_main:    resq 1
parser_local_bytes: resq 1
parser_num_buf:     resb 64
