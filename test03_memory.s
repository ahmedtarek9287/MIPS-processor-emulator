# ─────────────────────────────────────────────────────────────
#  Test 03: Memory Operations — lw, sw, lh, sh, lb, sb, lbu, lhu
#  Expected output:
#    lw  word  = 305419896
#    sw  readback = 305419896
#    lbu byte  = 18
#    lb  signed= 18
#    lhu half  = 4660
#    sb  byte  = 171
#    sh  half  = 43981
# ─────────────────────────────────────────────────────────────
    .data
buf:   .space 32
s_lw:  .asciiz "lw  word  = "
s_sw:  .asciiz "\nsw  readback = "
s_lbu: .asciiz "\nlbu byte  = "
s_lb:  .asciiz "\nlb  signed= "
s_lhu: .asciiz "\nlhu half  = "
s_sb:  .asciiz "\nsb  byte  = "
s_sh:  .asciiz "\nsh  half  = "
nl:    .asciiz "\n"

    .text
main:
    la   $s0, buf

    li   $t0, 0x12345678
    sw   $t0, 0($s0)
    lw   $t1, 0($s0)

    lb   $t2, 0($s0)
    lbu  $t3, 0($s0)
    lh   $t4, 0($s0)
    lhu  $t5, 0($s0)

    li   $t6, 0xAB
    sb   $t6, 8($s0)
    lbu  $t7, 8($s0)

    li   $s1, 0xABCD
    sh   $s1, 12($s0)
    lhu  $s2, 12($s0)

    li   $v0, 4
    la   $a0, s_lw
    syscall
    li   $v0, 1
    move $a0, $t1
    syscall

    li   $v0, 4
    la   $a0, s_sw
    syscall
    li   $v0, 1
    move $a0, $t0
    syscall

    li   $v0, 4
    la   $a0, s_lbu
    syscall
    li   $v0, 1
    move $a0, $t3
    syscall

    li   $v0, 4
    la   $a0, s_lb
    syscall
    li   $v0, 1
    move $a0, $t2
    syscall

    li   $v0, 4
    la   $a0, s_lhu
    syscall
    li   $v0, 1
    move $a0, $t5
    syscall

    li   $v0, 4
    la   $a0, s_sb
    syscall
    li   $v0, 1
    move $a0, $t7
    syscall

    li   $v0, 4
    la   $a0, s_sh
    syscall
    li   $v0, 1
    move $a0, $s2
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall
