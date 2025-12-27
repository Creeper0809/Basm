section .bss
ir_has_eax_imm:   resb 1
ir_do_print_eax:  resb 1
ir_eax_imm_u64:   resq 1

section .text

ir_reset:
	mov byte [ir_has_eax_imm], 0
	mov byte [ir_do_print_eax], 0
	mov qword [ir_eax_imm_u64], 0
	ret

ir_set_eax_imm:
	mov byte [ir_has_eax_imm], 1
	mov [ir_eax_imm_u64], rdi
	ret

ir_set_print_eax:
	mov byte [ir_do_print_eax], 1
	ret
