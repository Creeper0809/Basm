// Smoke Test 4-ish: "cat" a real file via syscall wrappers.
// Prints the repository README.md to stdout.

func main() {
  alias r12 : buf;
  alias r13 : fd;
  alias rbx : n;

  // allocate buffer
  heap_alloc(4096);
  buf = rax;

  // open README.md (O_RDONLY=0)
  sys_open("README.md", 0, 0);
  fd = rax;

  // if (fd < 0) exit(1);
  if (fd < 0) {
    sys_exit(1);
  }

  n = 1;
  while (n > 0) {
    sys_read(fd, buf, 4096);
    n = rax;
    if (n <= 0) { break; }
    sys_write(1, buf, n);
  }

  sys_close(fd);
}
