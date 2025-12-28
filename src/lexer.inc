section .text

; ================================
; Stage 1 lexer (tokenizer)
;
; Rules:
; - Whitespace and '//' comments are consumed here.
; - Line counting: increment on '\n' (0x0A), treat '\r' (0x0D) as whitespace.
; - Parser sees only meaningful tokens.
;
; Token record (in .bss):
;   lexer_tok_kind  : qword
;   lexer_tok_ptr   : qword
;   lexer_tok_len   : qword
;   lexer_tok_line  : qword (1-based)

%define TOK_EOF      0
%define TOK_IDENT    1
%define TOK_KW_FUNC  2
%define TOK_KW_VAR   3
%define TOK_LPAREN   4
%define TOK_RPAREN   5
%define TOK_LBRACE   6
%define TOK_RBRACE   7
%define TOK_SEMI     8
%define TOK_COMMA    9
%define TOK_EQ      10
%define TOK_INT     11
%define TOK_REG     12
%define TOK_LBRACK  13
%define TOK_RBRACK  14
%define TOK_PLUS    15
%define TOK_MINUS   16
%define TOK_KW_PTR8 17
%define TOK_KW_PTR64 18
%define TOK_STR    19

section .bss
lexer_buf_ptr:  resq 1
lexer_buf_len:  resq 1
lexer_idx:      resq 1
lexer_line:     resq 1
lexer_bol:      resq 1

lexer_tok_kind: resq 1
lexer_tok_ptr:  resq 1
lexer_tok_len:  resq 1
lexer_tok_line: resq 1

section .text

; rdi=buf_ptr, rsi=buf_len
lexer_init:
	mov [lexer_buf_ptr], rdi
	mov [lexer_buf_len], rsi
	mov qword [lexer_idx], 0
	mov qword [lexer_line], 1
	mov qword [lexer_bol], 1
	call lexer_next
	ret

; returns rax = current kind
lexer_peek:
	mov rax, [lexer_tok_kind]
	ret

; advances and returns rax = new current kind
lexer_next:
	call lexer_skip_ws_and_comments

	; EOF?
	mov rcx, [lexer_idx]
	mov rdx, [lexer_buf_len]
	cmp rcx, rdx
	jae lexer_make_eof

	mov rdi, [lexer_buf_ptr]
	mov al, [rdi + rcx]

	; single-char punctuation
	cmp al, '('
	je lexer_make_lparen
	cmp al, ')'
	je lexer_make_rparen
	cmp al, '{'
	je lexer_make_lbrace
	cmp al, '}'
	je lexer_make_rbrace
	cmp al, ';'
	je lexer_make_semi
	cmp al, ','
	je lexer_make_comma
	cmp al, '='
	je lexer_make_eq
	cmp al, '['
	je lexer_make_lbrack
	cmp al, ']'
	je lexer_make_rbrack
	cmp al, '+'
	je lexer_make_plus
	cmp al, '-'
	je lexer_make_minus

	; string literal
	cmp al, '"'
	je lexer_read_str

	; integer literal
	cmp al, '0'
	jb .ident_start
	cmp al, '9'
	jbe lexer_read_int

	; identifier / keyword
	; allow '_' as identifier start
.ident_start:
	cmp al, '_'
	je lexer_read_ident
	cmp al, 'A'
	jb lexer_err_bad_char
	cmp al, 'Z'
	jbe lexer_read_ident
	cmp al, 'a'
	jb lexer_err_bad_char
	cmp al, 'z'
	jbe lexer_read_ident

	jmp lexer_err_bad_char

lexer_read_str:
	; token slice excludes surrounding quotes, but includes escape sequences
	; errors on raw newline or unknown escape
	mov r8, [lexer_idx]          ; points at '"'
	mov r9, [lexer_line]
	mov rdi, [lexer_buf_ptr]
	mov r10, [lexer_buf_len]
	inc r8                      ; start after opening quote
	mov rcx, r8                 ; scan index

.loop:
	cmp rcx, r10
	jae lexer_err_bad_char
	mov al, [rdi + rcx]
	cmp al, 10                  ; raw newline not allowed
	je lexer_err_bad_char
	cmp al, '"'
	je .done
	cmp al, 92
	jne .adv
	; escape sequence
	lea r11, [rcx + 1]
	cmp r11, r10
	jae lexer_err_bad_char
	mov al, [rdi + r11]
	cmp al, 92
	je .esc_ok
	cmp al, '"'
	je .esc_ok
	cmp al, 'n'
	je .esc_ok
	cmp al, 't'
	je .esc_ok
	jmp lexer_err_bad_char
.esc_ok:
	add rcx, 2
	jmp .loop

.adv:
	inc rcx
	jmp .loop

.done:
	; rcx points at closing quote
	mov [lexer_idx], rcx
	inc qword [lexer_idx]       ; consume closing quote
	mov qword [lexer_bol], 0
	; slice: [start=r8, end=rcx)
	mov rax, [lexer_buf_ptr]
	add rax, r8
	mov [lexer_tok_ptr], rax
	mov rdx, rcx
	sub rdx, r8
	mov [lexer_tok_len], rdx
	mov [lexer_tok_line], r9
	mov qword [lexer_tok_kind], TOK_STR
	mov rax, TOK_STR
	ret

; rdi=expected kind
lexer_expect:
	push r12
	mov r12, rdi
	call lexer_peek
	cmp rax, r12
	jne .err
	call lexer_next
	pop r12
	ret
.err:
	lea rdi, [rel lexer_msg_unexpected]
	mov rsi, [lexer_tok_line]
	call util_fatal_line

lexer_make_eof:
	mov qword [lexer_tok_kind], TOK_EOF
	mov qword [lexer_tok_len], 0
	mov rax, [lexer_buf_ptr]
	add rax, [lexer_idx]
	mov [lexer_tok_ptr], rax
	mov rax, [lexer_line]
	mov [lexer_tok_line], rax
	mov rax, TOK_EOF
	ret

lexer_make_lparen:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_LPAREN
	mov rax, TOK_LPAREN
	ret
lexer_make_rparen:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_RPAREN
	mov rax, TOK_RPAREN
	ret
lexer_make_lbrace:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_LBRACE
	mov rax, TOK_LBRACE
	ret
lexer_make_rbrace:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_RBRACE
	mov rax, TOK_RBRACE
	ret
lexer_make_semi:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_SEMI
	mov rax, TOK_SEMI
	ret
lexer_make_comma:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_COMMA
	mov rax, TOK_COMMA
	ret

lexer_make_eq:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_EQ
	mov rax, TOK_EQ
	ret

lexer_make_lbrack:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_LBRACK
	mov rax, TOK_LBRACK
	ret

lexer_make_rbrack:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_RBRACK
	mov rax, TOK_RBRACK
	ret

lexer_make_plus:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_PLUS
	mov rax, TOK_PLUS
	ret

lexer_make_minus:
	call lexer_make_punct
	mov qword [lexer_tok_kind], TOK_MINUS
	mov rax, TOK_MINUS
	ret

; helper: create a 1-char token from current idx
lexer_make_punct:
	mov rcx, [lexer_idx]
	inc rcx
	mov [lexer_idx], rcx
	mov qword [lexer_bol], 0
	mov qword [lexer_tok_len], 1
	mov rax, [lexer_buf_ptr]
	add rax, rcx
	dec rax
	mov [lexer_tok_ptr], rax
	mov rax, [lexer_line]
	mov [lexer_tok_line], rax
	ret

lexer_read_ident:
	mov r8, [lexer_idx]          ; start
	mov r9, [lexer_line]

	mov rdi, [lexer_buf_ptr]
	mov r10, [lexer_buf_len]

.loop:
	cmp r8, r10
	jae .done
	mov al, [rdi + r8]
	cmp al, 'A'
	jb .chk2
	cmp al, 'Z'
	jbe .adv
.chk2:
	cmp al, 'a'
	jb .chk3
	cmp al, 'z'
	jbe .adv
.chk3:
	cmp al, '0'
	jb .chk4
	cmp al, '9'
	jbe .adv
.chk4:
	cmp al, '_'
	je .adv
	jmp .done
.adv:
	inc r8
	jmp .loop

.done:
	; token slice: [lexer_idx, r8)
	mov rcx, [lexer_idx]
	mov [lexer_idx], r8
	mov qword [lexer_bol], 0
	mov rax, [lexer_buf_ptr]
	add rax, rcx
	mov [lexer_tok_ptr], rax
	mov rdx, r8
	sub rdx, rcx
	mov [lexer_tok_len], rdx
	mov [lexer_tok_line], r9

	; keywords: func / var
	; compare len first
	cmp rdx, 4
	jne .not_kw
	; possible "func"
	cmp byte [rax + 0], 'f'
	jne .not_kw
	cmp byte [rax + 1], 'u'
	jne .not_kw
	cmp byte [rax + 2], 'n'
	jne .not_kw
	cmp byte [rax + 3], 'c'
	jne .not_kw
	mov qword [lexer_tok_kind], TOK_KW_FUNC
	mov rax, TOK_KW_FUNC
	ret

.not_kw:
	cmp rdx, 3
	jne .ident
	cmp byte [rax + 0], 'v'
	jne .ident
	cmp byte [rax + 1], 'a'
	jne .ident
	cmp byte [rax + 2], 'r'
	jne .ident
	mov qword [lexer_tok_kind], TOK_KW_VAR
	mov rax, TOK_KW_VAR
	ret

.ident:
	; keywords: ptr8
	cmp rdx, 4
	jne .maybe_ptr64
	cmp byte [rax + 0], 'p'
	jne .ident2
	cmp byte [rax + 1], 't'
	jne .ident2
	cmp byte [rax + 2], 'r'
	jne .ident2
	cmp byte [rax + 3], '8'
	jne .ident2
	mov qword [lexer_tok_kind], TOK_KW_PTR8
	mov rax, TOK_KW_PTR8
	ret

.maybe_ptr64:
	cmp rdx, 5
	jne .ident2
	cmp byte [rax + 0], 'p'
	jne .ident2
	cmp byte [rax + 1], 't'
	jne .ident2
	cmp byte [rax + 2], 'r'
	jne .ident2
	cmp byte [rax + 3], '6'
	jne .ident2
	cmp byte [rax + 4], '4'
	jne .ident2
	mov qword [lexer_tok_kind], TOK_KW_PTR64
	mov rax, TOK_KW_PTR64
	ret

.ident2:
	; registers: rax..r15
	call lexer_classify_reg
	cmp rax, 1
	je .is_reg
	mov qword [lexer_tok_kind], TOK_IDENT
	mov rax, TOK_IDENT
	ret

.is_reg:
	mov qword [lexer_tok_kind], TOK_REG
	mov rax, TOK_REG
	ret

; returns rax=1 if current token slice is a valid reg name else 0
lexer_classify_reg:
	; input: lexer_tok_ptr/len already set
	mov rbx, [lexer_tok_ptr]
	mov rcx, [lexer_tok_len]
	cmp rcx, 2
	je .len2
	cmp rcx, 3
	je .len3
	xor rax, rax
	ret
.len2:
	; r8, r9
	cmp byte [rbx + 0], 'r'
	jne .no
	mov al, [rbx + 1]
	cmp al, '8'
	je .yes
	cmp al, '9'
	je .yes
	xor rax, rax
	ret
.len3:
	cmp byte [rbx + 0], 'r'
	jne .no
	; rax rbx rcx rdx rsi rdi rsp rbp
	cmp byte [rbx + 1], 'a'
	je .chk_x
	cmp byte [rbx + 1], 'b'
	je .chk_b
	cmp byte [rbx + 1], 'c'
	je .chk_x
	cmp byte [rbx + 1], 'd'
	je .chk_x_or_i
	cmp byte [rbx + 1], 's'
	je .chk_s
	; r10..r15
	cmp byte [rbx + 1], '1'
	jne .no
	mov al, [rbx + 2]
	cmp al, '0'
	jb .no
	cmp al, '5'
	ja .no
	jmp .yes

.chk_x:
	cmp byte [rbx + 2], 'x'
	je .yes
	jmp .no

.chk_b:
	cmp byte [rbx + 2], 'x'
	je .yes
	cmp byte [rbx + 2], 'p'
	je .yes
	jmp .no

.chk_x_or_i:
	cmp byte [rbx + 2], 'x'
	je .yes
	cmp byte [rbx + 2], 'i'
	je .yes
	jmp .no

.chk_s:
	cmp byte [rbx + 2], 'i'
	je .yes
	cmp byte [rbx + 2], 'p'
	je .yes
	jmp .no
.yes:
	mov rax, 1
	ret
.no:
	xor rax, rax
	ret

