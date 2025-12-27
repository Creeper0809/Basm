; Utilities

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

; util_u64_to_dec
;   Converts unsigned integer to decimal ASCII (no NUL terminator required).
;
; Inputs:
;   rdi = value
;   rsi = end pointer (buffer end, write backwards)
; Outputs:
;   rax = start pointer
;   rdx = length
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
