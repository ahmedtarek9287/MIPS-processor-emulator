# ─────────────────────────────────────────────────────────────
#  Test 08: Array Sorting — Bubble Sort
#  Tests: array access, computed offsets, nested loops, lw/sw
#  Expected output:
#    Before: 64 25 12 22 11
#    After:  11 12 22 25 64
# ─────────────────────────────────────────────────────────────
    .data
arr:   .word 64, 25, 12, 22, 11
s_bef: .asciiz "Before: "
s_aft: .asciiz "After:  "
space: .asciiz " "
nl:    .asciiz "\n"

    .text
main:
    la   $s0, arr
    li   $s1, 5

    li   $v0, 4
    la   $a0, s_bef
    syscall
    jal  print_array

    li   $t0, 0
outer:
    bge  $t0, $s1, sort_done
    li   $t1, 0
    sub  $t3, $s1, $t0
    addi $t3, $t3, -1
inner:
    bge  $t1, $t3, inner_done
    sll  $t4, $t1, 2
    add  $t4, $t4, $s0
    lw   $t5, 0($t4)
    lw   $t6, 4($t4)
    ble  $t5, $t6, no_swap
    sw   $t6, 0($t4)
    sw   $t5, 4($t4)
no_swap:
    addi $t1, $t1, 1
    j    inner
inner_done:
    addi $t0, $t0, 1
    j    outer
sort_done:

    li   $v0, 4
    la   $a0, s_aft
    syscall
    jal  print_array

    li   $v0, 10
    syscall

print_array:
    addi $sp, $sp, -16
    sw   $ra, 12($sp)
    sw   $s2, 8($sp)
    li   $s2, 0
pa_loop:
    bge  $s2, $s1, pa_done
    sll  $t0, $s2, 2
    add  $t0, $t0, $s0
    lw   $a0, 0($t0)
    li   $v0, 1
    syscall
    li   $v0, 4
    la   $a0, space
    syscall
    addi $s2, $s2, 1
    j    pa_loop
pa_done:
    li   $v0, 4
    la   $a0, nl
    syscall
    lw   $ra, 12($sp)
    lw   $s2, 8($sp)
    addi $sp, $sp, 16
    jr   $ra
