func main() {

    alias rax : tmp;
    alias r12 : a;
    alias r13 : b;
    alias rbx : eq;

    // a = "Hi\0"
    heap_alloc(3);
    a = tmp;
    ptr8[a + 0] = 72;
    ptr8[a + 1] = 105;
    ptr8[a + 2] = 0;

    // b = "Hi\0"
    heap_alloc(3);
    b = tmp;
    ptr8[b + 0] = 72;
    ptr8[b + 1] = 105;
    ptr8[b + 2] = 0;

    streq(a, b);
    eq = tmp;

    print_str("eq1 = ");
    print_dec(eq);
    print_str("\n");

    // change b[1] to 'o' => "Ho\0"
    ptr8[b + 1] = 111;

    streq(a, b);
    eq = tmp;

    print_str("eq2 = ");
    print_dec(eq);
    print_str("\n");
}
