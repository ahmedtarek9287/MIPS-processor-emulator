# ─────────────────────────────────────────────────────────────
#  Test 09: String Operations
#  Tests: lb, sb, null-terminated strings, strlen, strcpy, strrev
#  Expected output:
#    strlen = 5
#    copy   = Hello
#    reversed = olleH
# ─────────────────────────────────────────────────────────────
    .data
src:   .asciiz "Hello"
dst:   .space  16
rev:   .space  16
s_len: .asciiz "strlen = "
s_cpy: .asciiz "\ncopy   = "
s_rev: .asciiz "\nreversed = "
nl:    .asciiz "\n"

    .text
main:
    la   $a0, src
    jal  strlen
    move $s0, $v0

    li   $v0, 4
    la   $a0, s_len
    syscall
    li   $v0, 1
    move $a0, $s0
    syscall

    la   $a0, dst
    la   $a1, src
    jal  strcpy

    li   $v0, 4
    la   $a0, s_cpy
    syscall
    li   $v0, 4
    la   $a0, dst
    syscall

    la   $a0, rev
    la   $a1, src
    move $a2, $s0
    jal  strrev

    li   $v0, 4
    la   $a0, s_rev
    syscall
    li   $v0, 4
    la   $a0, rev
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall

strlen:
    move $t0, $a0
    li   $v0, 0
sl_loop:
    lb   $t1, 0($t0)
    beq  $t1, $zero, sl_done
    addi $v0, $v0, 1
    addi $t0, $t0, 1
    j    sl_loop
sl_done:
    jr   $ra

strcpy:
    lb   $t0, 0($a1)
    sb   $t0, 0($a0)
    beq  $t0, $zero, sc_done
    addi $a0, $a0, 1
    addi $a1, $a1, 1
    j    strcpy
sc_done:
    jr   $ra

strrev:
    add  $t0, $a1, $a2
    addi $t0, $t0, -1
    move $t1, $a0
    move $t2, $a2
sr_loop:
    beq  $t2, $zero, sr_done
    lb   $t3, 0($t0)
    sb   $t3, 0($t1)
    addi $t0, $t0, -1
    addi $t1, $t1, 1
    addi $t2, $t2, -1
    j    sr_loop
sr_done:
    sb   $zero, 0($t1)
    jr   $ra
