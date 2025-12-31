// ptr8 unsigned compare smoke (0x80 > 0x7f should be true)

func main() {
    alias rax : tmp;
    alias r12 : p;
    alias r13 : q;
    alias rbx : b;

    heap_alloc(1);
    p = tmp;
    ptr8[p] = 0x80;

    heap_alloc(1);
    q = tmp;
    ptr8[q] = 0x7f;

    b = ptr8[q];

    if (ptr8[p] > b) {
        print_str("good\n");
    }
}
