# Basm Stage1 문법(현재 구현 기준)

이 문서는 **현재 레포의 Stage1 컴파일러가 실제로 처리할 수 있는 문법**을 기준으로 정리한 “사용자 관점 스펙”입니다. (파서/렉서 구현 + `examples/syntax.b` 동작 기준)

> 핵심 컨셉: **High-Level Assembly**
> - 레지스터를 숨기지 않고 직접 다룸
> - 문장은 거의 1:1로 NASM 코드로 내려감
> - Stage1은 아직 “표현식 언어”라기보다 “문장(Statement) 언어”에 가까움

---

## 1) 어휘 규칙(토큰)

### 공백/줄바꿈
- 공백/탭/개행은 토큰 구분에만 사용됩니다.

### 주석
- 한 줄 주석만 지원합니다.
- `//` 부터 줄 끝까지가 주석입니다.

```c
rax += 1; // 여기부터 끝까지 주석
```

### 키워드
- `func`, `var`, `const`, `layout`, `if`, `while`, `break`, `continue`, `asm`, `ptr8`, `ptr16`, `ptr32`, `ptr64`, `alias`

### 식별자(ident)
- 함수 이름, 변수 이름(전역/로컬)로 사용됩니다.

### 레지스터(reg)
현재 Stage1에서 “레지스터 토큰”으로 인식되는 이름은 아래뿐입니다.

- 64-bit: `rax rbx rcx rdx rsi rdi rsp rbp`
- 64-bit: `r8 r9 r10 r11 r12 r13 r14 r15`

> `eax` 같은 32-bit 레지스터 이름은 아직 **현재 문법에서 레지스터로 인식되지 않습니다.**

### 정수 리터럴(int)
- 10진수: `123`
- 16진수: `0x1122`, `0XFF`

> `-1` 같은 음수 리터럴은 별도 문법이 없습니다. 필요하면 `rax = 0; rax -= 1;` 같은 방식으로 만듭니다.

### 문자열 리터럴(str)
- 큰따옴표: `"..."`
- 지원 escape: `\n`, `\t`, `\"`, `\\`
- 문자열 내부에 “raw newline(실제 개행)”은 허용되지 않습니다.

### 문자 리터럴(char)
- 작은따옴표: `'A'`
- 지원 escape: `\n`, `\t`, `\'`, `\\`
- 내부적으로 **ASCII 코드값(정수)** 으로 취급됩니다.

---

## 2) 프로그램 구조

### 전체 구조
- 전역 `var` 선언(0개 이상)
- 함수 `func` 선언(0개 이상)
- 반드시 `func main() { ... }` 가 존재해야 합니다.

대략적인 형태:

```c
var g;
var buf[16];

func main() {
    var x;
    // ...
}
```

---

## 3) 전역 변수 선언

### 스칼라 전역
```c
var g_qword;
```
- 8바이트(qword) 슬롯을 하나 잡고 0으로 초기화됩니다.

### 전역 배열(바이트 배열)
```c
var g_bytes[4];
```
- N 바이트짜리 raw 메모리 블록입니다.
- 초기화는 없습니다(.bss `resb` 개념).

---

## 3.1) 전역 상수 선언(const)

Stage1에서는 `const`를 **파일 최상단(전역 영역)** 에서만 선언할 수 있습니다.

```c
const N = 100;
```

- 형태: `const <NAME> = <INT>;`
- `<NAME>`은 이후 문장에서 **즉시값(int)처럼** 사용할 수 있습니다.
- 또한 `ptr*[ ... + const ]` 같은 주소식의 disp로도 사용할 수 있습니다.

---

## 3.2) 레이아웃 선언(layout)

Stage1의 `layout`은 “구조체(field 오프셋)”를 간단히 쓰기 위한 기능입니다.
역시 **전역 영역**에서만 선언할 수 있습니다.

```c
layout Foo {
  ptr8  a;
  ptr64 b;
}
```

`layout Foo { ... }`는 아래 상수들을 자동으로 생성합니다.

- `Foo.a`, `Foo.b` : 각 멤버의 byte offset
- `Foo.SIZE` : 전체 크기(byte)

예:

