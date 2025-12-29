var g_qword;
var g_byte;
var g_bytes[4];
var g_qwords[3];

func foo(){
	print_str("in foo\n");
}

func main() {
	var a;
	var b;
	var n;
	var ch;
	var buf[16];
	var qw[24];

	print_str("--- hello.b: Stage1 showcase ---\n");

	// basic ptr64 load/store (local scalar)
	rax = 0x2a;
	ptr64[a] = rax;
	rdi = ptr64[a];
	print_dec(rdi);
	print_str(" <- 42\n");

	ptr64[b] = 124;
	rdi = ptr64[b];
	print_dec(rdi);
	print_str(" <- 124\n");

	ptr64[n] = 2025;
	rdi = ptr64[n];
	print_dec(rdi);
	print_str(" <- 2025\n");

	// ptr8 local scalar
	ptr8[ch] = 90;      // 'Z'
	rdi = ptr8[ch];
	print_dec(rdi);
	print_str(" <- 90 == 'Z'\n");
	print_str("Z\n");

	// globals
	ptr8[g_byte] = 66;
	rdi = ptr8[g_byte];
	print_dec(rdi);
	print_str(" <- g_byte\n");

	r11 = 0x1122334455667788;
	ptr64[g_qword] = r11;
	rdi = ptr64[g_qword];
	print_dec(rdi);
	print_str(" <- g_qword\n");

	// local byte buffer + base+index addressing (reg is byte offset)
	rax = 0;
	ptr8[buf + rax] = 72;   // 'H'
	rax = 1;
	ptr8[buf + rax] = 105;  // 'i'
	rax = 2;
	ptr8[buf + rax] = 10;   // '\n'

	rax = 0;
	rdi = ptr8[buf + rax];
	print_dec(rdi);
	print_str(" ");
	rax = 1;
	rdi = ptr8[buf + rax];
	print_dec(rdi);
	print_str(" ");
	rax = 2;
	rdi = ptr8[buf + rax];
	print_dec(rdi);
	print_str(" <- buf bytes\n");

	// global byte array + base+index
	rax = 0;
	ptr8[g_bytes + rax] = 1;
	rax = 1;
	ptr8[g_bytes + rax] = 2;
	rax = 2;
	ptr8[g_bytes + rax] = 3;
	rax = 3;
	ptr8[g_bytes + rax] = 4;

	rax = 3;
	rdi = ptr8[g_bytes + rax];
	print_dec(rdi);
	print_str(" <- g_bytes[3]\n");

	// local qword buffer (byte offsets: 0,8,16...)
	rax = 0;
	ptr64[qw + rax] = 999;
	rax = 8;
	ptr64[qw + rax] = 12345;
	rax = 16;
	r11 = 0x1122334455667788;
	ptr64[qw + rax] = r11;

	rax = 16;
	rdi = ptr64[qw + rax];
	print_dec(rdi);
	print_str(" <- qw[2]\n");

	// global qword array
	rax = 0;
	ptr64[g_qwords + rax] = 111;
	rax = 8;
	ptr64[g_qwords + rax] = 222;
	rax = 16;
	ptr64[g_qwords + rax] = 333;
	rax = 8;
	rdi = ptr64[g_qwords + rax];
	print_dec(rdi);
	print_str(" <- g_qwords[1]\n");

	// compound assignment + bit ops + shifts
	rbx = 3;
	rbx += 5;      // 8
	rbx *= 8;      // 64  // keep inline comment to exercise lexer
	rbx -= 10;     // 54
	rbx &= 0xFF;   // keep low byte
	rbx |= 0x0F;
	rbx ^= 0xF0;

	rcx = 1;
	rcx <<= 6;     // 64
	rcx = 64;
	rcx >>= 3;     // 8

	rdi = rbx;
	print_dec(rdi);
	print_str(" <- after compound ops\n");

	// comparisons are only allowed in if
	print_str("--- if showcase ---\n");
	print_str("rbx = ");
	rdi = rbx;
	print_dec(rdi);
	print_str("\n");

	// pattern 1) max/min without else (choose-default + override)
	print_str("max(a,b) = ");
	r8 = ptr64[a];
	r9 = ptr64[b];
	r10 = r8;
	if (r9 > r10) { r10 = r9; }
	rdi = r10;
	print_dec(rdi);
	print_str("\n");

	print_str("min(a,b) = ");
	r8 = ptr64[a];
	r9 = ptr64[b];
	r10 = r8;
	if (r9 < r10) { r10 = r9; }
	rdi = r10;
	print_dec(rdi);
	print_str("\n");

	// pattern 2) range check using nested if
	print_str("in_range[16..128] = ");
	r11 = 0;
	if (rbx >= 16) {
		r12 = 0;
		if (rbx <= 128) { r12 = 1; }
		if (r12 == 1) { r11 = 1; }
	}
	rdi = r11; 
	print_dec(rdi); //
	print_str("\n");

	// pattern 3) pseudo-else via two ifs (one of them should win)
	print_str("bucket = ");
	r11 = 0;
	rbx = 10;
	if (rbx < 64) { r11 = 111; }
	if (rbx >= 64) { r11 = 222; }
	rdi = r11;
	print_dec(rdi);
	print_str(" (111 if <64 else 222)\n");

	// pattern 4) boolean from bitwise + if (even check)
	print_str("is_even(rbx) = ");
	rax = rbx;
	rax &= 1;
	r11 = 0;
	if (rax == 0) { r11 = 1; }
	rdi = r11;
	print_dec(rdi);
	print_str("\n");

	// call + escapes
	foo();
	print_str("tab:\tend\n\n");
}
