# ─────────────────────────────────────────────────────────────
#  Test 06: Recursive Functions — fib(n) and gcd(a,b)
#  Expected output:
#    fib(0) = 0
#    ...
#    fib(10) = 55
#    gcd(48,18) = 6
#    gcd(100,75) = 25
# ─────────────────────────────────────────────────────────────
    .data
s_fib:  .asciiz "fib("
s_fib2: .asciiz ") = "
s_gcd:  .asciiz "gcd("
s_com:  .asciiz ","
s_eq:   .asciiz ") = "
nl:     .asciiz "\n"

    .text
main:
    addi $sp, $sp, -4
    sw   $ra, 0($sp)
    li   $s3, 0
fib_outer:
    bgt  $s3, 10, fib_done

    li   $v0, 4
    la   $a0, s_fib
    syscall
    li   $v0, 1
    move $a0, $s3
    syscall
    li   $v0, 4
    la   $a0, s_fib2
    syscall

    move $a0, $s3
    jal  fib
    move $s5, $v0          # save fib result BEFORE clobbering $v0

    li   $v0, 1
    move $a0, $s5
    syscall
    li   $v0, 4
    la   $a0, nl
    syscall

    addi $s3, $s3, 1
    j    fib_outer
fib_done:

    li   $a0, 48
    li   $a1, 18
    jal  gcd
    move $s4, $v0

    li   $v0, 4
    la   $a0, s_gcd
    syscall
    li   $v0, 1
    li   $a0, 48
    syscall
    li   $v0, 4
    la   $a0, s_com
    syscall
    li   $v0, 1
    li   $a0, 18
    syscall
    li   $v0, 4
    la   $a0, s_eq
    syscall
    li   $v0, 1
    move $a0, $s4
    syscall
    li   $v0, 4
    la   $a0, nl
    syscall

    li   $a0, 100
    li   $a1, 75
    jal  gcd
    move $s4, $v0

    li   $v0, 4
    la   $a0, s_gcd
    syscall
    li   $v0, 1
    li   $a0, 100
    syscall
    li   $v0, 4
    la   $a0, s_com
    syscall
    li   $v0, 1
    li   $a0, 75
    syscall
    li   $v0, 4
    la   $a0, s_eq
    syscall
    li   $v0, 1
    move $a0, $s4
    syscall
    li   $v0, 4
    la   $a0, nl
    syscall

    lw   $ra, 0($sp)
    addi $sp, $sp, 4
    li   $v0, 10
    syscall

# recursive fib(n)
fib:
    addi $sp, $sp, -12
    sw   $ra, 8($sp)
    sw   $s0, 4($sp)
    sw   $a0, 0($sp)
    slti $t0, $a0, 2
    bne  $t0, $zero, fib_base
    addi $a0, $a0, -1
    jal  fib
    move $s0, $v0
    lw   $a0, 0($sp)
    addi $a0, $a0, -2
    jal  fib
    add  $v0, $v0, $s0
    j    fib_ret
fib_base:
    move $v0, $a0
fib_ret:
    lw   $ra, 8($sp)
    lw   $s0, 4($sp)
    addi $sp, $sp, 12
    jr   $ra

# iterative gcd(a=$a0, b=$a1)
gcd:
    addi $sp, $sp, -8
    sw   $s0, 4($sp)
    sw   $s1, 0($sp)
    move $s0, $a0
    move $s1, $a1
gcd_loop:
    beq  $s1, $zero, gcd_done
    div  $s0, $s1
    mfhi $s0
    move $t0, $s1
    move $s1, $s0
    move $s0, $t0
    j    gcd_loop
gcd_done:
    move $v0, $s0
    lw   $s0, 4($sp)
    lw   $s1, 0($sp)
    addi $sp, $sp, 8
    jr   $ra
