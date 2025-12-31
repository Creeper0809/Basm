func main() {
  asm {
    "mov rdi, 777\n"
    "call print_dec\n"
  };
}