```c
ptr8[buf + Foo.a] = 1;
```

> 주의: Stage1의 layout은 alignment/padding 규칙이 아니라, **선언된 순서대로 size만큼 offset을 누적**하는 단순 규칙입니다.

---

## 4) 함수 선언

### 기본 형태
```c
func foo() {
    // ...
}
```

### 인자 목록
```c
func f(a, b, c) { }
```
- 선언부 인자는 `ident`를 콤마로 나열하며 최대 6개까지 파싱됩니다.
- **Stage1에서는 선언된 인자를 실제로 로컬 변수로 바인딩/접근하는 기능이 아직 없습니다.**

### 함수 바디 규칙(First Statement Rule)
- 함수 바디 시작 부분에는 `var` 로컬 선언이 0개 이상 올 수 있습니다.
- `var` 선언이 끝나면 그 다음부터는 statement만 올 수 있습니다.
  - 즉, 중간에 `var`를 다시 선언하는 형태는 허용되지 않습니다.

---

## 5) 로컬 변수 선언

### 로컬 스칼라(8바이트)
```c
func main() {
    var x;
    // ...
}
```
- 8바이트 슬롯이 스택에 잡히며 기본값은 0입니다.

### 로컬 배열(바이트 배열)
```c
func main() {
    var buf[16];
}
```
- 스택에 **raw 바이트 버퍼**가 잡힙니다.
- 배열은 초기화하지 않습니다.
- 스택 할당은 8바이트 단위로 올림(round-up)됩니다.
- 함수 전체 로컬 할당량을 16바이트 정렬 유지하도록 padding이 들어갈 수 있습니다.

---

## 6) 주소식(addr)과 ptr 접근

Stage1에서 메모리 접근은 `ptr8[...]`, `ptr16[...]`, `ptr32[...]`, `ptr64[...]` 형태로만 합니다.

### addr 문법
`ptr*[addr]`의 `addr`는 아래 중 하나입니다.

- `ident`
- `ident + reg`  (reg는 **바이트 오프셋**)
- `ident + int`
- `ident + const`
- `reg`
- `reg + int`
- `reg - int`
- `reg + const`
- `reg - const`

추가 규칙/제약(Stage1):
- `ident`가 alias이면, ident를 레지스터처럼 취급하여 `reg` addr 규칙으로 들어갑니다.
- `ident` base에서 suffix는 **plus-only**이며, 아래 중 딱 1개만 허용됩니다.
  - `ident + reg` 또는 `ident + int` 또는 `ident + const`
- 그래서 `ptr8[buf + Foo.a + 8]`, `ptr8[Foo.a + buf]` 같은 형태는 아직 불가입니다.

> **스케일(scale) 문법은 없습니다.**
> 예를 들어 qword 배열처럼 “index*8”이 필요하면, 사용자가 직접 `rax = 8;` 같은 바이트 오프셋을 만들어야 합니다.

### 로드(load)
- `reg = ptr8[addr];`
- `reg = ptr16[addr];`
- `reg = ptr32[addr];`
- `reg = ptr64[addr];`

예:
```c
rdi = ptr8[g_bytes + rax];
rax = ptr64[g_qwords + rax];
```

### 스토어(store)
- `ptr8[addr] = (reg | int);`
- `ptr16[addr] = (reg | int);`
- `ptr32[addr] = (reg | int);`
- `ptr64[addr] = reg;`
- `ptr64[addr] = int;` 는 **signed 32-bit 범위에서만 허용**됩니다.

추가 제약:
- `ptr16[...] = int`는 `0..65535` 범위만 허용됩니다.
- `ptr32[...] = int`는 `0..0xFFFFFFFF` 범위만 허용됩니다.

레지스터 폭(중요):
- Stage1의 레지스터 토큰은 64-bit 이름만 인식하지만, `ptr8/16/32` 접근에서는 내부적으로 `al/ax/eax`, `r8b/r8w/r8d`처럼 **하위 폭 레지스터로 변환**해서 emit 합니다.
- 따라서 `ptr16[...] = rax;` 처럼 “64-bit 레지스터 이름”을 써도 동작합니다(단, 변환 가능한 레지스터만).

