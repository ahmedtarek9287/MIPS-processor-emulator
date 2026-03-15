#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
#  run_tests.sh — MIPS Emulator Test Runner
#  Runs every test in tests/ against its expected output.
#  Usage:  ./run_tests.sh [--verbose]
# ═══════════════════════════════════════════════════════════════

EMU="./mips_emu"
TESTS_DIR="tests"
PASS=0
FAIL=0
VERBOSE=0
[ "$1" = "--verbose" ] && VERBOSE=1

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[1;33m'
BLU='\033[1;34m'
NC='\033[0m'

# ── Expected outputs ─────────────────────────────────────────────
declare -A EXPECTED

EXPECTED["test01_arithmetic.s"]="add  \$t0 = 30
sub  \$t1 = 10
and  \$t2 = 0
or   \$t3 = 30
xor  \$t4 = 30
nor  \$t5 = -31
slt  \$t6 = 1
sltu \$t7 = 0"

EXPECTED["test02_immediates_shifts.s"]="addi  = 42
addiu = -1
andi  = 12
ori   = 63
lui   = 65536
slti  = 1
sll   = 32
srl   = 2
sra   = -1
sllv  = 48"

EXPECTED["test03_memory.s"]="lw  word  = 305419896
sw  readback = 305419896
lbu byte  = 18
lb  signed= 18
lhu half  = 4660
sb  byte  = 171
sh  half  = 43981"

EXPECTED["test04_branches.s"]="beq  taken: YES
bne  taken: YES
blez taken: YES
bgtz taken: YES
bltz taken: YES
bgez taken: YES
loop sum 1..100 = 5050"

EXPECTED["test05_subroutines.s"]="factorial(6) = 720
power(2,10)  = 1024"

EXPECTED["test06_recursion.s"]="fib(0) = 0
fib(1) = 1
fib(2) = 1
fib(3) = 2
fib(4) = 3
fib(5) = 5
fib(6) = 8
fib(7) = 13
fib(8) = 21
fib(9) = 34
fib(10) = 55
gcd(48,18) = 6
gcd(100,75) = 25"

EXPECTED["test07_multiply_divide.s"]="6 * 7       = 42
1000*1000   = 1000000
17/5  quot  = 3
17/5  rem   = 2
-17/5 quot  = -3
-17/5 rem   = -2
multu big   = -1"

EXPECTED["test08_bubble_sort.s"]="Before: 64 25 12 22 11
After:  11 12 22 25 64"

EXPECTED["test09_strings.s"]="strlen = 5
copy   = Hello
reversed = olleH"

EXPECTED["test10_hazards.s"]="fwd chain = 10
load-use  = 42
chain lw  = 84"

# ── Header ──────────────────────────────────────────────────────
echo -e "${BLU}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BLU}║   MIPS Pipeline Emulator — Test Suite            ║${NC}"
echo -e "${BLU}╚══════════════════════════════════════════════════╝${NC}"
echo ""

if [ ! -f "$EMU" ]; then
    echo -e "${RED}ERROR: $EMU not found. Run 'make' first.${NC}"
    exit 1
fi

# ── Run each test ───────────────────────────────────────────────
for src in $(ls "$TESTS_DIR"/*.s 2>/dev/null | sort); do
    name=$(basename "$src")
    actual=$("$EMU" "$src" --stderr 2>/dev/null)
    actual=$(echo "$actual" | sed 's/[[:space:]]*$//')   # strip trailing whitespace per line

    expected="${EXPECTED[$name]}"

    if [ -z "$expected" ]; then
        echo -e "  ${YEL}[SKIP]${NC} $name  (no expected output defined)"
        continue
    fi

    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GRN}[PASS]${NC} $name"
        PASS=$((PASS + 1))
        if [ $VERBOSE -eq 1 ]; then
            echo "$actual" | sed 's/^/         /'
        fi
    else
        echo -e "  ${RED}[FAIL]${NC} $name"
        FAIL=$((FAIL + 1))
        if [ $VERBOSE -eq 1 ]; then
            echo -e "    ${YEL}Expected:${NC}"
            echo "$expected" | sed 's/^/      /'
            echo -e "    ${YEL}Got:${NC}"
            echo "$actual"   | sed 's/^/      /'
        else
            # Show first differing line
            diff_line=$(diff <(echo "$expected") <(echo "$actual") | head -4)
            echo "    $diff_line"
        fi
    fi
done

# ── Summary ─────────────────────────────────────────────────────
TOTAL=$((PASS + FAIL))
echo ""
echo -e "${BLU}══════════════════════════════════════════════════${NC}"
if [ $FAIL -eq 0 ]; then
    echo -e "  ${GRN}All $TOTAL tests passed!${NC}"
else
    echo -e "  ${GRN}$PASS passed${NC}  ${RED}$FAIL failed${NC}  out of $TOTAL"
fi
echo -e "${BLU}══════════════════════════════════════════════════${NC}"

exit $FAIL
