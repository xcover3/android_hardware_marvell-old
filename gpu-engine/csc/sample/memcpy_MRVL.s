    .text
    .global memcpy_MRVL

.macro  UNALIGNED_MEMCPY shift
        sub     r1, #(\shift)
        ldr     ip, [r1], #4

        tst     r0, #4
        beq                100f
        mov   r3, ip, lsr #(\shift * 8)
        ldr   ip, [r1], #4
        sub   r2, r2, #4
        orr   r3, r3, ip, asl #(32 - \shift * 8)
        str   r3, [r0], #4
100:
        tst     r0, #8
        beq                101f
        mov   r3, ip, lsr #(\shift * 8)
        ldmia r1!, {r4, ip}
        sub   r2, r2, #8
        orr   r3, r3, r4, asl #(32 - \shift * 8)
        mov   r4, r4, lsr #(\shift * 8)
        orr   r4, r4, ip, asl #(32 - \shift * 8)
        stmia r0!, {r3-r4}
101:
        cmp     r2, #32
        blt     2f
        pld     [r1, #48]
1:
        pld     [r1, #80]
        subs    r2, r2, #32
        blt                2f
        mov   r3, ip, lsr #(\shift * 8)
        ldmia r1!, {r4 - r10, ip}
        orr   r3, r3, r4, asl #(32 - \shift * 8)
        mov   r4, r4, lsr #(\shift * 8)
        orr   r4, r4, r5, asl #(32 - \shift * 8)
        mov   r5, r5, lsr #(\shift * 8)
        orr   r5, r5, r6, asl #(32 - \shift * 8)
        mov   r6, r6, lsr #(\shift * 8)
        orr   r6, r6, r7, asl #(32 - \shift * 8)
        mov   r7, r7, lsr #(\shift * 8)
        orr   r7, r7, r8, asl #(32 - \shift * 8)
        mov   r8, r8, lsr #(\shift * 8)
        orr   r8, r8, r9, asl #(32 - \shift * 8)
        mov   r9, r9, lsr #(\shift * 8)
        orr   r9, r9, r10, asl #(32 - \shift * 8)
        mov   r10, r10, lsr #(\shift * 8)
        orr   r10, r10, ip, asl #(32 - \shift * 8)
        stmia r0!, {r3 - r10}
        bgt     1b

2:      /* copy remaining data */
                  pld [sp, #0]

        tst     r2, #16
                  beq               103f
        mov   r3, ip, lsr #(\shift * 8)
        ldmia r1!, {r4-r6, ip}
        orr   r3, r3, r4, asl #(32 - \shift * 8)
        mov   r4, r4, lsr #(\shift * 8)
        orr   r4, r4, r5, asl #(32 - \shift * 8)
        mov   r5, r5, lsr #(\shift * 8)
        orr   r5, r5, r6, asl #(32 - \shift * 8)
        mov   r6, r6, lsr #(\shift * 8)
        orr   r6, r6, ip, asl #(32 - \shift * 8)
        stmia r0!, {r3 - r6}
103:

        tst     r2, #8
        beq                105f
        mov   r3, ip, lsr #(\shift * 8)
        ldmia r1!, {r4, ip}
        orr   r3, r3, r4, asl #(32 - \shift * 8)
        mov   r4, r4, lsr #(\shift * 8)
        orr   r4, r4, ip, asl #(32 - \shift * 8)
        stmia r0!, {r3-r4}
105:

        tst     r2, #4
        beq                106f
        mov   r3, ip, lsr #(\shift * 8)
        ldr   ip, [r1], #4
        orr   r3, r3, ip, asl #(32 - \shift * 8)
        str   r3, [r0], #4
106:
        sub     r1, r1, #(4 - \shift)

        tst     r2, #2
                  beq               107f
        ldrb  r3, [r1], #1
        ldrb  r4, [r1], #1
        strb  r3, [r0], #1
        strb  r4, [r0], #1
107:

        tst     r2, #1
        beq                108f
        ldrb  r3, [r1], #1
        strb  r3, [r0], #1
108:

        ldmfd     sp!, {r0, r4, r5, r6, r7, r8, r9, r10}

        bx      lr
.endm


        .align
memcpy_MRVL:

        cmp     r2, #20
        blt     memcpy_small_block

        stmfd     sp!, {r0, r4, r5, r6, r7, r8, r9, r10}

                  ands     ip, r0, #3
              pld      [r1, #0]
                  bne      9f
                  ands     ip, r1, #3
                  bne      10f

memcpy_dst_4bytes_aligned_src0:
        /* both source and destination are 4 bytes aligned */
1:                subs     r2, r2, #(32)
                  blt      5f

              pld      [r1, #28]
              subs     r2, r2, #64
              blt      3f
2:            pld      [r1, #60]
              pld      [r1, #92]
                  ldmia    r1!, {r3 - r10}
                  subs     r2, r2, #32
                  blt               100f
                  stmia    r0!, {r3 - r10}
                  ldmia    r1!, {r3 - r10}
                  subs     r2, r2, #32
100:
                  pld [sp, #0]

                  stmia    r0!, {r3 - r10}
                  bge      2b
3:            ldmia    r1!, {r3 - r10}
              adds     r2, r2, #32
              blt               101f
              stmia    r0!, {r3 - r10}
              ldmia    r1!, {r3 - r10}
101: stmia    r0!, {r3 - r10}
                  beq               108f

5:                tst               r2, #16
                  beq               6f
                  ldmia    r1!, {r3, r4, r5, r6}
                  stmia    r0!, {r3, r4, r5, r6}

6:                tst               r2, #8
                  beq               7f
                  ldmia    r1!, {r3, r4}
                  stmia    r0!, {r3, r4}

7:                tst               r2, #4
                  beq               8f
                  ldr               r3, [r1], #4
                  str               r3, [r0], #4

8:      tst     r2, #2
                  beq               107f
        ldrb  r3, [r1], #1
        ldrb  r4, [r1], #1
        strb  r3, [r0], #1
        strb  r4, [r0], #1
107:

        tst     r2, #1
        beq                108f
        ldrb  r3, [r1], #1
        strb  r3, [r0], #1

108:
                  ldmfd    sp!, {r0, r4, r5, r6, r7, r8, r9, r10}
                  bx lr

        /* copy data until destination address is 4 bytes aligned */
9:                tst     r0, #1
                  beq               100f
        ldrb  r3, [r1], #1
        sub   r2, r2, #1
        strb  r3, [r0], #1
100:
                  tst     r0, #2
                  beq               101f
        ldrb  r3, [r1], #1
        ldrb  r4, [r1], #1
        sub   r2, r2, #2
        orr   r3, r3, r4, asl #8
        strh  r3, [r0], #2
101:
                  ands     ip, r1, #3
                  beq               memcpy_dst_4bytes_aligned_src0

        /* destination address is 4 bytes aligned */
        /* now we should handle 4 cases of source address alignment */
10:               tst     r1, #1
        bne     memcpy_dst_4bytes_aligned_src1or3

memcpy_dst_4bytes_aligned_src2:
        UNALIGNED_MEMCPY 2

memcpy_dst_4bytes_aligned_src1or3:
        tst    r1, #2
        bne    memcpy_dst_4bytes_aligned_src3
memcpy_dst_4bytes_aligned_src1:
        UNALIGNED_MEMCPY 1

memcpy_dst_4bytes_aligned_src3:
        UNALIGNED_MEMCPY 3

memcpy_small_block:
        stmfd  sp!, {r0, r4}

1:      subs   r2, r2, #3
                  blt         100f
        ldrb ip, [r0]
        ldrb r3, [r1], #1
        ldrb r4, [r1], #1
        ldrb ip, [r1], #1
        strb r3, [r0], #1
        strb r4, [r0], #1
        strb ip, [r0], #1
        b    1b

100:
        adds   r2, r2, #2
        mov    ip, r0
        beq    101f
        blt    102f

        ldrb r3, [r1], #1
        strb r3, [ip], #1
101:
        ldrb r3, [r1], #1
        strb r3, [ip], #1
102:

                  ldmfd sp!, {r0, r4}
        bx     lr