중요: x86-64는 `mov qword [mem], imm64` 인코딩이 없어서, 큰 64비트 상수는 아래처럼 해야 합니다.

```c
r11 = 0x1122334455667788;
ptr64[qw + rax] = r11;
```

---

## 7) 문장(Statements)

Stage1은 “표현식 우선순위” 같은 것을 거의 지원하지 않고, 아래 문장들만 지원합니다.

### 7.0 alias (레지스터 별칭)
형태:
```c
alias <reg> : <name>;
```

- `<name>`은 이후 문장에서 **레지스터처럼** 사용할 수 있습니다.
- 예:
```c
alias r12 : count;
count = 0;
count += 1;
```

주의:
- 런타임 호출(`print_dec`, `print_str`)은 일부 레지스터를 clobber 합니다. 카운터처럼 값 유지가 필요하면 `r12` 같은 callee-saved 쪽을 쓰는 것이 안전합니다.

### 7.1 레지스터 대입(=)
```c
rax = 123;
rax = rbx;
rax = ptr8[buf + rdx];
rax = ptr16[buf + rdx];
rax = ptr32[buf + rdx];
rax = ptr64[g_qword];
```
- RHS는 `int | reg | ptr8[...] | ptr16[...] | ptr32[...] | ptr64[...]` 중 하나만 올 수 있습니다.
- `rax = rbx + 1;` 같은 일반 표현식은 **현재 불가**입니다.

### 7.2 복합 대입(Compound Assignment)
레지스터에만 적용됩니다:

- `+=`, `-=`, `*=`
- `&=`, `|=`, `^=`
- `<<=`, `>>=`

예:
```c
rbx += 5;
rbx &= 0xFF;
rcx <<= 6;
```

제약:
- `<<=` / `>>=` 의 RHS는 현재 **int(즉시값)만 허용**됩니다.

### 7.3 함수 호출(call) 문장
형태:
```c
ident '(' [args] ')' ';'
```
- 인자는 최대 6개
- 인자 타입: `int | char | reg | str | ptr8[...] | ptr16[...] | ptr32[...] | ptr64[...]`

예:
```c
print_str("hello\n");
print_dec(rdi);
```

주의:
- 호출은 일반적으로 caller-saved 레지스터를 많이 clobber 합니다.
  - 예제처럼, “값을 레지스터에 들고 print_* 호출을 여러 번” 할 때는 중간에 값이 깨질 수 있으니 다시 로드하거나(또는 callee-saved 사용) 주의해야 합니다.

### 7.4 asm 블록 (NASM escape hatch)

형태:
```c
asm { "<nasm text>" ... };
```

- `{}` 안에는 **문자열 리터럴(str)** 을 1개 이상 나열합니다.
  - 구분은 공백/개행 또는 `;` 모두 가능합니다.
- 각 문자열은 그대로 NASM 출력 `.text`에 삽입됩니다.
- 문자열 안에 raw newline(실제 개행)은 불가이므로, 줄바꿈은 `\n` escape를 써야 합니다.

예:
```c
func main() {
  asm {
    "mov rdi, 777\n"
    "call print_dec\n"
  };
}
```

주의(footguns):
- 이 블록은 **컴파일러가 레지스터/스택/호출규약을 보호해주지 않습니다.**
- 잘못된 NASM을 넣으면 어셈블 단계에서 실패합니다.

---

## 8) while 문 (비교 연산은 조건에서만 가능)

### 문법
```c
while ( (<reg|alias> | ptr8[addr]) <cmp> <int|reg|alias|const|char> ) { <stmt>* }
```

- `<cmp>`: `== != < <= > >=`
- 비교 연산은 **if/while 조건에서만 허용**됩니다.
- `break;` / `continue;` 를 지원합니다(while 안에서만 사용 가능).

추가:
- `ptr8[addr]`를 LHS로 쓴 비교는 **unsigned byte 비교**로 처리됩니다.
- `char` 리터럴(`'A'`, `'\n'`)은 RHS에 쓸 수 있으며 내부적으로 정수(ASCII)로 취급됩니다.

예:

```c
alias rcx : counter;
alias rbx : sum;

counter = 1;
sum = 0;

while (counter <= 100) {
  sum += counter;
  counter += 1;
}
```

