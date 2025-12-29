## Basm (The First Step of Bpp)

"HTML을 프로그래밍 언어라고 인정할 수 없다."

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

- Explicit Registers: rax, r8 등을 직접 제어한다.

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

// Just do it. (Stage1 현재 구현 기준)
// - 레지스터는 64-bit 이름(rax..r15)만 레지스터로 인식합니다.
// - 비교 연산자는 if 조건에서만 허용됩니다.

rax = 10;
rax += 1;

// 메모리 접근은 ptr8/ptr64를 통해서만 합니다.
// (예: ptr64[var] = rax;  rdi = ptr64[var];)

if (rax > 5) {
        // 함수 호출은 ident(args...);
        // (내장 런타임 예: print_str, print_dec)
        print_str("ok\n");
}
```

## 문법 문서

현재 Stage1에서 실제로 지원되는 문법/제약은 아래 문서에 정리되어 있습니다.

- [syntax.md](syntax.md)

### 자주 헷갈리는 포인트(요약)

- `eax` 같은 32-bit 레지스터 이름은 현재 레지스터로 인식되지 않습니다(예: `rax` 사용).
- 비교(`== != < <= > >=`)는 **if 조건에서만** 사용 가능합니다.
- `ptr64[mem] = imm64`는 CPU 인코딩 제약 때문에 직접 지원하지 않습니다.
    - 큰 64비트 상수는 `r11 = 0x...; ptr64[mem] = r11;`처럼 레지스터 경유로 저장하세요.
    - 작은 즉시값은 `ptr64[mem] = 123;` 형태로 가능(범위 제한 있음).

## Roadmap

    [O] Stage 0: 프로젝트 기획

    [ ] Stage 1 (Current): Basm 구현 (Written in NASM)

        [O] NASM 매크로 및 파싱 로직 구현

        [O] 레지스터 직접 할당 및 변수 생성 문법 지원
        
        [O] 오퍼레이터 지원

        [ ] 제어문(if, while) 구조화

        [ ] ELF64 바이너리 생성

    [ ] Stage 2: Bootstrapping

        [ ] Basm을 사용하여 Bpp 컴파일러를 재작성

    [ ] Stage 3: World Domination (Replacing PHP)

## File Structure

```text
.
├── README.md
├── Makefile
├── basm.asm
├── build/             # 산출물(basm.o, basm)
├── include/
│   ├── consts.inc
│   └── macros.inc
├── src/
│   ├── emitter.inc
│   ├── cli.inc
│   ├── elf64.inc
│   ├── lexer.inc
│   ├── parser.inc
│   └── util.inc
├── tools/
│   └── basm_driver.c   # gcc 링커 드라이버 매핑 
├── examples/
│   └── hello.b         # 예시 b 언어
└── .gitignore
```

- `basm.asm`: 엔트리 포인트. 각 모듈을 `%include`로 묶습니다.
- `include/`: 공용 상수/매크로.
- `src/`: CLI/파서/IR/백엔드/ELF64 등 단계별 모듈.
- `examples/`: Basm/Bpp 예제 코드.
- `build/`: 컴파일러 빌드 산출물.

## Build & Run

이 프로젝트는 NASM과 Linker (ld)를 사용합니다.

현재 구성은 gcc처럼 쓰기 위해 2개의 바이너리를 둡니다.

- `basm`: 드라이버(컴파일 → 조립/링크 → 실행까지 자동)
- `basm-cc`: 컴파일러(입력 `.b` → NASM asm 텍스트 생성)

레포 로컬 빌드 시:

- `build/basm` = `basm-cc` 역할
- `build/basm_driver` = `basm` 역할


### 1) Build (Makefile 권장)
```bash
make build
```

짧게 실행까지 하고 싶으면 (두 번째 인자를 입력 파일로 받습니다):
```bash
make run examples/hello.b
```

### 2) 컴파일러만 사용하기 (ASM 생성)
`-o`를 주면 파일로도 저장됩니다.
```bash
./build/basm examples/hello.b              # stdout으로 asm 출력
./build/basm examples/hello.b -o /tmp/out.asm
```

### 4) (옵션) basm 커맨드로 설치
`~/.local/bin/basm`로 설치합니다.
```bash
make install
export PATH="$HOME/.local/bin:$PATH"   # 필요 시
basm examples/hello.b

# 설치된 컴파일러를 직접 쓰고 싶으면
basm-cc ./examples/hello.b                 # stdout으로 asm 출력
basm-cc ./examples/hello.b -o /tmp/out.asm
```

설치 직후에 `basm`이 이상한 경로를 찾는다면(과거 경로 캐시), 아래로 쉘 캐시를 갱신:
```bash
hash -r
```