lexer_read_int:
	mov r8, [lexer_idx]          ; start
	mov r9, [lexer_line]
	mov rdi, [lexer_buf_ptr]
	mov r10, [lexer_buf_len]
	; support hex: 0x[0-9a-fA-F]+
	mov al, [rdi + r8]
	cmp al, '0'
	jne .dec
	lea r11, [r8 + 1]
	cmp r11, r10
	jae .dec
	mov al, [rdi + r11]
	cmp al, 'x'
	je .hex
	cmp al, 'X'
	je .hex
	jmp .dec

.hex:
	; consume '0x'
	add r8, 2
.hex_loop:
	cmp r8, r10
	jae .int_done
	mov al, [rdi + r8]
	; 0-9
	cmp al, '0'
	jb .int_done
	cmp al, '9'
	jbe .hex_adv
	; A-F
	cmp al, 'A'
	jb .hex_chk2
	cmp al, 'F'
	jbe .hex_adv
.hex_chk2:
	; a-f
	cmp al, 'a'
	jb .int_done
	cmp al, 'f'
	ja .int_done
.hex_adv:
	inc r8
	jmp .hex_loop

.dec:
.int_loop:
	cmp r8, r10
	jae .int_done
	mov al, [rdi + r8]
	cmp al, '0'
	jb .int_done
	cmp al, '9'
	ja .int_done
	inc r8
	jmp .int_loop
.int_done:
	mov rcx, [lexer_idx]
	mov [lexer_idx], r8
	mov qword [lexer_bol], 0
	mov rax, [lexer_buf_ptr]
	add rax, rcx
	mov [lexer_tok_ptr], rax
	mov rdx, r8
	sub rdx, rcx
	mov [lexer_tok_len], rdx
	mov [lexer_tok_line], r9
	mov qword [lexer_tok_kind], TOK_INT
	mov rax, TOK_INT
	ret

lexer_skip_ws_and_comments:
	mov rdi, [lexer_buf_ptr]
	mov rsi, [lexer_buf_len]
	mov rcx, [lexer_idx]
	mov rbx, [lexer_bol]

.ws_loop:
	cmp rcx, rsi
	jae .done
	mov al, [rdi + rcx]

	; comment begins with '//' anywhere
	; comment begins with '//' anywhere
	cmp al, '/'
	jne .ws
	lea r8, [rcx + 1]
	cmp r8, rsi
	jae .done
	cmp byte [rdi + rcx + 1], '/'
	je .skip_comment_slash
	jmp .done


.ws:
	; whitespace
	cmp al, ' '
	je .adv
	cmp al, 9
	je .adv
	cmp al, 13               ; '\r' ignored
	je .adv
	cmp al, 10               ; '\n'
	je .nl
	jmp .done

.nl:
	inc rcx
	mov rax, [lexer_line]
	inc rax
	mov [lexer_line], rax
	mov rbx, 1
	jmp .ws_loop

.adv:
	inc rcx
	jmp .ws_loop
.skip_comment_slash:
	; consume the two slashes
	add rcx, 2
.c2_loop:
	cmp rcx, rsi
	jae .done
	mov al, [rdi + rcx]
	inc rcx
	cmp al, 10
	jne .c2_loop
	; count newline in comment
	mov rax, [lexer_line]
	inc rax
	mov [lexer_line], rax
	mov rbx, 1
	jmp .ws_loop

.done:
	mov [lexer_idx], rcx
	mov [lexer_bol], rbx
	ret

lexer_err_bad_char:
	lea rdi, [rel lexer_msg_bad_char]
	mov rsi, [lexer_line]
	call util_fatal_line

lexer_validate:
	cmp rsi, 0
	jne .scan
	lea rdi, [rel .msg_empty]
	call util_fatal

.scan:
	xor rcx, rcx
.loop:
	cmp rcx, rsi
	jae .ok

	mov al, [rdi + rcx]

	; allow: tab(9), newline(10), carriage return(13)
	cmp al, 9
	je .next
	cmp al, 10
	je .next
	cmp al, 13
	je .next

	; allow: ASCII printable range 0x20..0x7E
	cmp al, 0x20
	jb .bad
	cmp al, 0x7E
	ja .bad

.next:
	inc rcx
	jmp .loop

.bad:
	lea rdi, [rel .msg_bad_byte]
	call util_fatal

.ok:
	ret

section .rodata
.msg_empty: db "lexer error: empty input",10,0
.msg_bad_byte: db "lexer error: invalid byte (non-ascii/control)",10,0

lexer_msg_bad_char: db "lexer error: invalid character",10,0
lexer_msg_unexpected: db "parse error: unexpected token",10,0
