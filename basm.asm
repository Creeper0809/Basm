BITS 64
DEFAULT REL

global _start

%define FILE_BUF_MAX 1048576

%include "include/consts.inc"
%include "include/macros.inc"

%include "src/cli.inc"
%include "src/lexer.inc"
%include "src/parser.inc"
%include "src/emitter.inc"
%include "src/util.inc"

section .text

_start:
    mov rdi, [rsp]        ; argc
    lea rsi, [rsp + 8]    ; argv
    call cli_parse

    ; read input file
    call cli_get_input_path
    mov rdi, rax                  ; path
    lea rsi, [rel file_buf]       ; buf
    mov rdx, FILE_BUF_MAX         ; max
    call util_read_file
    mov r12, rax                  ; len

    ; lexer sanity check
    lea rdi, [rel file_buf]
    mov rsi, r12
    call lexer_validate

    ; reset emitter and parse-and-emit
    call emit_reset
    lea rdi, [rel file_buf]
    mov rsi, r12
    call parse_program

    ; optionally write output.asm if -o was provided
    call cli_get_output_path
    test rax, rax
    jz .skip_file_output

    mov rdi, rax
    call util_open_write_trunc
    mov r13, rax                  ; fd

    mov rdi, r13
    call emit_flush

    mov rdi, r13
    call util_close

.skip_file_output:

    ; also print generated asm to stdout
    mov rdi, FD_stdout
    call emit_flush

    xor rdi, rdi
    call util_exit

section .bss
file_buf: resb FILE_BUF_MAX
