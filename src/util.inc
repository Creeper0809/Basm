section .text

; rdi = exit_code
util_exit:
	mov rax, SYS_exit
	syscall

; rdi = fd, rsi = buf, rdx = len
; writes until all bytes written or fatal
util_write_all:
	push rbp
	mov rbp, rsp
	push rbx

	mov rbx, rdx            ; remaining
.loop:
	cmp rbx, 0
	je .done

	mov rax, SYS_write
	syscall
	; rax < 0 => error
	test rax, rax
	js .err
	cmp rax, 0
	je .err

	add rsi, rax
	sub rbx, rax
	mov rdx, rbx
	jmp .loop

.err:
	lea rdi, [rel util_msg_write_fail]
	call util_fatal

.done:
	pop rbx
	pop rbp
	ret

; rdi = zero-terminated string
; returns rax = length (not including 0)
util_strlen:
	xor rax, rax
.l:
	cmp byte [rdi + rax], 0
	je .done
	inc rax
	jmp .l
.done:
	ret

; rdi = a, rsi = b (both 0-terminated)
; returns al = 1 if equal else 0
util_streq:
	xor eax, eax
.loop:
	mov r8b, [rdi]
	mov r9b, [rsi]
	cmp r8b, r9b
	jne .no
	cmp r8b, 0
	je .yes
	inc rdi
	inc rsi
	jmp .loop
.yes:
	mov al, 1
	ret
.no:
	xor al, al
	ret

; rdi = zero-terminated message (written to stderr), exits(1)
util_fatal:
	push rdi
	call util_strlen
	pop rsi                 ; msg
	mov rdi, FD_stderr
	mov rdx, rax
	call util_write_all
	mov rdi, 1
	call util_exit

; rdi = zero-terminated message, rsi = 1-based line number
; prints: "error at line <n>: <msg>" then exits(1)
util_fatal_line:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rdi            ; msg
	mov r13, rsi            ; line

	; prefix
	lea rdi, [rel util_msg_line_prefix]
	call util_strlen
	mov rdi, FD_stderr
	lea rsi, [rel util_msg_line_prefix]
	mov rdx, rax
	call util_write_all

	; line number
	mov rdi, r13
	lea rsi, [rel util_line_num_buf + 32]
	call util_u64_to_dec
	; rax=start, rdx=len
	mov rdi, FD_stderr
	mov rsi, rax
	call util_write_all

	; ": "
	mov rdi, FD_stderr
	lea rsi, [rel util_msg_line_mid]
	mov rdx, util_msg_line_mid_len
	call util_write_all

	; message
	mov rdi, r12
	call util_strlen
	mov rdi, FD_stderr
	mov rsi, r12
	mov rdx, rax
	call util_write_all

	mov rdi, 1
	call util_exit

util_print_usage:
	lea rdi, [rel util_msg_usage]
	call util_print_stderr
	ret

; rdi = zero-terminated string
util_print_stderr:
	push rdi
	call util_strlen
	pop rsi
	mov rdi, FD_stderr
	mov rdx, rax
	call util_write_all
	ret

; rdi = fd
util_close:
	mov rax, SYS_close
	syscall
	ret

util_u64_to_dec:
	push rbp
	mov rbp, rsp
	push r8
	push r9
	push r10
	push r11

	mov rax, rdi
	mov r8, rsi           ; write ptr
	xor r9d, r9d          ; len
	mov r11, 10

	cmp rax, 0
	jne .loop
	dec r8
	mov byte [r8], '0'
	mov r9, 1
	jmp .done

.loop:
	xor rdx, rdx
	div r11               ; rax=quotient, rdx=remainder
	add dl, '0'
	dec r8
	mov [r8], dl
	inc r9
	test rax, rax
	jnz .loop

.done:
	mov rax, r8
	mov rdx, r9

	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	ret

