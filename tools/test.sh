#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR=build
BASM_BIN="$BUILD_DIR/basm"

NASM=${NASM:-nasm}
LD=${LD:-ld}

if [[ ! -x "$BASM_BIN" ]]; then
  echo "[test] building compiler" >&2
  make -j build >/dev/null
fi

pass=0
fail=0

die() {
  echo "[test] FAIL: $*" >&2
  exit 1
}

compile_link_run() {
  local src="$1"
  local base
  base="$BUILD_DIR/$(basename "$src" .b)"

  "$BASM_BIN" "$src" -o "$base.asm" >/dev/null
  "$NASM" -f elf64 "$base.asm" -o "$base.o" >/dev/null
  "$LD" "$base.o" -o "$base" >/dev/null

  "$base"
}

assert_file_eq() {
  local name="$1"
  local got_path="$2"
  local expected_path="$3"

  if ! cmp -s "$expected_path" "$got_path"; then
    echo "[test] $name: output mismatch" >&2
    echo "----- expected (hexdump) -----" >&2
    hexdump -C "$expected_path" | head -n 40 >&2 || true
    echo "------ got (hexdump) --------" >&2
    hexdump -C "$got_path" | head -n 40 >&2 || true
    return 1
  fi
}

run_case() {
  local name="$1"
  shift
  if "$@"; then
    echo "[test] ok: $name" >&2
    pass=$((pass+1))
  else
    echo "[test] FAIL: $name" >&2
    fail=$((fail+1))
  fi
}

run_and_capture() {
  local src="$1"
  local out_path="$2"
  compile_link_run "$src" >"$out_path"
}

expect_text() {
  local out_path="$1"
  shift
  printf "%s" "$*" >"$out_path"
}

# 1) Output-checked examples (byte-exact)
{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/hello_world.b "$got"
  expect_text "$exp" $'Hello World! \n'
  run_case "hello_world" assert_file_eq hello_world "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/while_sum_100.b "$got"
  expect_text "$exp" $'sum(1..100) = 5050\n'
  run_case "while_sum_100" assert_file_eq while_sum_100 "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/break_continue.b "$got"
  expect_text "$exp" $'sum(1..10) = 55\n'
  run_case "break_continue" assert_file_eq break_continue "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/heap_alloc.b "$got"
  expect_text "$exp" $'Hi\n'
  run_case "heap_alloc" assert_file_eq heap_alloc "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/memcpy.b "$got"
  expect_text "$exp" $'Hello\n'
  run_case "memcpy" assert_file_eq memcpy "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/streq.b "$got"
  expect_text "$exp" $'eq1 = 1\neq2 = 0\n'
  run_case "streq" assert_file_eq streq "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/strlen.b "$got"
  expect_text "$exp" $'3\n'
  run_case "strlen" assert_file_eq strlen "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/char_literal.b "$got"
  expect_text "$exp" $'A\n'
  run_case "char_literal" assert_file_eq char_literal "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/ptr8_ucmp.b "$got"
  expect_text "$exp" $'good\n'
  run_case "ptr8_ucmp" assert_file_eq ptr8_ucmp "$got" "$exp"
  rm -f "$got" "$exp"
}

{
  got=$(mktemp)
  exp=$(mktemp)
  run_and_capture examples/asm_block.b "$got"
  expect_text "$exp" "777"
  run_case "asm_block" assert_file_eq asm_block "$got" "$exp"
  rm -f "$got" "$exp"
}

# cat_readme: big output, sanity-check prefix matches actual README.md prefix
{
  got=$(mktemp)
  run_and_capture examples/cat_readme.b "$got"
  got_prefix=$(mktemp)
  exp_prefix=$(mktemp)
  head -c 80 "$got" >"$got_prefix" || true
  head -c 80 README.md >"$exp_prefix" || true
  run_case "cat_readme" assert_file_eq cat_readme "$got_prefix" "$exp_prefix"
  rm -f "$got" "$got_prefix" "$exp_prefix"
}

# 2) Full compile/link/run sweep (no output checks)
{
  sweep() {
    for f in examples/*.b; do
      compile_link_run "$f" >/dev/null
    done
  }
  run_case "examples sweep" sweep
}

echo "[test] pass=$pass fail=$fail" >&2
[[ $fail -eq 0 ]]
