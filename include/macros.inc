; Common NASM macros

%macro PUSH_R12_R15 0
	push r12
	push r13
	push r14
	push r15
%endmacro

%macro POP_R15_R12 0
	pop r15
	pop r14
	pop r13
	pop r12
%endmacro