; rdi = ptr, rsi = len
; returns rax = parsed unsigned integer
; supports decimal and 0x/0X-prefixed hex
; NOTE: assumes input is a valid TOK_INT slice (lexer guarantees format).
util_parse_u64:
	push rbp
	mov rbp, rsp
	push r12
	push r13
	push r14
	push r15

	mov r12, rdi          ; ptr
	mov r13, rsi          ; len
	xor eax, eax          ; acc

	; default base = 10
	mov r14d, 10

	; detect 0x / 0X
	cmp r13, 2
	jb .loop
	cmp byte [r12 + 0], '0'
	jne .loop
	mov dl, [r12 + 1]
	cmp dl, 'x'
	je .hex
	cmp dl, 'X'
	jne .loop
.hex:
	mov r14d, 16
	add r12, 2
	sub r13, 2

.loop:
	test r13, r13
	jz .done
	mov dl, [r12]

	cmp r14d, 10
	je .digit_dec

	; hex digit
	cmp dl, '0'
	jb .done
	cmp dl, '9'
	jbe .hex_num
	cmp dl, 'A'
	jb .hex_chk2
	cmp dl, 'F'
	jbe .hex_upper
.hex_chk2:
	cmp dl, 'a'
	jb .done
	cmp dl, 'f'
	ja .done
	; lower a-f
	sub dl, 'a'
	add dl, 10
	jmp .hex_acc
.hex_upper:
	sub dl, 'A'
	add dl, 10
	jmp .hex_acc
.hex_num:
	sub dl, '0'
.hex_acc:
	; acc = acc*16 + dl
	shl rax, 4
	movzx r15, dl
	add rax, r15
	jmp .adv

.digit_dec:
	; decimal digit
	cmp dl, '0'
	jb .done
	cmp dl, '9'
	ja .done
	sub dl, '0'
	; acc = acc*10 + dl
	lea rax, [rax*4 + rax]   ; *5
	lea rax, [rax*2]         ; *10
	movzx r15, dl
	add rax, r15

.adv:
	inc r12
	dec r13
	jmp .loop

.done:
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	ret

; rdi = path
; returns rax = fd or fatal
util_open_readonly:
	mov rax, SYS_open
	mov rsi, O_RDONLY
	xor rdx, rdx
	syscall
	test rax, rax
	js .err
	ret
.err:
	lea rdi, [rel util_msg_open_fail]
	call util_fatal

; rdi = path
; returns rax = fd or fatal
util_open_write_trunc:
	mov rax, SYS_open
	mov rsi, O_WRONLY | O_CREAT | O_TRUNC
	mov rdx, MODE_0644
	syscall
	test rax, rax
	js .err
	ret
.err:
	lea rdi, [rel util_msg_open_fail]
	call util_fatal

; rdi = path, rsi = buf, rdx = max
; returns rax = bytes_read or fatal
util_read_file:
	push rbp
	mov rbp, rsp
	push r12
	push r13

	mov r12, rsi            ; buf
	mov r13, rdx            ; max

	call util_open_readonly
	mov r8, rax             ; fd

	mov rax, SYS_read
	mov rdi, r8
	mov rsi, r12
	mov rdx, r13
	syscall
	test rax, rax
	js .read_err

	mov r9, rax             ; bytes_read

	mov rdi, r8
	call util_close
	mov rax, r9

	pop r13
	pop r12
	pop rbp
	ret

.read_err:
	lea rdi, [rel util_msg_read_fail]
	call util_fatal

section .rodata
util_msg_usage: db "usage: basm <input.b> [-o <output.asm>]",10,0
util_msg_open_fail: db "error: failed to open file",10,0
util_msg_read_fail: db "error: failed to read file",10,0
util_msg_write_fail: db "error: failed to write file",10,0

util_msg_line_prefix: db "error at line ",0
util_msg_line_mid: db ": ",0
util_msg_line_mid_len: equ 2

section .bss
util_line_num_buf: resb 32
