func main() {

    alias rax : tmp;
    alias r12 : src;
    alias r13 : dst;

    // allocate src buffer
    heap_alloc(6);
    src = tmp;

    // "Hello\0"
    ptr8[src + 0] = 72;  // H
    ptr8[src + 1] = 101; // e
    ptr8[src + 2] = 108; // l
    ptr8[src + 3] = 108; // l
    ptr8[src + 4] = 111; // o
    ptr8[src + 5] = 0;

    // allocate dst buffer
    heap_alloc(6);
    dst = tmp;

    // memcpy(dst, src, 6)
    memcpy(dst, src, 6);

    print_str(dst);
    print_str("\n");
}
