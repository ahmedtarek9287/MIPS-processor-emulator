# ════════════════════════════════════════════════════════════════
#  Makefile — 5-Stage Pipelined MIPS Emulator
#  Usage:
#    make          Build the emulator
#    make test     Run all 10 test cases with pass/fail report
#    make clean    Remove build artefacts and log files
#    make install  Install binary to PREFIX/bin  (default /usr/local)
# ════════════════════════════════════════════════════════════════

# ── Toolchain ────────────────────────────────────────────────────
CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS  :=

# ── Paths ────────────────────────────────────────────────────────
TARGET   := mips_emu
SRC      := mips_emu.cpp
TESTS    := $(sort $(wildcard tests/*.s))
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin

# ── Detect OS (for install path differences) ─────────────────────
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME), Darwin)
    INSTALL := install
else ifeq ($(findstring MINGW,$(UNAME)),MINGW)
    TARGET := mips_emu.exe
    INSTALL := cp
else
    INSTALL := install
endif

# ── Colour codes (suppressed when NO_COLOR is set) ───────────────
ifndef NO_COLOR
    RED   := \033[0;31m
    GRN   := \033[0;32m
    YEL   := \033[1;33m
    BLU   := \033[1;34m
    BOLD  := \033[1m
    NC    := \033[0m
else
    RED GRN YEL BLU BOLD NC :=
endif

# ════════════════════════════════════════════════════════════════
#  Primary targets
# ════════════════════════════════════════════════════════════════
.PHONY: all build test clean install uninstall help

all: build

build: $(TARGET)

$(TARGET): $(SRC)
	@echo "$(BLU)$(BOLD)Building $(TARGET)...$(NC)"
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)
	@echo "$(GRN)Build successful → $(TARGET)$(NC)"

# ════════════════════════════════════════════════════════════════
#  Test target — runs every .s file in tests/ and checks output
# ════════════════════════════════════════════════════════════════
test: $(TARGET)
	@echo ""
	@echo "$(BLU)$(BOLD)╔══════════════════════════════════════════════════╗$(NC)"
	@echo "$(BLU)$(BOLD)║   MIPS Pipeline Emulator — Test Suite            ║$(NC)"
	@echo "$(BLU)$(BOLD)╚══════════════════════════════════════════════════╝$(NC)"
	@echo ""
	@chmod +x run_tests.sh
	@./run_tests.sh
	@echo ""

# ════════════════════════════════════════════════════════════════
#  Verbose test — prints pipeline diagram for each test
# ════════════════════════════════════════════════════════════════
test-verbose: $(TARGET)
	@for t in $(TESTS); do \
	    echo "$(YEL)══ $$t ══$(NC)"; \
	    ./$(TARGET) $$t --verbose --stderr 2>&1; \
	    echo ""; \
	done

# ════════════════════════════════════════════════════════════════
#  Single-file run helper:  make run FILE=tests/test01_arithmetic.s
# ════════════════════════════════════════════════════════════════
run: $(TARGET)
ifndef FILE
	$(error Usage: make run FILE=<path/to/program.s>)
endif
	@echo "$(BLU)Running: $(FILE)$(NC)"
	./$(TARGET) $(FILE) --stderr

# ════════════════════════════════════════════════════════════════
#  Install / uninstall
# ════════════════════════════════════════════════════════════════
install: $(TARGET)
	@echo "$(BLU)Installing $(TARGET) to $(BINDIR)$(NC)"
	@mkdir -p $(BINDIR)
	$(INSTALL) -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "$(GRN)Installed → $(BINDIR)/$(TARGET)$(NC)"

uninstall:
	@echo "Removing $(BINDIR)/$(TARGET)"
	@rm -f $(BINDIR)/$(TARGET)
	@echo "$(GRN)Done$(NC)"

# ════════════════════════════════════════════════════════════════
#  Clean
# ════════════════════════════════════════════════════════════════
clean:
	@echo "$(YEL)Cleaning build artefacts...$(NC)"
	rm -f $(TARGET) mips_emu.exe
	rm -f registers.log *.log
	@echo "$(GRN)Clean done$(NC)"

# ════════════════════════════════════════════════════════════════
#  Help
# ════════════════════════════════════════════════════════════════
help:
	@echo ""
	@echo "$(BOLD)MIPS Pipeline Emulator — Makefile targets$(NC)"
	@echo ""
	@echo "  $(GRN)make$(NC)                   Build the emulator (default)"
	@echo "  $(GRN)make test$(NC)              Run all 10 test cases"
	@echo "  $(GRN)make test-verbose$(NC)      Run tests with per-cycle pipeline trace"
	@echo "  $(GRN)make run FILE=<x.s>$(NC)   Assemble and run a single file"
	@echo "  $(GRN)make install$(NC)           Install to $(BINDIR)"
	@echo "  $(GRN)make uninstall$(NC)         Remove from $(BINDIR)"
	@echo "  $(GRN)make clean$(NC)             Remove build artefacts"
	@echo ""
	@echo "  CXX=$(CXX)  CXXFLAGS=$(CXXFLAGS)"
	@echo "  PREFIX=$(PREFIX)"
	@echo ""
