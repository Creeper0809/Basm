var g_qword;
var g_byte;

func foo(){
    print_str("in foo\n");
}

func main() {
	var a;
	var b;
	var c;
	var n;
	var ch;

	rax = 0x2a;
	rbx = rax;

	ptr64[a] = rbx;
	rcx = ptr64[a];
	rdi = rcx;
	print_dec(rdi);
	print_str("\n");

	ptr64[b] = 124;
	rdi = ptr64[b];
	print_dec(rdi);
	print_str("\n");

	ptr64[n] = 2025;
	rdi = ptr64[n];
	print_dec(rdi);
	print_str("\n");

	ptr8[ch] = 90;      // 'Z'
	rdi = ptr8[ch];
	print_dec(rdi);
	print_str(" <- 90 == 'Z'\n");
	print_str("Z\n");

	
	rdx = ptr8[g_byte];

	
	ptr8[c] = rdx;
	r8 = ptr8[c];
	rdi = r8;
	print_dec(rdi);
	print_str("\n");

	r10 = rbp;
	ptr64[r10-8] = 777;
	rdi = ptr64[a];
	print_dec(rdi);
	print_str("\n");

	r11 = 0x1122334455667788;
	ptr64[g_qword] = r11;
	r11 = ptr64[g_qword];
	rdi = r11;
	print_dec(rdi);
	print_str("\n");

    foo();

	print_str("tab:\tend\\n\n");
}
