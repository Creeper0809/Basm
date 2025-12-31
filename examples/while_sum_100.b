func main() {

    alias rcx : counter;
    alias rbx : sum;

	counter = 1;
	sum = 0;

	while (counter <= 100) {
		sum += counter;
		counter += 1;
	}

    print_str("sum(1..100) = ");
	print_dec(sum);
    print_str("\n");
	
}