---

## 9) if 문 (비교 연산은 조건에서만 가능)

### 문법
```c
if ( (<reg|alias> | ptr8[addr]) <cmp> <int|reg|alias|const|char> ) { <stmt>* }
```

- `<cmp>`: `== != < <= > >=`
- 비교 연산은 **if/while 조건에서만 허용**됩니다.
- `else`는 없습니다.

예:
```c
if (rbx >= 16) {
    r11 = 1;
}

// pseudo-else는 두 개의 if로 작성
r11 = 0;
if (rbx < 64)  { r11 = 111; }
if (rbx >= 64) { r11 = 222; }
```

주의:
- Stage1 비교는 CPU의 signed 조건 점프(jl/jg 계열)를 기반으로 합니다.
  - unsigned 비교가 필요하면 Stage2에서 확장하거나 별도 규칙이 필요합니다.

---

## 10) 한 파일로 기능 확인하기

현재 구현된 대부분의 기능은 아래 예제에서 한 번에 볼 수 있습니다.

- [examples/syntax.b](../examples/syntax.b)
- [examples/while_sum_100.b](../examples/while_sum_100.b)
- [examples/hello_world.b](../examples/hello_world.b)
- [examples/asm_block.b](../examples/asm_block.b)
- [examples/cat_readme.b](../examples/cat_readme.b)

---

## 11) 런타임 내장 함수(Runtime Builtins)

Stage1은 출력 바이너리에 아래 런타임 함수들이 함께 포함됩니다.

또한 엔트리포인트 `_start`는 Linux 프로세스 스택의 인자 정보를 꺼내서 **`main(argc, argv)`** 형태로 호출합니다.

- `argc`는 `rdi`로 전달됩니다.
- `argv`는 `rsi`로 전달됩니다.
  - `argv`는 포인터 배열입니다. 즉 `argv[0]`는 `ptr64[rsi + 0]`, `argv[1]`는 `ptr64[rsi + 8]` 입니다.

- `print_str(ptr)` : null-terminated 문자열 출력
- `print_dec(u64)` : 10진수 출력
- `heap_alloc(size)` : bump allocator. `size` 바이트를 할당하고, **반환 포인터를 `rax`로 돌려줍니다** (OOM이면 `rax=0`).
  - 할당은 8바이트 단위로 올림(정렬)됩니다.
- `memcpy(dst, src, len)` : 메모리 복사. 반환값은 `rax=dst`
- `streq(a, b)` : null-terminated 문자열 비교. 같으면 `rax=1`, 다르면 `rax=0`
- `strlen(ptr)` : null-terminated 문자열 길이. 반환값은 `rax=len`

또한 Linux x86_64 시스템콜 래퍼도 포함됩니다.

- `sys_read(fd, buf, len)` : `read(2)` 래퍼. 반환값 `rax = 읽은 바이트 수` (에러면 음수)
- `sys_write(fd, buf, len)` : `write(2)` 래퍼. 반환값 `rax = 쓴 바이트 수` (에러면 음수)
- `sys_open(path, flags, mode)` : `open(2)` 래퍼. 반환값 `rax = fd` (에러면 음수)
- `sys_fstat(fd, statbuf)` : `fstat(2)` 래퍼. 반환값 `rax = 0` (에러면 음수)
- `sys_close(fd)` : `close(2)` 래퍼. 반환값 `rax = 0` (에러면 음수)
- `sys_exit(code)` : `exit(2)` 래퍼. **반환하지 않습니다**

예:
- [examples/cat_readme.b](../examples/cat_readme.b)

반환값을 쓰려면 `rax`를 alias로 잡아두는 패턴을 사용합니다.

```c
alias rax : p;
heap_alloc(16);
ptr8[p + 0] = 65;
```

`rax`는 caller-saved라서(그리고 다음 `heap_alloc` 호출에서도) 값이 쉽게 덮입니다.
포인터를 계속 들고 있어야 하면 호출 직후 callee-saved 레지스터로 옮겨서 보관하는 게 안전합니다.

