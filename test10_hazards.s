# ─────────────────────────────────────────────────────────────
#  Test 10: Pipeline Hazards — Load-Use Stall & Forwarding
#  Expected output:
#    fwd chain = 10
#    load-use  = 42
#    chain lw  = 84
# ─────────────────────────────────────────────────────────────
    .data
val1:  .word 42
val2:  .word 42
s_fwd: .asciiz "fwd chain = "
s_lu:  .asciiz "\nload-use  = "
s_ch:  .asciiz "\nchain lw  = "
nl:    .asciiz "\n"

    .text
main:
    li   $t0, 1
    add  $t1, $t0, $t0
    add  $t2, $t1, $t1
    add  $t3, $t2, $t2
    add  $t4, $t3, $t1

    li   $v0, 4
    la   $a0, s_fwd
    syscall
    li   $v0, 1
    move $a0, $t4
    syscall

    la   $s0, val1
    lw   $s1, 0($s0)
    add  $s2, $s1, $zero

    li   $v0, 4
    la   $a0, s_lu
    syscall
    li   $v0, 1
    move $a0, $s2
    syscall

    la   $s0, val1
    lw   $s3, 0($s0)
    lw   $s4, 4($s0)
    add  $s5, $s3, $s4

    li   $v0, 4
    la   $a0, s_ch
    syscall
    li   $v0, 1
    move $a0, $s5
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall
