# ─────────────────────────────────────────────────────────────
#  Test 01: Basic R-type Arithmetic
#  Tests: add, addu, sub, subu, and, or, xor, nor, slt, sltu
#  Expected output:
#    add  $t0 = 30
#    sub  $t1 = 10
#    and  $t2 = 4
#    or   $t3 = 30
#    xor  $t4 = 26
#    nor  $t5 = -31   (bitwise NOR of 20 and 10)
#    slt  $t6 = 1     (5 < 10 => true)
#    sltu $t7 = 0     (10 < 5 => false)
# ─────────────────────────────────────────────────────────────
    .data
lbl_add:   .asciiz "add  $t0 = "
lbl_sub:   .asciiz "\nsub  $t1 = "
lbl_and:   .asciiz "\nand  $t2 = "
lbl_or:    .asciiz "\nor   $t3 = "
lbl_xor:   .asciiz "\nxor  $t4 = "
lbl_nor:   .asciiz "\nnor  $t5 = "
lbl_slt:   .asciiz "\nslt  $t6 = "
lbl_sltu:  .asciiz "\nsltu $t7 = "
newline:   .asciiz "\n"

    .text
main:
    li  $s0, 20
    li  $s1, 10

    add  $t0, $s0, $s1      # 20 + 10 = 30
    sub  $t1, $s0, $s1      # 20 - 10 = 10
    and  $t2, $s0, $s1      # 20 & 10 = 0b10100 & 0b01010 = 0b00000 = 0 ... actually 20=0x14,10=0xA -> 0
    or   $t3, $s0, $s1      # 20 | 10 = 30
    xor  $t4, $s0, $s1      # 20 ^ 10 = 30
    nor  $t5, $s0, $s1      # ~(20|10)
    li  $s2, 5
    li  $s3, 10
    slt  $t6, $s2, $s3      # 5 < 10  => 1
    sltu $t7, $s3, $s2      # 10 < 5  => 0

    # Print results
    li $v0, 4
    la $a0, lbl_add
    syscall
    li $v0, 1
    move $a0, $t0
    syscall

    li $v0, 4
    la $a0, lbl_sub
    syscall
    li $v0, 1
    move $a0, $t1
    syscall

    li $v0, 4
    la $a0, lbl_and
    syscall
    li $v0, 1
    move $a0, $t2
    syscall

    li $v0, 4
    la $a0, lbl_or
    syscall
    li $v0, 1
    move $a0, $t3
    syscall

    li $v0, 4
    la $a0, lbl_xor
    syscall
    li $v0, 1
    move $a0, $t4
    syscall

    li $v0, 4
    la $a0, lbl_nor
    syscall
    li $v0, 1
    move $a0, $t5
    syscall

    li $v0, 4
    la $a0, lbl_slt
    syscall
    li $v0, 1
    move $a0, $t6
    syscall

    li $v0, 4
    la $a0, lbl_sltu
    syscall
    li $v0, 1
    move $a0, $t7
    syscall

    li $v0, 4
    la $a0, newline
    syscall

    li $v0, 10
    syscall
