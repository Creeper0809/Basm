section .text

parser_skip_ws:
.ws_loop:
	cmp rcx, rsi
	jae .ws_done
	mov al, [rdi + rcx]
	cmp al, ';'
	jne .not_comment

	cmp rcx, 0
	je .skip_comment
	mov r11b, [rdi + rcx - 1]
	cmp r11b, 10
	je .skip_comment
	cmp r11b, 13
	je .skip_comment
    
	jmp .ws_done

.not_comment:
	cmp al, ' '
	je .ws_advance
	cmp al, 9
	je .ws_advance
	cmp al, 10
	je .ws_advance
	cmp al, 13
	je .ws_advance
	jmp .ws_done

.skip_comment:
.c_loop:
	cmp rcx, rsi
	jae .ws_done
	mov al, [rdi + rcx]
	inc rcx
	cmp al, 10
	jne .c_loop
	jmp .ws_loop

.ws_advance:
	inc rcx
	jmp .ws_loop

.ws_done:
	ret

parse_program:
	mov r14, rdi
	mov r15, rsi
	xor rcx, rcx
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws

	; --- Firstable hardcoded ---
	lea r9, [rcx + 3]
	cmp r9, r15
	ja .err
	cmp byte [r14 + rcx + 0], 'e'
	jne .err
	cmp byte [r14 + rcx + 1], 'a'
	jne .err
	cmp byte [r14 + rcx + 2], 'x'
	jne .err
	add rcx, 3
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jae .err
	cmp byte [r14 + rcx], '='
	jne .err
	inc rcx
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jae .err

	xor rax, rax              ; value
	xor rdx, rdx              ; digits
.digits:
	lea r9, [rcx]
	cmp r9, r15
	jae .after_digits
	mov r11b, [r14 + r9]
	cmp r11b, '0'
	jb .after_digits
	cmp r11b, '9'
	ja .after_digits

	cmp rax, 429496729
	ja .err
	jne .mul

	; if value == 429496729 next digit must be <= 5
	mov r10b, r11b
	sub r10b, '0'
	cmp r10b, 5
	ja .err
.mul:
	imul rax, rax, 10
	movzx r10, r11b
	sub r10, '0'
	add rax, r10

	inc rcx
	inc rdx
	jmp .digits

.after_digits:
	cmp rdx, 0
	je .err

	; store into IR
	mov rdi, rax
	call ir_set_eax_imm

	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jae .err
	cmp byte [r14 + rcx], ';'
	jne .err
	inc rcx
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws

	; if line end then no print
	cmp rcx, r15
	je .ok

	lea r9, [rcx + 5]
	cmp r9, r15
	ja .err
	cmp byte [r14 + rcx + 0], 'p'
	jne .err
	cmp byte [r14 + rcx + 1], 'r'
	jne .err
	cmp byte [r14 + rcx + 2], 'i'
	jne .err
	cmp byte [r14 + rcx + 3], 'n'
	jne .err
	cmp byte [r14 + rcx + 4], 't'
	jne .err
	add rcx, 5
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jae .err
	cmp byte [r14 + rcx], '('
	jne .err
	inc rcx
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws

	lea r9, [rcx + 3]
	cmp r9, r15
	ja .err
	cmp byte [r14 + rcx + 0], 'e'
	jne .err
	cmp byte [r14 + rcx + 1], 'a'
	jne .err
	cmp byte [r14 + rcx + 2], 'x'
	jne .err
	add rcx, 3
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jae .err
	cmp byte [r14 + rcx], ')'
	jne .err
	inc rcx
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jae .err
	cmp byte [r14 + rcx], ';'
	jne .err
	inc rcx
	mov rdi, r14
	mov rsi, r15
	call parser_skip_ws
	cmp rcx, r15
	jne .err

	call ir_set_print_eax

.ok:
	ret

.err:
	lea rdi, [rel parser_msg_prog]
	call util_fatal

section .rodata
parser_msg_prog: db "parse error: expected 'eax = <number>;' and optional 'print(eax);'", 10, 0
