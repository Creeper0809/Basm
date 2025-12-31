BUILD_DIR := build

BASM_ASM := basm.asm
BASM_OBJ := $(BUILD_DIR)/basm.o
BASM_BIN := $(BUILD_DIR)/basm

DRIVER_SRC := tools/basm_driver.c
DRIVER_BIN := $(BUILD_DIR)/basm_driver

EXAMPLE ?= examples/hello.b
OUT_ASM  ?= /tmp/hello_ir.asm
OUT_OBJ  := $(OUT_ASM:.asm=.o)
OUT_BIN  := $(OUT_ASM:.asm=)

NASM := nasm
LD   := ld

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
INSTALL ?= install

ARG_GOAL_TARGETS := run go compile print asm
ifneq ($(filter $(firstword $(MAKECMDGOALS)),$(ARG_GOAL_TARGETS)),)
ARG1 := $(word 2,$(MAKECMDGOALS))
ifneq ($(strip $(ARG1)),)
EXAMPLE := $(ARG1)
$(eval $(ARG1):;@:)
endif
endif

.PHONY: build clean compile run print install uninstall go asm driver test

build: driver
	@mkdir -p $(BUILD_DIR)
	$(NASM) -f elf64 $(BASM_ASM) -o $(BASM_OBJ)
	$(LD) $(BASM_OBJ) -o $(BASM_BIN)

driver:
	@mkdir -p $(BUILD_DIR)
	gcc -O2 -Wall -Wextra -std=c11 $(DRIVER_SRC) -o $(DRIVER_BIN)

compile: build
	$(BASM_BIN) $(EXAMPLE) -o $(OUT_ASM) > /dev/null

run: compile
	$(NASM) -f elf64 $(OUT_ASM) -o $(OUT_OBJ)
	$(LD) $(OUT_OBJ) -o $(OUT_BIN)
	$(OUT_BIN)

go: run

asm: compile
	@echo "asm: $(OUT_ASM)"

print: build
	$(BASM_BIN) $(EXAMPLE) -o $(OUT_ASM)

clean:
	rm -rf $(BUILD_DIR)

install: build
	@mkdir -p $(BINDIR)
	$(INSTALL) -m 755 $(DRIVER_BIN) $(BINDIR)/basm
	$(INSTALL) -m 755 $(BASM_BIN) $(BINDIR)/basm-cc
	@echo "installed: $(BINDIR)/basm"
	@echo "installed: $(BINDIR)/basm-cc"

uninstall:
	rm -f $(BINDIR)/basm $(BINDIR)/basm-cc

test: build
	bash tools/test.sh
