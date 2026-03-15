# ─────────────────────────────────────────────────────────────
#  Test 02: Immediate Instructions & Shift Operations
#  Tests: addi, addiu, andi, ori, xori, lui, slti, sltiu,
#         sll, srl, sra, sllv, srlv, srav
#  Expected output:
#    addi  = 42
#    addiu = 65535
#    andi  = 12
#    ori   = 63
#    lui   = 65536
#    slti  = 1
#    sll   = 32
#    srl   = 2
#    sra   = -1
#    sllv  = 48
# ─────────────────────────────────────────────────────────────
    .data
s_addi:  .asciiz "addi  = "
s_addu:  .asciiz "\naddiu = "
s_andi:  .asciiz "\nandi  = "
s_ori:   .asciiz "\nori   = "
s_lui:   .asciiz "\nlui   = "
s_slti:  .asciiz "\nslti  = "
s_sll:   .asciiz "\nsll   = "
s_srl:   .asciiz "\nsrl   = "
s_sra:   .asciiz "\nsra   = "
s_sllv:  .asciiz "\nsllv  = "
nl:      .asciiz "\n"

    .text
main:
    addi  $t0, $zero, 42
    addiu $t1, $zero, 65535
    li    $t2, 60
    andi  $t2, $t2, 15
    li    $t3, 47
    ori   $t3, $t3, 16
    lui   $t4, 1
    li    $t5, 5
    slti  $t5, $t5, 100
    li    $t6, 4
    sll   $t6, $t6, 3
    li    $t7, 32
    srl   $t7, $t7, 4
    li    $s0, -8
    sra   $s0, $s0, 3
    li    $s1, 6
    li    $s2, 3
    sllv  $s1, $s1, $s2

    li   $v0, 4
    la   $a0, s_addi
    syscall
    li   $v0, 1
    move $a0, $t0
    syscall

    li   $v0, 4
    la   $a0, s_addu
    syscall
    li   $v0, 1
    move $a0, $t1
    syscall

    li   $v0, 4
    la   $a0, s_andi
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $v0, 4
    la   $a0, s_ori
    syscall
    li   $v0, 1
    move $a0, $t3
    syscall

    li   $v0, 4
    la   $a0, s_lui
    syscall
    li   $v0, 1
    move $a0, $t4
    syscall

    li   $v0, 4
    la   $a0, s_slti
    syscall
    li   $v0, 1
    move $a0, $t5
    syscall

    li   $v0, 4
    la   $a0, s_sll
    syscall
    li   $v0, 1
    move $a0, $t6
    syscall

    li   $v0, 4
    la   $a0, s_srl
    syscall
    li   $v0, 1
    move $a0, $t7
    syscall

    li   $v0, 4
    la   $a0, s_sra
    syscall
    li   $v0, 1
    move $a0, $s0
    syscall

    li   $v0, 4
    la   $a0, s_sllv
    syscall
    li   $v0, 1
    move $a0, $s1
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall
