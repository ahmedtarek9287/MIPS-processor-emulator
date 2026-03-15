# ─────────────────────────────────────────────────────────────
#  Test 04: Branch Instructions & Loops
#  Tests: beq, bne, blez, bgtz, bltz, bgez, blt, bgt, ble, bge
#  Expected output:
#    beq  taken: YES
#    bne  taken: YES
#    blez taken: YES
#    bgtz taken: YES
#    bltz taken: YES
#    bgez taken: YES
#    loop sum 1..100 = 5050
# ─────────────────────────────────────────────────────────────
    .data
s_beq:  .asciiz "beq  taken: "
s_bne:  .asciiz "\nbne  taken: "
s_blez: .asciiz "\nblez taken: "
s_bgtz: .asciiz "\nbgtz taken: "
s_bltz: .asciiz "\nbltz taken: "
s_bgez: .asciiz "\nbgez taken: "
s_sum:  .asciiz "\nloop sum 1..100 = "
yes:    .asciiz "YES"
no:     .asciiz "NO"
nl:     .asciiz "\n"

    .text
main:
    li   $v0, 4
    la   $a0, s_beq
    syscall
    li   $t0, 7
    li   $t1, 7
    beq  $t0, $t1, beq_yes
    li   $v0, 4
    la   $a0, no
    syscall
    j    beq_done
beq_yes:
    li   $v0, 4
    la   $a0, yes
    syscall
beq_done:

    li   $v0, 4
    la   $a0, s_bne
    syscall
    li   $t2, 99
    bne  $t0, $t2, bne_yes
    li   $v0, 4
    la   $a0, no
    syscall
    j    bne_done
bne_yes:
    li   $v0, 4
    la   $a0, yes
    syscall
bne_done:

    li   $v0, 4
    la   $a0, s_blez
    syscall
    li   $t3, 0
    blez $t3, blez_yes
    li   $v0, 4
    la   $a0, no
    syscall
    j    blez_done
blez_yes:
    li   $v0, 4
    la   $a0, yes
    syscall
blez_done:

    li   $v0, 4
    la   $a0, s_bgtz
    syscall
    li   $t4, 1
    bgtz $t4, bgtz_yes
    li   $v0, 4
    la   $a0, no
    syscall
    j    bgtz_done
bgtz_yes:
    li   $v0, 4
    la   $a0, yes
    syscall
bgtz_done:

    li   $v0, 4
    la   $a0, s_bltz
    syscall
    li   $t5, -1
    bltz $t5, bltz_yes
    li   $v0, 4
    la   $a0, no
    syscall
    j    bltz_done
bltz_yes:
    li   $v0, 4
    la   $a0, yes
    syscall
bltz_done:

    li   $v0, 4
    la   $a0, s_bgez
    syscall
    li   $t6, 0
    bgez $t6, bgez_yes
    li   $v0, 4
    la   $a0, no
    syscall
    j    bgez_done
bgez_yes:
    li   $v0, 4
    la   $a0, yes
    syscall
bgez_done:

    li   $v0, 4
    la   $a0, s_sum
    syscall
    li   $s0, 0
    li   $s1, 1
    li   $s2, 100
loop:
    add  $s0, $s0, $s1
    addi $s1, $s1, 1
    ble  $s1, $s2, loop
    li   $v0, 1
    move $a0, $s0
    syscall

    li   $v0, 4
    la   $a0, nl
    syscall
    li   $v0, 10
    syscall
