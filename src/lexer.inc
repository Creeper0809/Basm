section .text

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
