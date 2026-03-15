# ─────────────────────────────────────────────────────────────
#  Test 07: Multiply / Divide / HI-LO Registers
#  Tests: mult, multu, div, divu, mfhi, mflo
#  Expected output:
#    6 * 7       = 42
#    1000*1000   = 1000000
#    17/5  quot  = 3
#    17/5  rem   = 2
#    -17/5 quot  = -3
#    -17/5 rem   = -2
#    multu big   = 4294967295
# ─────────────────────────────────────────────────────────────
    .data
s1: .asciiz "6 * 7       = "
s2: .asciiz "\n1000*1000   = "
s3: .asciiz "\n17/5  quot  = "
s4: .asciiz "\n17/5  rem   = "
s5: .asciiz "\n-17/5 quot  = "
s6: .asciiz "\n-17/5 rem   = "
s7: .asciiz "\nmultu big   = "
nl: .asciiz "\n"

    .text
main:
    li   $t0, 6
    li   $t1, 7
    mult $t0, $t1
    mflo $t2

    li   $v0, 4
    la   $a0, s1
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $t0, 1000
    li   $t1, 1000
    mult $t0, $t1
    mflo $t2

    li   $v0, 4
    la   $a0, s2
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $t0, 17
    li   $t1, 5
    div  $t0, $t1
    mflo $t2
    mfhi $t3

    li   $v0, 4
    la   $a0, s3
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $v0, 4
    la   $a0, s4
    syscall
    li   $v0, 1
    move $a0, $t3
    syscall

    li   $t0, -17
    li   $t1, 5
    div  $t0, $t1
    mflo $t2
    mfhi $t3

    li   $v0, 4
    la   $a0, s5
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $v0, 4
    la   $a0, s6
    syscall
    li   $v0, 1
    move $a0, $t3
    syscall

    li   $t0, -1
    li   $t1, 1
    multu $t0, $t1
    mflo  $t2

    li   $v0, 4
    la   $a0, s7
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall
