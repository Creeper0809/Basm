func main() {

    // heap_alloc(size) returns pointer in RAX.
    alias rax : tmp;
    alias r12 : p;

    heap_alloc(8);
	p = tmp;

    // Write "Hi\n\0" into allocated buffer
    ptr8[p + 0] = 72;  // 'H'
    ptr8[p + 1] = 105; // 'i'
    ptr8[p + 2] = 10;  // '\n'
    ptr8[p + 3] = 0;

    print_str(p);
}
