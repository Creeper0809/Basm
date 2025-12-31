// char literal smoke

func main() {
    alias rax : tmp;
    alias r12 : p;

    heap_alloc(3);
    p = tmp;

    ptr8[p + 0] = 'A';
    ptr8[p + 1] = '\n';
    ptr8[p + 2] = 0;

    print_str(p);
}
