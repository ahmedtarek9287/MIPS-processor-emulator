# ─────────────────────────────────────────────────────────────
#  Test 05: Subroutine Calls — jal, jr, stack conventions
#  Tests: jal, jr, sw/lw stack frames, nested calls
#  Expected output:
#    factorial(6) = 720
#    power(2,10)  = 1024
# ─────────────────────────────────────────────────────────────
    .data
s_fact: .asciiz "factorial(6) = "
s_pow:  .asciiz "\npower(2,10)  = "
nl:     .asciiz "\n"

    .text
main:
    li   $a0, 6
    jal  factorial
    move $s0, $v0

    li   $v0, 4
    la   $a0, s_fact
    syscall
    li   $v0, 1
    move $a0, $s0
    syscall

    li   $a0, 2
    li   $a1, 10
    jal  power
    move $s1, $v0

    li   $v0, 4
    la   $a0, s_pow
    syscall
    li   $v0, 1
    move $a0, $s1
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall

# factorial(n) iterative
factorial:
    addi $sp, $sp, -8
    sw   $s0, 4($sp)
    sw   $s1, 0($sp)
    move $s0, $a0
    li   $s1, 1
fact_loop:
    mult $s1, $s0
    mflo $s1
    addi $s0, $s0, -1
    bgtz $s0, fact_loop
    move $v0, $s1
    lw   $s0, 4($sp)
    lw   $s1, 0($sp)
    addi $sp, $sp, 8
    jr   $ra

# power(base=$a0, exp=$a1) iterative
power:
    addi $sp, $sp, -12
    sw   $ra, 8($sp)
    sw   $s0, 4($sp)
    sw   $s1, 0($sp)
    move $s0, $a0
    move $s1, $a1
    li   $v0, 1
pow_loop:
    beq  $s1, $zero, pow_done
    mult $v0, $s0
    mflo $v0
    addi $s1, $s1, -1
    j    pow_loop
pow_done:
    lw   $ra, 8($sp)
    lw   $s0, 4($sp)
    lw   $s1, 0($sp)
    addi $sp, $sp, 12
    jr   $ra
