func main() {

    alias rcx : counter;
    alias rbx : sum;

    counter = 0;
    sum = 0;

    while (counter < 100) {
        counter += 1;

        if (counter > 50) {
            break;
        }

        if (counter > 10) {
            continue;
        }

        sum += counter;
    }

    print_str("sum(1..10) = ");
    print_dec(sum);
    print_str("\n");
}
