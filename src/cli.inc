section .bss
cli_input_path:  resq 1
cli_output_path: resq 1

section .text

cli_get_input_path:
	mov rax, [cli_input_path]
	ret

cli_get_output_path:
	mov rax, [cli_output_path]
	ret

; rdi = argc, rsi = argv (pointer to argv[0] pointer)
cli_parse:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r13, rdi                   ; argc
	mov r12, rsi                   ; argv base

	mov qword [cli_input_path], 0
	mov qword [cli_output_path], 0

	; argc must be >= 2
	cmp r13, 2
	jb .usage

	; input path is argv[1]
	mov rax, [r12 + 8]
	mov [cli_input_path], rax

	; parse remaining args
	mov rcx, 2                     ; i
.loop:
	cmp rcx, r13
	jae .done

	mov rbx, [r12 + rcx*8]         ; arg

	; if arg == "-o"
	lea rsi, [rel .str_o]
	mov rdi, rbx
	call util_streq
	test al, al
	jz .unknown

	; need next arg for output
	lea rax, [rcx + 1]
	cmp rax, r13
	jae .usage

	mov rax, [r12 + (rcx+1)*8]
	mov [cli_output_path], rax
	add rcx, 2
	jmp .loop

.unknown:
	; unknown argument
	lea rdi, [rel .msg_unknown]
	call util_fatal

.done:
	pop r13
	pop r12
	pop rbp
	ret

.usage:
	call util_print_usage
	mov rdi, 1
	call util_exit

section .rodata
.str_o: db "-o", 0
.msg_unknown: db "unknown argument", 10, 0
