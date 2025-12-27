## Basm (The First Step of Bpp)

"나는 PHP와 HTML을 프로그래밍 언어라고 인정할 수 없다."

Basm은 언어 Bpp를 만들기 위한 첫 번째 단계이자, 부트스트래핑을 위한 초기 컴파일러입니다.

NASM으로 작성되었으며 "어셈블리어와 편하게 대화하는 방법"을 제시합니다.

## Why?

이 세상에는 없어져야 더 행복해질 수 있는 것들이 잔뜩있습니다.

- C언어의 레지스터 숨김

- 클로버 리스트

- 그리고... 시험 문제에 bpp 대신 html을 언어라고 적어야하는 상황

그것들을 bpp의 힘으로 모두 없앨겁니다.

## Core Philosophy: High-Level Assembly

Basm의 철학은 단순합니다.

- High-Level Assembly: 어셈블리어의 제어권 + C언어의 가독성.

- Explicit Registers: eax, r8등을 직접 제어한다.

## Syntax Preview

Traditional C + Inline Assembly (Painful):
```C

// GCC Style....
int val = 10;
__asm__ volatile (
    "movl %1, %%eax \n\t"
    "addl $1, %%eax \n\t"
    : "=a"(val) : "r"(val)
);
```

Basm (EZ & Clean):
```C

// Just do it.
eax = 10;
eax += 1;

if (eax > 5) {
    // System Call Example (Linux x64 write)
    rax = 1;        // sys_write
    rdi = 1;        // stdout
    rsi = msg_ptr;  // buffer
    rdx = 12;       // length
    syscall;
}
```

## Roadmap

    [x] Stage 0: 프로젝트 기획

    [ ] Stage 1 (Current): Basm 구현 (Written in NASM)

        [ ] NASM 매크로 및 파싱 로직 구현

        [ ] IR 생성 로직 구현

        [ ] 백엔드 생성 로직 구현

        [ ] 레지스터 직접 할당 및 변수 생성 문법 지원
        
        [ ] 오퍼레이터 지원

        [ ] 제어문(if, while) 구조화

        [ ] ELF64 바이너리 생성

    [ ] Stage 2: Bootstrapping

        [ ] Basm을 사용하여 Bpp 컴파일러를 재작성

    [ ] Stage 3: World Domination (Replacing PHP)

## Build & Run

이 프로젝트는 NASM과 Linker (ld)를 사용합니다.


### 1. Assemble the compiler
```Bash
nasm -f elf64 basm.asm -o basm.o
ld basm.o -o basm
```

### 2. Compile your Bpp code using Basm
```Bash
./basm source.bpp -o output.asm
nasm -f elf64 output.asm -o output.o
ld output.o -o output
```

### 3. Run!
```Bash
./output
```
