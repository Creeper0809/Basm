; --- Linux x86_64 syscalls ---
%define SYS_read   0
%define SYS_write  1
%define SYS_open   2
%define SYS_close  3
%define SYS_exit 60

; --- File descriptors ---
%define FD_stdin   0
%define FD_stdout  1
%define FD_stderr  2

; --- open(2) flags ---
%define O_RDONLY   0
%define O_WRONLY   1
%define O_CREAT    64
%define O_TRUNC    512

%define MODE_0644  420
