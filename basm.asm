; Basm - Stage 1 (skeleton)
; Main entrypoint. Modules are stubbed for now.

BITS 64
DEFAULT REL

global _start

%include "include/consts.inc"
%include "include/macros.inc"

%include "src/cli.inc"
%include "src/lexer.inc"
%include "src/parser.inc"
%include "src/ir.inc"
%include "src/backend.inc"
%include "src/elf64.inc"
%include "src/util.inc"

section .text

_start:
    ; TODO: parse argv, compile source.bpp to output.asm
    ; For now, exit(0) so the toolchain wiring is valid.
    mov rax, SYS_exit
    xor rdi, rdi
    syscall