```c
alias rax : tmp;
alias r12 : p1;
alias r13 : p2;

heap_alloc(16);
p1 = tmp;

heap_alloc(32);
p2 = tmp;

```

---

## 12) ABI / 호출 규약 (기술 스펙)

이 문서의 이 섹션부터는 “사용자 문법”을 넘어서, **생성되는 코드와 런타임의 동작 규약(ABI)** 를 명시합니다.

### 타겟/환경
- 아키텍처: **x86_64**
- 실행 환경: **Linux**
- 어셈블러/링커: NASM + ld

### 함수 호출 규약
- 인자 전달: System V AMD64 규약 스타일로 **최대 6개까지 레지스터로 전달**
  - 1..6번째 인자 레지스터: `rdi, rsi, rdx, rcx, r8, r9`
- 반환값: `rax`

주의:
- Stage1은 “레지스터를 숨기지 않는 언어”라서, **컴파일러가 callee-saved 보존을 강제하지 않습니다.**
- 런타임 builtins 및 사용자 함수 호출 전후로 값 보존이 필요하면, 사용자가 직접(예: callee-saved 레지스터 활용, 또는 `asm`로 push/pop) 관리해야 합니다.

### 런타임 builtins: clobber 규칙(중요)

아래는 “이 함수 호출 이후 값이 보존되지 않는 레지스터(최소 보장)” 기준으로 적습니다.

- `print_str(ptr)`
  - 인자: `rdi=ptr`
  - 반환: 없음(의미 있는 반환값 없음)
  - clobber(최소): `rax, rdi, rsi, rdx`

- `print_dec(u64)`
  - 인자: `rdi=value`
  - 반환: 없음(의미 있는 반환값 없음)
  - clobber(최소): `rax, rcx, rdx, r8, r9, rdi`

- `heap_alloc(size)`
  - 인자: `rdi=size`
  - 반환: `rax=ptr` (OOM이면 `rax=0`)
  - 정렬: 요청 size는 **8바이트 단위로 올림** 처리됨
  - clobber(최소): `rax, rcx, rdx, r8`

- `memcpy(dst, src, len)`
  - 인자: `rdi=dst, rsi=src, rdx=len`
  - 반환: `rax=dst`
  - 부작용(중요): 내부가 `rep movsb`라서 **호출 후 `rdi`/`rsi`가 증가한 값으로 남습니다.**
  - clobber(최소): `rax, rcx, rdi, rsi`

- `streq(a, b)`
  - 인자: `rdi=a, rsi=b` (둘 다 null-terminated)
  - 반환: `rax=1`(같음) / `rax=0`(다름)
  - clobber(최소): `rax, rcx, rdx, r8`

- `strlen(ptr)`
  - 인자: `rdi=ptr`
  - 반환: `rax=len`
  - clobber(최소): `rax`

### syscall wrappers: 동작/에러/클로버

이 래퍼들은 Linux x86_64의 `syscall` 명령을 그대로 사용합니다.

- 공통:
  - 반환: `rax`에 커널 반환값이 들어옵니다.
  - 에러: 보통 **음수(-errno)** 로 리턴됩니다(예: `rax < 0`).
  - `syscall` 자체 clobber: **`rcx`, `r11`** (하드웨어/ABI 규칙)

- `sys_read(fd, buf, len)`
  - 인자: `rdi=fd, rsi=buf, rdx=len`
  - 반환: `rax=읽은 바이트 수` (EOF=0, 에러<0)
  - clobber(최소): `rax, rcx, r11`

- `sys_write(fd, buf, len)`
  - 인자: `rdi=fd, rsi=buf, rdx=len`
  - 반환: `rax=쓴 바이트 수` (에러<0)
  - clobber(최소): `rax, rcx, r11`

- `sys_open(path, flags, mode)`
  - 인자: `rdi=path, rsi=flags, rdx=mode`
  - 반환: `rax=fd` (에러<0)
  - clobber(최소): `rax, rcx, r11`

- `sys_fstat(fd, statbuf)`
  - 인자: `rdi=fd, rsi=statbuf`
  - 반환: `rax=0` (에러<0)
  - clobber(최소): `rax, rcx, r11`

