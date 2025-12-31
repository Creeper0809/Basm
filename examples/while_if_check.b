func main() {

    alias rcx : i;
    alias rbx : hits;

    i = 1;
    hits = 0;

    // i = 1..20
    while (i <= 20) {
        // if inside while should run exactly for 5, 10, 15, 20
        if (i >= 5) {
            if (i <= 20) {
                if (i == 5)  { hits += 1; }
                if (i == 10) { hits += 1; }
                if (i == 15) { hits += 1; }
                if (i == 20) { hits += 1; }
            }
        }
        i += 1;
    }

    print_str("hits = ");
    print_dec(hits);
    print_str("\n");
}