- `sys_close(fd)`
  - 인자: `rdi=fd`
  - 반환: `rax=0` (에러<0)
  - clobber(최소): `rax, rcx, r11`

- `sys_exit(code)`
  - 인자: `rdi=code`
  - 반환: 없음(프로세스 종료)

---

## 13) 코드 생성(Codegen) / 출력 ASM 구조

Stage1 컴파일러는 `.b` 소스를 **NASM용 ASM 텍스트** 로 변환합니다.

### 출력 헤더
출력 ASM은 대략 아래 헤더를 포함합니다.

- `BITS 64`
- `DEFAULT REL`
- `global _start`
- 런타임/헬퍼 함수 정의

### 엔트리포인트
- 최종 엔트리는 `_start` 입니다.
- `_start`는 힙을 초기화한 뒤 `main`을 호출하고, `syscall exit(0)`로 종료합니다.
  - 즉, `main`의 `ret` 값은 **현재 종료 코드로 사용되지 않습니다.**

### 섹션 배치(개략)
- `.text` : `_start`, 런타임 builtins, 사용자 함수 코드
- `.data` : 전역 스칼라(`dq 0`), 문자열 리터럴(0-terminated)
- `.bss`  : 전역 byte 배열(`resb`), 힙 버퍼(`heap_buf`) 및 포인터(`heap_cur`)

### 로컬 변수 레이아웃
- 스택 기준점은 `rbp`입니다.
- 로컬 스칼라(`var x;`)는 `push 0`으로 **0 초기화** 된 8바이트 슬롯을 잡습니다.
- 로컬 배열(`var buf[N];`)은 `sub rsp, round_up(N,8)`로 잡고 **초기화하지 않습니다.**
- 로컬 변수 주소는 내부적으로 `[rbp-<offset>]` 형태로 내려갑니다.

### 문자열 리터럴의 배치
- 문자열 리터럴은 `.data`에 `db <bytes>, 0`로 생성됩니다(null-terminated).
- 같은 문자열을 dedup(재사용)하는 최적화는 아직 없습니다(등장할 때마다 새 label).

### 라벨 네이밍(현재 구현)
- if: `Lif_end_<id>`
- while: `Lwhile_start_<id>`, `Lwhile_end_<id>`

### asm 블록의 의미
- `asm { "..." }`는 해당 문자열의 바이트를 **그대로 `.text`에 삽입**합니다.
- 컴파일러는 올바른 섹션/정렬/레지스터 보존을 보장하지 않습니다.

---

## 14) 에러 모델 / 제한(Limits)

### 에러 모델
- 대부분의 오류는 “parse error”로 라인 번호와 함께 즉시 종료합니다.
- NASM 문법 오류는 컴파일러가 아닌 **어셈블 단계(NASM)** 에서 실패합니다(특히 `asm` 블록).

### 주요 제한(현재 구현 기준)
- 함수 인자: 최대 6개까지 파싱되지만, **인자를 로컬로 바인딩해 쓰는 기능은 미구현**
- 호출 인자: 최대 6개
- 심볼 테이블: 최대 256개(초과 시 에러)
- 출력 ASM 버퍼: 기본 1MiB(`EMIT_BUF_MAX`) (초과 시 에러)
- 힙 크기: 1MiB (`HEAP_SIZE`) 고정

---

## 15) 미지원/비정의 동작(Unsupported / Undefined Behavior)

Stage1의 목표는 “표현식 언어”가 아니라 “High-Level Assembly”이므로, 아래는 의도적으로 미지원이거나 동작이 정의되지 않습니다.

### 문법 미지원
- `return;` / `return expr;`
- `else`
- 일반 표현식(`rax = rbx + 1;`, `rax = (a+b)*c;` 등)
- 음수 리터럴(`-1`) 직접 표기
- 스케일드 인덱싱(`base + index*8`) 주소식

### 동작/규약 주의
- builtin 호출은 많은 레지스터를 clobber 하며, 컴파일러가 보존하지 않습니다.
- `memcpy`는 호출 후 `rdi/rsi`가 증가합니다(원본 포인터가 필요하면 호출 전에 보관).
- syscall 래퍼의 에러(-errno)는 사용자가 직접 검사해야 합니다.
```
