@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@
@    Copyright (c) 2012 - 2013 by Marvell International Ltd. and its affiliates.
@    All rights reserved.
@
@    This software file (the "File") is owned and distributed by Marvell
@    International Ltd. and/or its affiliates ("Marvell") under Marvell Commercial
@    License.
@
@    If you received this File from Marvell and you have entered into a commercial
@    license agreement (a "Commercial License") with Marvell, the File is licensed
@    to you under the terms of the applicable Commercial License.
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#include "ARGB8888ToYUV.h"

    .text


    @@
    @@ Assignments & Macros
    @@
    QQ10      = 10 @ assuse QBITS=10
    WORDSIZE  = 4  @ sizeof(Ipp32s)
    HWORDSIZE = 2  @ sizeof(Ipp16u)

    .extern RGB_YUV_Q10  @ Import RGB->YUV coefficients
    .extern RGB_YUV_GC
    .extern g_Formula
    .global gpu_csc_ARGBToI420
    .global gpu_csc_ARGBToNV21
    .global gpu_csc_ARGBToNV12
    .global gpu_csc_ARGBToUYVY
    .global gpu_csc_RGBToNV12

    .align 5
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Purpose:    interlived ARGB -> planed YUV
@
@ void ARGB8888_I420(
@          const Ipp8u* pSrc, int srcSL,
@          Ipp8u* pDst[3], int dstSL[3],
@          int width, int height)
@
@   ARM Registers Assignment:
@       r0          :   pSrc, pSrcLine 1
@       r1          :   U-plane line
@       r2          :   V-plane line
@       r3          :   Y-plane line 1
@       r4          :   pSrcLine 2
@       r5          :   Y-plane line 2
@       r6          :   width
@       r7          :   height
@       r8          :   srcstride
@       r9          :   Y-plane stride
@       r10         :   U-plane stride
@       r11         :   V-plane stride
@       r12         :   width counter
@       lr          :   UV descaler
@   NEON registers Assignment:
@       d31         :   [K0 K0 K0 K0]
@       d30         :   [K1 K1 K1 K1]
@       d29         :   [K2 K2 K2 K2]
@       d28         :   [K3 K3 K3 K3]
@       d27         :   [K4 K4 K4 K4]
@       d26         :   [0x80 0x80 0x80 0x80]
@       q12         :   temp value
@       q11         :   temp value
@       d21         :   [0 K4 0  K3]
@       d20         :   [0 K1 K2 K3]
@       q9          :   temp value
@       q8          :   temp value
@       q3          :   temp value
@       q2          :   temp value
@       q1          :   temp value
@       q0          :   temp value
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

gpu_csc_ARGBToI420:
    LDR         r12,  =g_Formula
    STMDB       sp!,  {r4 - r11, lr}                    @ push registers
    LDR         r12,  [r12]
    TST         r12,  #0x10
    BEQ         gpu_csc_ARGBToI420_GC

    LDR         lr,   =RGB_YUV_Q10                      @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    LDRH        r9,   [lr,  #0*HWORDSIZE]               @ RGB_YUV[0] = K[0]
    LDRH        r10,  [lr,  #1*HWORDSIZE]               @ RGB_YUV[1] = K[1]
    LDRH        r11,  [lr,  #2*HWORDSIZE]               @ RGB_YUV[2] = K[2]
    LDRH        r4,   [lr,  #3*HWORDSIZE]               @ RGB_YUV[3] = K[3]
    LDRH        r5,   [lr,  #4*HWORDSIZE]               @ RGB_YUV[4] = K[4]

    VDUP.u16    d31,  r9                                @ [K0 K0 K0 K0]
    VDUP.u16    d30,  r10                               @ [K1 K1 K1 K1]
    VDUP.u16    d29,  r11                               @ [K2 K2 K2 K2]
    VDUP.u16    d28,  r4                                @ [K3 K3 K3 K3]
    VDUP.u16    d27,  r5                                @ [K4 K4 K4 K4]

    ADD         r11,  r11,  r10,  LSL #16
    VMOV.u16    d26,  #0x80                             @ [0x80 0x80 0x80 0x80]
    VMOV.u32    d20,  r11,  r9                          @ [0 k0 k1 k2]
    VMOV.u32    d21,  r4,   r5                          @ [0 k4  0 k3]

    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ U-plane scanline size
    LDR         r11,  [r3,  #4*2]                       @ V-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter

    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)
    LDR         r1,   [r2,  #4*1]                       @ r1 U-plane address
    LDR         r2,   [r2,  #4*2]                       @ r2 V-plane address

    MOV         r8,   r4,   LSL #1                      @ source advancer
    SUB         r8,   r8,   r6,   LSL #2
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r6,   r6,   #1
    SUB         r10,  r10,  r6,   ASR #1                @ U-plane advancer
    SUB         r11,  r11,  r6,   ASR #1                @ V-plane advancer
    SUB         r6,   r6,   #1

    ADD         r4,   r0,   r4                          @ r0,r4 are points source line 1 and line 2
    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
ARGB8888_I420_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]
    PLD         [r2]

    CMP         r7,   #1                                @ test target height
    BEQ         ARGB8888_I420_last_line_head @ last line

    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_I420_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
ARGB8888_I420_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMOVL.u8    q12,  d2                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8
    VST1.8      d3,   [r3]!

    VLD4.8      {d4 - d7},  [r4]!                       @ read 2-nd source line:
    PLD         [r4,  #32]

    VMOVL.u8    q11,  d6                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VMULL.u16   q9,   d22,  d31                         @ R * K[0]
    VMULL.u16   q12,  d23,  d31
    VMOVL.u8    q11,  d5                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMLAL.u16   q9,   d22,  d30                         @ + G * K[1]
    VMLAL.u16   q12,  d23,  d30
    VMOVL.u8    q11,  d4                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q9,   d22,  d29                         @ + B * K[2]
    VMLAL.u16   q12,  d23,  d29

    VSHRN.u32   d18,  q9,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VSHRN.u32   d19,  q12,  #10
    VMOVN.u16   d7,   q9                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8
    VST1.8      d7,   [r5]!

ARGB8888_I420_line_loop_batch_getUV:
    VADD.u16    q8,   q9,   q8                          @ [Y07+Y17 Y06+Y16 Y05+Y15 Y04+Y14 Y03+Y13 Y02+Y12 Y01+Y11 Y00+Y10]
    VADDL.u8    q3,   d2,   d6                          @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q2,   d0,   d4                          @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]

    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01 + Y10 + Y11
    VPADD.u16   d2,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d0,   d4,   d5                          @ B00+B01+B10+B11

    VSUB.s16    d2,   d2,   d16                         @ R - Y
    VSUB.s16    d0,   d0,   d16                         @ B - Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #12                         @ >>12
    VSHRN.s32   d18,  q0,   #12                         @ >>12

    VADD.s16    d1,   d19,  d26                         @ +0x80
    VADD.s16    d0,   d18,  d26

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff [V0 V1 V2 V3 U0 U1 U2 U3]

    TST         r3,   #27
    BNE         ARGB8888_I420_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         ARGB8888_I420_line_loop_batch_skipPLD
    PLD         [r1,  #32]
    PLD         [r2,  #32]

ARGB8888_I420_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.32     d0[0],[r1]!                             @ store U
    VST1.32     d0[1],[r2]!                             @ store V

    BGE         ARGB8888_I420_line_loop_batch

ARGB8888_I420_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_I420_line_tail                 @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_I420_line_loop_lastpixel
ARGB8888_I420_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    VLDR        d2,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #8
    ADD         r4,   r4,   #8

    VMOVL.u8    q0,   d0                                @ [A01 R01 G01 B01 A00 R00 G00 B00]
    VMOVL.u8    q1,   d2                                @ [A11 R11 G11 B11 A10 R10 G10 B10]

    VMULL.u16   q8,   d0,   d20                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u16   q9,   d1,   d20                         @ [0 R01*K0 G01*K1 B01*K2]
    VMULL.u16   q11,  d2,   d20                         @ [0 R10*K0 G10*K1 B10*K2]
    VMULL.u16   q12,  d3,   d20                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u32   d4,   d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d7,   d24,  d25

    VPADD.u32   d4,   d4,   d5                          @ R*K0 + G*K1 + B*K2
    VPADD.u32   d5,   d6,   d7

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10
ARGB8888_I420_line_loop_nobatch_getUV:
    VADD.u16    q0,   q0,   q1
    VADD.u16    d0,   d0,   d1                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VPADDL.u16  d4,   d4
    VPADDL.u32  d4,   d4                                @ Y00+Y01+Y10+Y11
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #12                         @ >>12
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r2]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_I420_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_I420_line_tail

ARGB8888_I420_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    VLDR        s4,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #4
    ADD         r4,   r4,   #4

    VMOVL.u8    q0,   d0
    VMOVL.u8    q1,   d2

    VMULL.u16   q8,   d0,   d20                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u16   q11,  d2,   d20                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d4,   d4,   d6                          @ [Y10 Y00]

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y00
    VST1.8      d5[1],[r5]!                             @ store Y10

    VADD.u16    d0,   d0,   d2                          @ [A00+A10 R00+R10 G00+G10 B00+B10]
    VPADDL.u16  d4,   d4                                @ Y00+Y10
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r2]!                             @ store V

    B           ARGB8888_I420_line_tail

ARGB8888_I420_last_line_head:                           @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_I420_last_line_loop_batch_end

ARGB8888_I420_last_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMOVL.u8    q12,  d2                                @ R
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ G
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ B
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ Y >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8
    VST1.8      d3,   [r3]!

ARGB8888_I420_last_line_loop_batch_getUV:
    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01
    VPADDL.u8   d2,   d2                                @ R00+R01
    VPADDL.u8   d0,   d0                                @ B00+B01

    VSUB.s16    d2,   d2,   d16                         @ R -Y
    VSUB.s16    d0,   d0,   d16                         @ B -Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #11                         @ >>11
    VSHRN.s32   d18,  q0,   #11                         @ >>11

    VADD.s16    d1,   d19,  d26                         @ +0x80
    VADD.s16    d0,   d18,  d26

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         ARGB8888_I420_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         ARGB8888_I420_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
    PLD         [r2,  #32]
ARGB8888_I420_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.32     d0[0],[r1]!                             @ store dst
    VST1.32     d0[1],[r2]!                             @ store dst

    BGE         ARGB8888_I420_last_line_loop_batch

ARGB8888_I420_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_I420_line_tail                 @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_I420_last_line_loop_lastpixel
ARGB8888_I420_last_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMOVL.u8    q0,   d0

    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2
    VMULL.u16   q9,   d1,   d20

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d4,   d4,   d5

    VSHRN.u32   d4,   q2,   #10                         @ [Y1 Y0] >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!
ARGB8888_I420_last_line_loop_nobatch_getUV:
    VADD.u16    d0,   d0,   d1                          @ [A00+A01 R00+R01 G00+G01 B00+B01]
    VPADDL.u16  d4,   d4                                @ Y00+Y01
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r2]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_I420_last_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_I420_line_tail

ARGB8888_I420_last_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMOVL.u8    q0,   d0
    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d4,   d4,   d4

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y

    VDUP.u16    d4,   d4[0]
    VSUB.s16    d0,   d0,   d4
    VMULL.s16   q2,   d0,   d21
    VSHRN.s32   d4 ,  q2,   #10                         @ >>10
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r2]!                             @ store V
@@
@@ Switch next pair of lines
@@
ARGB8888_I420_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r2,   r2,   r11                         @ next V-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         ARGB8888_I420_line_head                 @ to the next pair of lines
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return

.ltorg
gpu_csc_ARGBToI420_GC_table_1:
    .word       0xff000204, 0xff000204
gpu_csc_ARGBToI420_GC_table_2:
    .word       0x03020504, 0xffff0100

    .align 5
gpu_csc_ARGBToI420_GC:
    LDR         lr,   =RGB_YUV_GC                       @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    VLDR        d2,   [lr]                              @ [16 K2 K1 K0]
    VLDR        d0,   [lr,  #4*HWORDSIZE]
    VLDR        d1,   [lr,  #8*HWORDSIZE]
    VLDR        d3,   gpu_csc_ARGBToI420_GC_table_1
    VLDR        d4,   gpu_csc_ARGBToI420_GC_table_2
    VMOV.i32    d27,  #0
    VMOV.i32    d24,  #0
    VDUP.8      d31,  d2[0]                             @ [K0 K0 K0 K0 K0 K0 K0 K0]
    VDUP.8      d30,  d2[2]                             @ [K1 K1 K1 K1 K1 K1 K1 K1]
    VDUP.8      d29,  d2[4]                             @ [K2 K2 K2 K2 K2 K2 K2 K2]
    VDUP.8      d28,  d2[6]                             @ [16 16 16 16 16 16 16 16]
    VTBL.8      d27,  {d2}, d3                          @ [0  K0 K1 K2 0  K0 K1 K2]
    VDUP.16     d26,  d0[3]                             @ [128 128 128 128]
    VTBL.8      d24,  {d0}, d4                          @ [0  K3 K4 K5]
    VTBL.8      d25,  {d1}, d4                          @ [0  K6 K7 K8]
    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ U-plane scanline size
    LDR         r11,  [r3,  #4*2]                       @ V-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter

    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)
    LDR         r1,   [r2,  #4*1]                       @ r1 U-plane address
    LDR         r2,   [r2,  #4*2]                       @ r2 V-plane address

    MOV         r8,   r4,   LSL #1                      @ source advancer
    SUB         r8,   r8,   r6,   LSL #2
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r6,   r6,   #1
    SUB         r10,  r10,  r6,   ASR #1                @ U-plane advancer
    SUB         r11,  r11,  r6,   ASR #1                @ V-plane advancer
    SUB         r6,   r6,   #1

    ADD         r4,   r0,   r4                          @ r0,r4 are points source line 1 and line 2
    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
ARGB8888_I420_GC_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]
    PLD         [r2]

    CMP         r7,   #1                                @ test target height
    BEQ         ARGB8888_I420_GC_last_line_head @ last line

    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_I420_GC_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
ARGB8888_I420_GC_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    VLD4.8      {d16- d19}, [r4]!                       @ read 2-nd source line:
    PLD         [r0,  #32]
    PLD         [r4,  #32]

    VMULL.u8    q11,  d6,   d31                         @ R * K[0]
    VMULL.u8    q10,  d18,  d31                         @ R * K[0]
    VADDL.u8    q3,   d6,   d18                         @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q1,   d5,   d17                         @ [G07+G17 G06+G16 G05+G15 G04+G14 G03+G13 G02+G12 G01+G11 G00+G10]
    VMLAL.u8    q11,  d5,   d30                         @ + G * K[1]
    VMLAL.u8    q10,  d17,  d30                         @ + G * K[1]
    VPADD.u16   d6,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d2,   d2,   d3                          @ G00+G01+G10+G11
    VMLAL.u8    q11,  d4,   d29                         @ + B * K[2]
    VMLAL.u8    q10,  d16,  d29                         @ + B * K[2]
    VADDL.u8    q2,   d4,   d16                         @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]
    VPADD.u16   d4,   d4,   d5                          @ B00+B01+B10+B11
    VMULL.s16   q8,   d6,   d0[0]                       @ R * K[3]
    VMULL.s16   q3,   d6,   d1[0]                       @ R * K[3]

    VRSHRN.u16  d22,  q11,  #8                          @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VRSHRN.u16  d20,  q10,  #8                          @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VMLAL.s16   q8,   d2,   d0[1]                       @ G * K[4]
    VMLAL.s16   q3,   d2,   d1[1]                       @ G * K[4]
    VADD.i8     d19,  d22,  d28
    VADD.i8     d20,  d20,  d28
    VST1.8      d19,  [r3]!
    VMLAL.s16   q8,   d4,   d0[2]                       @ B * K[5]
    VMLAL.s16   q3,   d4,   d1[2]                       @ B * K[5]
    VST1.8      d20,  [r5]!

ARGB8888_I420_GC_line_loop_batch_getUV:
    VRSHRN.s32  d16,  q8,   #10                         @ >>10
    VRSHRN.s32  d6,   q3,   #10                         @ >>10
    VADD.s16    d4,   d16,  d26
    VADD.s16    d5,   d6,   d26
    VMOVN.u16   d4,   q2

    TST         r3,   #27
    BNE         ARGB8888_I420_GC_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         ARGB8888_I420_GC_line_loop_batch_skipPLD
    PLD         [r1,  #32]
    PLD         [r2,  #32]

ARGB8888_I420_GC_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.32     d4[0],[r1]!                             @ store U
    VST1.32     d4[1],[r2]!                             @ store V
    BGE         ARGB8888_I420_GC_line_loop_batch

ARGB8888_I420_GC_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_I420_GC_line_tail              @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_I420_GC_line_loop_lastpixel
ARGB8888_I420_GC_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    VLDR        d6,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #8
    ADD         r4,   r4,   #8

    VMULL.u8    q8,   d4,   d27                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u8    q1,   d6,   d27                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u16   d16,  d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u16   d2,   d2,   d3

    VPADD.u16   d16,  d16,  d2                          @ R*K0 + G*K1 + B*K2

    VRSHRN.u16  d5,   q8,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10

ARGB8888_I420_GC_line_loop_nobatch_getUV:
    VADDL.u8    q2,   d4,   d6
    VADD.u16    d4,   d4,   d5                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VADD.i32    d6,   d6,   d7
    VPADD.i32   d6,   d6,   d6
    VRSHRN.s32  d6 ,  q3,   #10                         @ >>10
    VADD.s16    d6,   d6,   d26                         @ +0x80
    VMOVN.u16   d6,   q3
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d4 ,  q2,   #10                         @ >>10
    VADD.s16    d7,   d4,   d26                         @ +0x80
    VST1.8      d6[0],[r1]!                             @ store U
    VST1.8      d6[4],[r2]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_I420_GC_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_I420_GC_line_tail

ARGB8888_I420_GC_line_loop_lastpixel:
    VLDR        s8,   [r0]                              @ read 1-nd source line
    VLDR        s10,  [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #4
    ADD         r4,   r4,   #4

    VMULL.u8    q8,   d4,   d27                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u8    q11,  d5,   d27                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u16   d16,  d16,  d16
    VPADD.u16   d22,  d22,  d22
    VPADD.u16   d16,  d16,  d22                         @ [Y10 Y00]

    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VADD.i8     d16,  d16,  d28
    VST1.8      d16[0],     [r3]!                       @ store Y00
    VST1.8      d16[2],     [r5]!                       @ store Y10

    VADDL.u8    q2,   d4,   d5                          @ [A00+A10 R00+R10 G00+G10 B00+B10]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VADD.i32    d6,   d6,   d7
    VPADD.i32   d6,   d6,   d6
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VADD.s16    d6,   d6,   d26                         @ +0x80

    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d7,   d4,   d26                         @ +0x80
    VMOVN.u16   d6,   q3
    VST1.8      d6[0],[r1]!                             @ store U
    VST1.8      d6[4],[r2]!                             @ store V

    B           ARGB8888_I420_GC_line_tail

ARGB8888_I420_GC_last_line_head:                        @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_I420_GC_last_line_loop_batch_end

ARGB8888_I420_GC_last_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMULL.u8    q8,   d6,   d31                         @ R * K[0]
    VMLAL.u8    q8,   d5,   d30                         @ + G * K[1]
    VMLAL.u8    q8,   d4,   d29                         @ + B * K[2]

    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VADD.i8     d16,  d16,  d28
    VST1.8      d16,  [r3]!

ARGB8888_I420_GC_last_line_loop_batch_getUV:
    VPADDL.u8   d6,   d6                                @ R00+R01
    VPADDL.u8   d5,   d5                                @ G00+G01
    VPADDL.u8   d4,   d4                                @ B00+B01

    VMULL.s16   q8,   d6,   d0[0]                       @ R * K[3]
    VMLAL.s16   q8,   d5,   d0[1]                       @ G * K[4]
    VMLAL.s16   q8,   d4,   d0[2]                       @ B * K[5]
    VRSHRN.s32  d18,  q8,   #9                          @ >>9
    VADD.s16    d8,   d18,  d26                         @ +0x80

    VMULL.s16   q8,   d6,   d1[0]                       @ R * K[6]
    VMLAL.s16   q8,   d5,   d1[1]                       @ G * K[7]
    VMLAL.s16   q8,   d4,   d1[2]                       @ B * K[8]
    VRSHRN.s32  d18,  q8,   #9                          @ >>9
    VADD.s16    d9,   d18,  d26

    VMOVN.s16   d2,   q4                                @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         ARGB8888_I420_GC_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         ARGB8888_I420_GC_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
    PLD         [r2,  #32]
ARGB8888_I420_GC_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.32     d2[0],[r1]!                             @ store dst
    VST1.32     d2[1],[r2]!                             @ store dst
    BGE         ARGB8888_I420_GC_last_line_loop_batch

ARGB8888_I420_GC_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_I420_GC_line_tail              @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_I420_GC_last_line_loop_lastpixel
ARGB8888_I420_GC_last_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMULL.u8    q8,   d4,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d6,   d16,  d17
    VPADD.u16   d6,   d6,   d6

    VRSHRN.u16  d5,   q3,   #8                          @ [Y1 Y0] >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.16     d5[0],[r3]!
ARGB8888_I420_GC_last_line_loop_nobatch_getUV:
    VMOVL.u8    q2,   d4
    VADD.u16    d4,   d4,   d5                          @ [A00+A01 R00+R01 G00+G01 B00+B01]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VADD.i32    d6,   d6,   d7
    VPADD.i32   d6,   d6,   d6
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VADD.s16    d6,   d6,   d26                         @ +0x80

    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d7,   d4,   d26                         @ +0x80

    VMOVN.u16   d4,   q3
    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[4],[r2]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_I420_GC_last_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_I420_GC_line_tail

ARGB8888_I420_GC_last_line_loop_lastpixel:
    VLDR        s4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMULL.u8    q8,   d2,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d4,   d16,  d17
    VPADD.u16   d4,   d4,   d4

    VRSHRN.u16  d5,   q2,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.8      d5[0],[r3]!                             @ store Y

    VMOVL.u8    q1,   d2
    VMULL.s16   q3,   d2,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VADD.i32    d6,   d6,   d7
    VPADD.i32   d6,   d6,   d6
    VRSHRN.s32  d6 ,  q3,   #8                          @ >>8
    VADD.s16    d6,   d6,   d26                         @ +0x80

    VMULL.s16   q2,   d2,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d4 ,  q2,   #8                          @ >>8
    VADD.s16    d7,   d4,   d26                         @ +0x80
    VMOVN.u16   d4,   q3
    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[4],[r2]!                             @ store V
@@
@@ Switch next pair of lines
@@
ARGB8888_I420_GC_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r2,   r2,   r11                         @ next V-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         ARGB8888_I420_GC_line_head              @ to the next pair of lines
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return
@   ENDP        @gpu_csc_ARGBToI420


    .align 5
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Purpose:    interlived ARGB -> NV21
@
@ void gpu_csc_ARGBToNV21(
@          const Ipp8u* pSrc, int srcSL,
@          Ipp8u* pDst[2], int dstSL[2],
@          int width, int height)
@
@   ARM Registers Assignment:
@       r0          :   pSrc, pSrcLine 1
@       r1          :   UV-plane line
@       r2          :
@       r3          :   Y-plane line 1
@       r4          :   pSrcLine 2
@       r5          :   Y-plane line 2
@       r6          :   width
@       r7          :   height
@       r8          :   srcstride
@       r9          :   Y-plane stride
@       r10         :   UV-plane stride
@       r11         :
@       r12         :   width counter
@       lr          :   UV descaler
@   NEON registers Assignment:
@       d31         :   [K0 K0 K0 K0]
@       d30         :   [K1 K1 K1 K1]
@       d29         :   [K2 K2 K2 K2]
@       d28         :   [K3 K3 K3 K3]
@       d27         :   [K4 K4 K4 K4]
@       d26         :   [0x80 0x80 0x80 0x80]
@       q12         :   temp value
@       q11         :   temp value
@       d21         :   [0 K4 0  K3]
@       d20         :   [0 K1 K2 K3]
@       q9          :   temp value
@       q8          :   temp value
@       q3          :   temp value
@       q2          :   temp value
@       q1          :   temp value
@       q0          :   temp value
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

gpu_csc_ARGBToNV21:
    LDR         r12,  =g_Formula
    STMDB       sp!,  {r4 - r11, lr}                    @ push registers
    LDR         r12,  [r12]
    TST         r12,  #0x10
    BEQ         gpu_csc_ARGBToNV21_GC

    LDR         lr,   =RGB_YUV_Q10                      @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    LDRH        r9,   [lr,  #0*HWORDSIZE]               @ RGB_YUV[0] = K[0]
    LDRH        r10,  [lr,  #1*HWORDSIZE]               @ RGB_YUV[1] = K[1]
    LDRH        r11,  [lr,  #2*HWORDSIZE]               @ RGB_YUV[2] = K[2]
    LDRH        r4,   [lr,  #3*HWORDSIZE]               @ RGB_YUV[3] = K[3]
    LDRH        r5,   [lr,  #4*HWORDSIZE]               @ RGB_YUV[4] = K[4]

    VDUP.u16    d31,  r9                                @ [K0 K0 K0 K0]
    VDUP.u16    d30,  r10                               @ [K1 K1 K1 K1]
    VDUP.u16    d29,  r11                               @ [K2 K2 K2 K2]
    VDUP.u16    d28,  r4                                @ [K3 K3 K3 K3]
    VDUP.u16    d27,  r5                                @ [K4 K4 K4 K4]

    ADD         r11,  r11,  r10,  LSL #16
    VMOV.u16    d26,  #0x80                             @ [0x80 0x80 0x80 0x80]
    VMOV.u32    d20,  r11,  r9                          @ [0 k0 k1 k2]
    VMOV.u32    d21,  r4,   r5                          @ [0 k4  0 k3]

    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ U-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter

    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)
    LDR         r1,   [r2,  #4*1]                       @ r1 U-plane address

    MOV         r8,   r4,   LSL #1                      @ source advancer
    SUB         r8,   r8,   r6,   LSL #2
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r6,   r6,   #1
    MOV         r11,  r6,   ASR #1
    SUB         r10,  r10,  r11,  LSL #1                @ U-plane advancer
    SUB         r6,   r6,   #1

    ADD         r4,   r0,   r4                          @ r0,r4 are points source line 1 and line 2
    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
gpu_csc_ARGBToNV21_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]

    CMP         r7,   #1                                @ test target height
    BEQ         gpu_csc_ARGBToNV21_last_line_head @ last line

    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_ARGBToNV21_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
gpu_csc_ARGBToNV21_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    VLD4.8      {d4 - d7},  [r4]!                       @ read 2-nd source line:
    PLD         [r0,  #32]
    PLD         [r4,  #32]

    VMOVL.u8    q12,  d2                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_32 >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8


    VMOVL.u8    q11,  d6                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VST1.8      d3,   [r3]!
    VMULL.u16   q9,   d22,  d31                         @ R * K[0]
    VMULL.u16   q12,  d23,  d31
    VMOVL.u8    q11,  d5                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMLAL.u16   q9,   d22,  d30                         @ + G * K[1]
    VMLAL.u16   q12,  d23,  d30
    VMOVL.u8    q11,  d4                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q9,   d22,  d29                         @ + B * K[2]
    VMLAL.u16   q12,  d23,  d29

    VSHRN.u32   d18,  q9,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_32 >> QBITS
    VSHRN.u32   d19,  q12,  #10
    VMOVN.u16   d7,   q9                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8

    VADD.u16    q8,   q9,   q8                          @ [Y07+Y17 Y06+Y16 Y05+Y15 Y04+Y14 Y03+Y13 Y02+Y12 Y01+Y11 Y00+Y10]
    VST1.8      d7,   [r5]!
    VADDL.u8    q3,   d2,   d6                          @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q2,   d0,   d4                          @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]

    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01 + Y10 + Y11
    VPADD.u16   d2,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d0,   d4,   d5                          @ B00+B01+B10+B11

    VSUB.s16    d2,   d2,   d16                         @ R - Y
    VSUB.s16    d0,   d0,   d16                         @ B - Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #12                         @ >>12
    VSHRN.s32   d18,  q0,   #12                         @ >>12

    VADD.s16    d1,   d19,  d26                         @ +0x80
    VADD.s16    d0,   d18,  d26

    VZIP.s16    d1,   d0

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         gpu_csc_ARGBToNV21_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         gpu_csc_ARGBToNV21_line_loop_batch_skipPLD
    PLD         [r1,  #32]

gpu_csc_ARGBToNV21_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VREV64.32   d0,   d0
    VST1.8      {d0}, [r1]!                             @ store VU

    BGE         gpu_csc_ARGBToNV21_line_loop_batch

gpu_csc_ARGBToNV21_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_ARGBToNV21_line_tail            @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_ARGBToNV21_line_loop_lastpixel
gpu_csc_ARGBToNV21_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    VLDR        d2,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #8
    ADD         r4,   r4,   #8

    VMOVL.u8    q0,   d0                                @ [A01 R01 G01 B01 A00 R00 G00 B00]
    VMOVL.u8    q1,   d2                                @ [A11 R11 G11 B11 A10 R10 G10 B10]

    VMULL.u16   q8,   d0,   d20                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u16   q9,   d1,   d20                         @ [0 R01*K0 G01*K1 B01*K2]
    VMULL.u16   q11,  d2,   d20                         @ [0 R10*K0 G10*K1 B10*K2]
    VMULL.u16   q12,  d3,   d20                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u32   d4,   d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d7,   d24,  d25

    VPADD.u32   d4,   d4,   d5                          @ R*K0 + G*K1 + B*K2
    VPADD.u32   d5,   d6,   d7

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10

gpu_csc_ARGBToNV21_line_loop_nobatch_getUV:
    VADD.u16    q0,   q0,   q1
    VADD.u16    d0,   d0,   d1                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VPADDL.u16  d4,   d4
    VPADDL.u32  d4,   d4                                @ Y00+Y01+Y10+Y11
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #12                         @ >>12
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[2],[r1]!                             @ store V
    VST1.8      d4[0],[r1]!                             @ store U

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_ARGBToNV21_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         gpu_csc_ARGBToNV21_line_tail

gpu_csc_ARGBToNV21_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    VLDR        s4,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #4
    ADD         r4,   r4,   #4

    VMOVL.u8    q0,   d0
    VMOVL.u8    q1,   d2

    VMULL.u16   q8,   d0,   d20                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u16   q11,  d2,   d20                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d4,   d4,   d6                          @ [Y10 Y00]

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y00
    VST1.8      d5[1],[r5]!                             @ store Y10

    VADD.u16    d0,   d0,   d2                          @ [A00+A10 R00+R10 G00+G10 B00+B10]
    VPADDL.u16  d4,   d4                                @ Y00+Y10
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[2],[r1]!                             @ store V
    VST1.8      d4[0],[r1]!                             @ store U

    B           gpu_csc_ARGBToNV21_line_tail

gpu_csc_ARGBToNV21_last_line_head:                      @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_ARGBToNV21_last_line_loop_batch_end

gpu_csc_ARGBToNV21_last_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMOVL.u8    q12,  d2                                @ R
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ G
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ B
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ Y >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8
    VST1.8      d3,   [r3]!

gpu_csc_ARGBToNV21_last_line_loop_batch_getUV:
    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01
    VPADDL.u8   d2,   d2                                @ R00+R01
    VPADDL.u8   d0,   d0                                @ B00+B01

    VSUB.s16    d2,   d2,   d16                         @ R -Y
    VSUB.s16    d0,   d0,   d16                         @ B -Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #11                         @ >>11
    VSHRN.s32   d18,  q0,   #11                         @ >>11

    VADD.s16    d1,   d19,  d26                         @ +0x80
    VADD.s16    d0,   d18,  d26

    VZIP.s16    d1,   d0

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         gpu_csc_ARGBToNV21_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         gpu_csc_ARGBToNV21_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
gpu_csc_ARGBToNV21_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VREV64.32   d0,   d0
    VST1.8      {d0}, [r1]!                             @ store dst

    BGE         gpu_csc_ARGBToNV21_last_line_loop_batch

gpu_csc_ARGBToNV21_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_ARGBToNV21_line_tail            @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_ARGBToNV21_last_line_loop_lastpixel
gpu_csc_ARGBToNV21_last_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMOVL.u8    q0,   d0

    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2
    VMULL.u16   q9,   d1,   d20

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d4,   d4,   d5

    VSHRN.u32   d4,   q2,   #10                         @ [Y1 Y0] >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!

gpu_csc_ARGBToNV21_last_line_loop_nobatch_getUV:
    VADD.u16    d0,   d0,   d1                          @ [A00+A01 R00+R01 G00+G01 B00+B01]
    VPADDL.u16  d4,   d4                                @ Y00+Y01
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[2],[r1]!                             @ store V
    VST1.8      d4[0],[r1]!                             @ store U

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_ARGBToNV21_last_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         gpu_csc_ARGBToNV21_line_tail

gpu_csc_ARGBToNV21_last_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMOVL.u8    q0,   d0
    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d4,   d4,   d4

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y

    VDUP.u16    d4,   d4[0]
    VSUB.s16    d0,   d0,   d4
    VMULL.s16   q2,   d0,   d21
    VSHRN.s32   d4 ,  q2,   #10                         @ >>10
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff

    VST1.8      d4[2],[r1]!                             @ store V
    VST1.8      d4[0],[r1]!                             @ store U
@@
@@ Switch next pair of lines
@@
gpu_csc_ARGBToNV21_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         gpu_csc_ARGBToNV21_line_head            @ to the next pair of lines
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return

.ltorg
gpu_csc_ARGBToNV21_GC_table_1:
    .word       0xff000204, 0xff000204
gpu_csc_ARGBToNV21_GC_table_2:
    .word       0x03020504, 0xffff0100

    .align 5
gpu_csc_ARGBToNV21_GC:
    LDR         lr,   =RGB_YUV_GC                       @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    VLDR        d2,   [lr]                              @ [16 K2 K1 K0]
    VLDR        d0,   [lr,  #4*HWORDSIZE]
    VLDR        d1,   [lr,  #8*HWORDSIZE]
    VLDR        d3,   gpu_csc_ARGBToNV21_GC_table_1
    VLDR        d4,   gpu_csc_ARGBToNV21_GC_table_2
    VMOV.i32    d27,  #0
    VMOV.i32    d24,  #0
    VDUP.8      d31,  d2[0]                             @ [K0 K0 K0 K0 K0 K0 K0 K0]
    VDUP.8      d30,  d2[2]                             @ [K1 K1 K1 K1 K1 K1 K1 K1]
    VDUP.8      d29,  d2[4]                             @ [K2 K2 K2 K2 K2 K2 K2 K2]
    VDUP.8      d28,  d2[6]                             @ [16 16 16 16 16 16 16 16]
    VTBL.8      d27,  {d2}, d3                          @ [0  K0 K1 K2 0  K0 K1 K2]
    VDUP.16     d26,  d0[3]                             @ [128 128 128 128]
    VTBL.8      d24,  {d0}, d4                          @ [0  K3 K4 K5]
    VTBL.8      d25,  {d1}, d4                          @ [0  K6 K7 K8]
    ADD         r11,  r11,  r10,  LSL #16
    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ UV-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter

    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)
    LDR         r1,   [r2,  #4*1]                       @ r1 UV-plane address

    MOV         r8,   r4,   LSL #1                      @ source advancer
    SUB         r8,   r8,   r6,   LSL #2
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r6,   r6,   #1
    MOV         r11,  r6,   ASR #1
    SUB         r10,  r10,  r11,  LSL #1                @ UV-plane advancer
    SUB         r6,   r6,   #1

    ADD         r4,   r0,   r4                          @ r0,r4 are points source line 1 and line 2
    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
ARGB8888_NV21_GC_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]

    CMP         r7,   #1                                @ test target height
    BEQ         ARGB8888_NV21_GC_last_line_head @ last line

    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_NV21_GC_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
ARGB8888_NV21_GC_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    VLD4.8      {d16- d19}, [r4]!                       @ read 2-nd source line:
    PLD         [r0,  #32]
    PLD         [r4,  #32]

    VMULL.u8    q11,  d6,   d31                         @ R * K[0]
    VMLAL.u8    q11,  d5,   d30                         @ + G * K[1]
    VMLAL.u8    q11,  d4,   d29                         @ + B * K[2]

    VADDL.u8    q3,   d6,   d18                         @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q1,   d5,   d17                         @ [G07+G17 G06+G16 G05+G15 G04+G14 G03+G13 G02+G12 G01+G11 G00+G10]

    VRSHRN.u16  d22,  q11,  #8                          @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VADDL.u8    q2,   d4,   d16                         @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]
    VADD.i8     d19,  d22,  d28
    VST1.8      d19,  [r3]!


    VMULL.u8    q10,  d18,  d31                         @ R * K[0]
    VMLAL.u8    q10,  d17,  d30                         @ + G * K[1]
    VMLAL.u8    q10,  d16,  d29                         @ + B * K[2]
    VPADD.u16   d6,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d2,   d2,   d3                          @ G00+G01+G10+G11

    VRSHRN.u16  d20,  q10,  #8                          @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VPADD.u16   d4,   d4,   d5                          @ B00+B01+B10+B11
    VADD.i8     d19,  d20,  d28
    VST1.8      d19,  [r5]!

    VMULL.s16   q8,   d6,   d1[0]                       @ R * K[6]
    VMULL.s16   q3,   d6,   d0[0]                       @ R * K[3]
    VMLAL.s16   q8,   d2,   d1[1]                       @ G * K[7]
    VMLAL.s16   q3,   d2,   d0[1]                       @ G * K[4]
    VMLAL.s16   q8,   d4,   d1[2]                       @ B * K[8]
    VMLAL.s16   q3,   d4,   d0[2]                       @ B * K[5]
    VRSHRN.s32  d16,  q8,   #10                         @ >>10
    VRSHRN.s32  d6,   q3,   #10                         @ >>10
    VADD.s16    d20,  d16,  d26
    VADD.s16    d21,  d6,   d26
    VZIP.s16    d20,  d21
    VMOVN.u16   d4,   q10

    TST         r3,   #27
    BNE         ARGB8888_NV21_GC_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         ARGB8888_NV21_GC_line_loop_batch_skipPLD
    PLD         [r1,  #32]

ARGB8888_NV21_GC_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.8      {d4}, [r1]!                             @ store VU
    BGE         ARGB8888_NV21_GC_line_loop_batch

ARGB8888_NV21_GC_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_NV21_GC_line_tail              @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_NV21_GC_line_loop_lastpixel
ARGB8888_NV21_GC_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    VLDR        d6,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #8
    ADD         r4,   r4,   #8

    VMULL.u8    q8,   d4,   d27                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u8    q1,   d6,   d27                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u16   d16,  d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u16   d2,   d2,   d3

    VPADD.u16   d16,  d16,  d2                          @ R*K0 + G*K1 + B*K2

    VRSHRN.u16  d5,   q8,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10
    VADDL.u8    q2,   d4,   d6
    VADD.u16    d4,   d4,   d5                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #10                         @ >>10
    VRSHRN.s32  d4 ,  q2,   #10                         @ >>10
    VADD.s16    d7,   d6,   d26                         @ +0x80
    VADD.s16    d6,   d4,   d26                         @ +0x80
    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_NV21_GC_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_NV21_GC_line_tail

ARGB8888_NV21_GC_line_loop_lastpixel:
    VLDR        s8,   [r0]                              @ read 1-nd source line
    VLDR        s10,  [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #4
    ADD         r4,   r4,   #4

    VMULL.u8    q8,   d4,   d27                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u8    q11,  d5,   d27                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u16   d16,  d16,  d16
    VPADD.u16   d22,  d22,  d22
    VPADD.u16   d16,  d16,  d22                         @ [Y10 Y00]

    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VADD.i8     d16,  d16,  d28
    VST1.8      d16[0],     [r3]!                       @ store Y00
    VST1.8      d16[2],     [r5]!                       @ store Y10

    VADDL.u8    q2,   d4,   d5                          @ [A00+A10 R00+R10 G00+G10 B00+B10]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d7,   d6,   d26                         @ +0x80
    VADD.s16    d6,   d4,   d26                         @ +0x80
    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store VU

    B           ARGB8888_NV21_GC_line_tail

ARGB8888_NV21_GC_last_line_head:                        @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_NV21_GC_last_line_loop_batch_end

ARGB8888_NV21_GC_last_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMULL.u8    q8,   d6,   d31                         @ R * K[0]
    VPADDL.u8   d22,  d6                                @ R00+R01
    VPADDL.u8   d21,  d5                                @ G00+G01
    VPADDL.u8   d20,  d4                                @ B00+B01
    VMULL.s16   q1,   d22,  d1[0]                       @ R * K[6]
    VMULL.s16   q11,  d22,  d0[0]                       @ R * K[3]
    VMLAL.u8    q8,   d5,   d30                         @ + G * K[1]
    VMLAL.s16   q1,   d21,  d1[1]                       @ G * K[7]
    VMLAL.s16   q11,  d21,  d0[1]                       @ G * K[4]
    VMLAL.u8    q8,   d4,   d29                         @ + B * K[2]
    VMLAL.s16   q1,   d20,  d1[2]                       @ B * K[8]
    VMLAL.s16   q11,  d20,  d0[2]                       @ B * K[5]

    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VRSHRN.s32  d2,   q1,   #9                          @ >>9
    VRSHRN.s32  d22,  q11,  #9                          @ >>9
    VADD.i8     d16,  d16,  d28

    VADD.s16    d9,   d22,  d26                         @ +0x80
    VADD.s16    d8,   d2,   d26
    VST1.8      d16,  [r3]!

    VZIP.s16    d8,   d9
    VMOVN.s16   d2,   q4                                @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         ARGB8888_NV21_GC_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         ARGB8888_NV21_GC_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
ARGB8888_NV21_GC_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.8      {d2}, [r1]!
    BGE         ARGB8888_NV21_GC_last_line_loop_batch

ARGB8888_NV21_GC_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_NV21_GC_line_tail              @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_NV21_GC_last_line_loop_lastpixel
ARGB8888_NV21_GC_last_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMULL.u8    q8,   d4,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d6,   d16,  d17
    VPADD.u16   d6,   d6,   d6

    VRSHRN.u16  d5,   q3,   #8                          @ [Y1 Y0] >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.16     d5[0],[r3]!

ARGB8888_NV21_GC_last_line_loop_nobatch_getUV:
    VMOVL.u8    q2,   d4
    VADD.u16    d4,   d4,   d5                          @ [A00+A01 R00+R01 G00+G01 B00+B01]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d7,   d6,   d26                         @ +0x80
    VADD.s16    d6,   d4,   d26                         @ +0x80

    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store VU

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_NV21_GC_last_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_NV21_GC_line_tail

ARGB8888_NV21_GC_last_line_loop_lastpixel:
    VLDR        s4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMULL.u8    q8,   d2,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d4,   d16,  d17
    VPADD.u16   d4,   d4,   d4

    VRSHRN.u16  d5,   q2,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.8      d5[0],[r3]!                             @ store Y

    VMOVL.u8    q1,   d2
    VMULL.s16   q3,   d2,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d2,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #8                          @ >>8
    VRSHRN.s32  d4 ,  q2,   #8                          @ >>8
    VADD.s16    d7,   d6,   d26                         @ +0x80
    VADD.s16    d6,   d4,   d26                         @ +0x80
    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store VU
@@
@@ Switch next pair of lines
@@
ARGB8888_NV21_GC_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         ARGB8888_NV21_GC_line_head              @ to the next pair of lines

    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return
@   ENDP        @gpu_csc_ARGBToNV21


    .align 5
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Purpose:    interlived ARGB -> NV12
@
@ void gpu_csc_ARGBToNV12(
@          const Ipp8u* pSrc, int srcSL,
@          Ipp8u* pDst[2], int dstSL[2],
@          int width, int height)
@
@   ARM Registers Assignment:
@       r0          :   pSrc, pSrcLine 1
@       r1          :   UV-plane line
@       r2          :
@       r3          :   Y-plane line 1
@       r4          :   pSrcLine 2
@       r5          :   Y-plane line 2
@       r6          :   width
@       r7          :   height
@       r8          :   srcstride
@       r9          :   Y-plane stride
@       r10         :   UV-plane stride
@       r11         :
@       r12         :   width counter
@       lr          :   UV descaler
@   NEON registers Assignment:
@       d31         :   [K0 K0 K0 K0]
@       d30         :   [K1 K1 K1 K1]
@       d29         :   [K2 K2 K2 K2]
@       d28         :   [K3 K3 K3 K3]
@       d27         :   [K4 K4 K4 K4]
@       d26         :   [0x80 0x80 0x80 0x80]
@       q12         :   temp value
@       q11         :   temp value
@       d21         :   [0 K4 0  K3]
@       d20         :   [0 K1 K2 K3]
@       q9          :   temp value
@       q8          :   temp value
@       q3          :   temp value
@       q2          :   temp value
@       q1          :   temp value
@       q0          :   temp value
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

gpu_csc_ARGBToNV12:
    LDR         r12,  =g_Formula
    STMDB       sp!,  {r4 - r11, lr}                    @ push registers
    LDR         r12,  [r12]
    TST         r12,  #0x10
    BEQ         gpu_csc_ARGBToNV12_GC

    LDR         lr,   =RGB_YUV_Q10                      @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    LDRH        r9,   [lr,  #0*HWORDSIZE]               @ RGB_YUV[0] = K[0]
    LDRH        r10,  [lr,  #1*HWORDSIZE]               @ RGB_YUV[1] = K[1]
    LDRH        r11,  [lr,  #2*HWORDSIZE]               @ RGB_YUV[2] = K[2]
    LDRH        r4,   [lr,  #3*HWORDSIZE]               @ RGB_YUV[3] = K[3]
    LDRH        r5,   [lr,  #4*HWORDSIZE]               @ RGB_YUV[4] = K[4]

    VDUP.u16    d31,  r9                                @ [K0 K0 K0 K0]
    VDUP.u16    d30,  r10                               @ [K1 K1 K1 K1]
    VDUP.u16    d29,  r11                               @ [K2 K2 K2 K2]
    VDUP.u16    d28,  r4                                @ [K3 K3 K3 K3]
    VDUP.u16    d27,  r5                                @ [K4 K4 K4 K4]

    ADD         r11,  r11,  r10,  LSL #16
    VMOV.u16    d26,  #0x80                             @ [0x80 0x80 0x80 0x80]
    VMOV.u32    d20,  r11,  r9                          @ [0 k0 k1 k2]
    VMOV.u32    d21,  r4,   r5                          @ [0 k4  0 k3]

    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ U-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter

    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)
    LDR         r1,   [r2,  #4*1]                       @ r1 U-plane address

    MOV         r8,   r4,   LSL #1                      @ source advancer
    SUB         r8,   r8,   r6,   LSL #2
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r6,   r6,   #1
    MOV         r11,  r6,   ASR #1
    SUB         r10,  r10,  r11,  LSL #1                @ U-plane advancer
    SUB         r6,   r6,   #1

    ADD         r4,   r0,   r4                          @ r0,r4 are points source line 1 and line 2
    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
gpu_csc_ARGBToNV12_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]

    CMP         r7,   #1                                @ test target height
    BEQ         gpu_csc_ARGBToNV12_last_line_head @ last line

    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_ARGBToNV12_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
gpu_csc_ARGBToNV12_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    VLD4.8      {d4 - d7},  [r4]!                       @ read 2-nd source line:
    PLD         [r0,  #32]
    PLD         [r4,  #32]

    VMOVL.u8    q12,  d2                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_32 >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8


    VMOVL.u8    q11,  d6                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VST1.8      d3,   [r3]!
    VMULL.u16   q9,   d22,  d31                         @ R * K[0]
    VMULL.u16   q12,  d23,  d31
    VMOVL.u8    q11,  d5                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMLAL.u16   q9,   d22,  d30                         @ + G * K[1]
    VMLAL.u16   q12,  d23,  d30
    VMOVL.u8    q11,  d4                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q9,   d22,  d29                         @ + B * K[2]
    VMLAL.u16   q12,  d23,  d29

    VSHRN.u32   d18,  q9,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_32 >> QBITS
    VSHRN.u32   d19,  q12,  #10
    VMOVN.u16   d7,   q9                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8

    VADD.u16    q8,   q9,   q8                          @ [Y07+Y17 Y06+Y16 Y05+Y15 Y04+Y14 Y03+Y13 Y02+Y12 Y01+Y11 Y00+Y10]
    VST1.8      d7,   [r5]!
    VADDL.u8    q3,   d2,   d6                          @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q2,   d0,   d4                          @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]

    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01 + Y10 + Y11
    VPADD.u16   d2,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d0,   d4,   d5                          @ B00+B01+B10+B11

    VSUB.s16    d2,   d2,   d16                         @ R - Y
    VSUB.s16    d0,   d0,   d16                         @ B - Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #12                         @ >>12
    VSHRN.s32   d18,  q0,   #12                         @ >>12

    VADD.s16    d0,   d19,  d26                         @ +0x80
    VADD.s16    d1,   d18,  d26

    VZIP.s16    d1,   d0

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         gpu_csc_ARGBToNV12_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         gpu_csc_ARGBToNV12_line_loop_batch_skipPLD
    PLD         [r1,  #32]

gpu_csc_ARGBToNV12_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VREV64.32   d0,   d0
    VST1.8      {d0}, [r1]!                             @ store UV

    BGE         gpu_csc_ARGBToNV12_line_loop_batch

gpu_csc_ARGBToNV12_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_ARGBToNV12_line_tail            @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_ARGBToNV12_line_loop_lastpixel
gpu_csc_ARGBToNV12_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    VLDR        d2,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #8
    ADD         r4,   r4,   #8

    VMOVL.u8    q0,   d0                                @ [A01 R01 G01 B01 A00 R00 G00 B00]
    VMOVL.u8    q1,   d2                                @ [A11 R11 G11 B11 A10 R10 G10 B10]

    VMULL.u16   q8,   d0,   d20                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u16   q9,   d1,   d20                         @ [0 R01*K0 G01*K1 B01*K2]
    VMULL.u16   q11,  d2,   d20                         @ [0 R10*K0 G10*K1 B10*K2]
    VMULL.u16   q12,  d3,   d20                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u32   d4,   d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d7,   d24,  d25

    VPADD.u32   d4,   d4,   d5                          @ R*K0 + G*K1 + B*K2
    VPADD.u32   d5,   d6,   d7

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10

gpu_csc_ARGBToNV12_line_loop_nobatch_getUV:
    VADD.u16    q0,   q0,   q1
    VADD.u16    d0,   d0,   d1                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VPADDL.u16  d4,   d4
    VPADDL.u32  d4,   d4                                @ Y00+Y01+Y10+Y11
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #12                         @ >>12
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r1]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_ARGBToNV12_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         gpu_csc_ARGBToNV12_line_tail

gpu_csc_ARGBToNV12_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    VLDR        s4,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #4
    ADD         r4,   r4,   #4

    VMOVL.u8    q0,   d0
    VMOVL.u8    q1,   d2

    VMULL.u16   q8,   d0,   d20                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u16   q11,  d2,   d20                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d4,   d4,   d6                          @ [Y10 Y00]

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y00
    VST1.8      d5[1],[r5]!                             @ store Y10

    VADD.u16    d0,   d0,   d2                          @ [A00+A10 R00+R10 G00+G10 B00+B10]
    VPADDL.u16  d4,   d4                                @ Y00+Y10
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r1]!                             @ store V

    B           gpu_csc_ARGBToNV12_line_tail

gpu_csc_ARGBToNV12_last_line_head:                      @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_ARGBToNV12_last_line_loop_batch_end

gpu_csc_ARGBToNV12_last_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMOVL.u8    q12,  d2                                @ R
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ G
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ B
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ Y >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8
    VST1.8      d3,   [r3]!

gpu_csc_ARGBToNV12_last_line_loop_batch_getUV:
    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01
    VPADDL.u8   d2,   d2                                @ R00+R01
    VPADDL.u8   d0,   d0                                @ B00+B01

    VSUB.s16    d2,   d2,   d16                         @ R -Y
    VSUB.s16    d0,   d0,   d16                         @ B -Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #11                         @ >>11
    VSHRN.s32   d18,  q0,   #11                         @ >>11

    VADD.s16    d0,   d19,  d26                         @ +0x80
    VADD.s16    d1,   d18,  d26

    VZIP.s16    d1,   d0

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff
    TST         r3,   #27
    BNE         gpu_csc_ARGBToNV12_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         gpu_csc_ARGBToNV12_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
gpu_csc_ARGBToNV12_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VREV64.32   d0,   d0
    VST1.8      {d0}, [r1]!                             @ store dst

    BGE         gpu_csc_ARGBToNV12_last_line_loop_batch

gpu_csc_ARGBToNV12_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_ARGBToNV12_line_tail            @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_ARGBToNV12_last_line_loop_lastpixel
gpu_csc_ARGBToNV12_last_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMOVL.u8    q0,   d0

    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2
    VMULL.u16   q9,   d1,   d20

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d4,   d4,   d5

    VSHRN.u32   d4,   q2,   #10                         @ [Y1 Y0] >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!

gpu_csc_ARGBToNV12_last_line_loop_nobatch_getUV:
    VADD.u16    d0,   d0,   d1                          @ [A00+A01 R00+R01 G00+G01 B00+B01]
    VPADDL.u16  d4,   d4                                @ Y00+Y01
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r1]!                             @ store V

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_ARGBToNV12_last_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         gpu_csc_ARGBToNV12_line_tail

gpu_csc_ARGBToNV12_last_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMOVL.u8    q0,   d0
    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d4,   d4,   d4

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y

    VDUP.u16    d4,   d4[0]
    VSUB.s16    d0,   d0,   d4
    VMULL.s16   q2,   d0,   d21
    VSHRN.s32   d4 ,  q2,   #10                         @ >>10
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff

    VST1.8      d4[0],[r1]!                             @ store U
    VST1.8      d4[2],[r1]!                             @ store V
@@
@@ Switch next pair of lines
@@
gpu_csc_ARGBToNV12_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         gpu_csc_ARGBToNV12_line_head            @ to the next pair of lines
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return

.ltorg
gpu_csc_ARGBToNV12_GC_table_1:
    .word       0xff000204, 0xff000204
gpu_csc_ARGBToNV12_GC_table_2:
    .word       0x03020504, 0xffff0100

    .align 5
gpu_csc_ARGBToNV12_GC:
    LDR         lr,   =RGB_YUV_GC                       @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    VLDR        d2,   [lr]                              @ [16 K2 K1 K0]
    VLDR        d0,   [lr,  #4*HWORDSIZE]
    VLDR        d1,   [lr,  #8*HWORDSIZE]
    VLDR        d3,   gpu_csc_ARGBToNV12_GC_table_1
    VLDR        d4,   gpu_csc_ARGBToNV12_GC_table_2
    VMOV.i32    d27,  #0
    VMOV.i32    d24,  #0
    VDUP.8      d31,  d2[0]                             @ [K0 K0 K0 K0 K0 K0 K0 K0]
    VDUP.8      d30,  d2[2]                             @ [K1 K1 K1 K1 K1 K1 K1 K1]
    VDUP.8      d29,  d2[4]                             @ [K2 K2 K2 K2 K2 K2 K2 K2]
    VDUP.8      d28,  d2[6]                             @ [16 16 16 16 16 16 16 16]
    VTBL.8      d27,  {d2}, d3                          @ [0  K0 K1 K2 0  K0 K1 K2]
    VDUP.16     d26,  d0[3]                             @ [128 128 128 128]
    VTBL.8      d24,  {d0}, d4                          @ [0  K3 K4 K5]
    VTBL.8      d25,  {d1}, d4                          @ [0  K6 K7 K8]
    ADD         r11,  r11,  r10,  LSL #16
    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ UV-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter

    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)
    LDR         r1,   [r2,  #4*1]                       @ r1 UV-plane address

    MOV         r8,   r4,   LSL #1                      @ source advancer
    SUB         r8,   r8,   r6,   LSL #2
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r6,   r6,   #1
    MOV         r11,  r6,   ASR #1
    SUB         r10,  r10,  r11,  LSL #1                @ UV-plane advancer
    SUB         r6,   r6,   #1

    ADD         r4,   r0,   r4                          @ r0,r4 are points source line 1 and line 2
    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
ARGB8888_NV12_GC_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]

    CMP         r7,   #1                                @ test target height
    BEQ         ARGB8888_NV12_GC_last_line_head @ last line

    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_NV12_GC_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
ARGB8888_NV12_GC_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    VLD4.8      {d16- d19}, [r4]!                       @ read 2-nd source line:
    PLD         [r0,  #32]
    PLD         [r4,  #32]

    VMULL.u8    q11,  d6,   d31                         @ R * K[0]
    VMLAL.u8    q11,  d5,   d30                         @ + G * K[1]
    VMLAL.u8    q11,  d4,   d29                         @ + B * K[2]

    VADDL.u8    q3,   d6,   d18                         @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q1,   d5,   d17                         @ [G07+G17 G06+G16 G05+G15 G04+G14 G03+G13 G02+G12 G01+G11 G00+G10]

    VRSHRN.u16  d22,  q11,  #8                          @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VADDL.u8    q2,   d4,   d16                         @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]
    VADD.i8     d19,  d22,  d28
    VST1.8      d19,  [r3]!


    VMULL.u8    q10,  d18,  d31                         @ R * K[0]
    VMLAL.u8    q10,  d17,  d30                         @ + G * K[1]
    VMLAL.u8    q10,  d16,  d29                         @ + B * K[2]
    VPADD.u16   d6,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d2,   d2,   d3                          @ G00+G01+G10+G11

    VRSHRN.u16  d20,  q10,  #8                          @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VPADD.u16   d4,   d4,   d5                          @ B00+B01+B10+B11
    VADD.i8     d19,  d20,  d28
    VST1.8      d19,  [r5]!

    VMULL.s16   q8,   d6,   d1[0]                       @ R * K[6]
    VMULL.s16   q3,   d6,   d0[0]                       @ R * K[3]
    VMLAL.s16   q8,   d2,   d1[1]                       @ G * K[7]
    VMLAL.s16   q3,   d2,   d0[1]                       @ G * K[4]
    VMLAL.s16   q8,   d4,   d1[2]                       @ B * K[8]
    VMLAL.s16   q3,   d4,   d0[2]                       @ B * K[5]
    VRSHRN.s32  d16,  q8,   #10                         @ >>10
    VRSHRN.s32  d6,   q3,   #10                         @ >>10
    VADD.s16    d21,  d16,  d26
    VADD.s16    d20,  d6,   d26
    VZIP.s16    d20,  d21
    VMOVN.u16   d4,   q10

    TST         r3,   #27
    BNE         ARGB8888_NV12_GC_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         ARGB8888_NV12_GC_line_loop_batch_skipPLD
    PLD         [r1,  #32]

ARGB8888_NV12_GC_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.8      {d4}, [r1]!                             @ store UV
    BGE         ARGB8888_NV12_GC_line_loop_batch

ARGB8888_NV12_GC_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_NV12_GC_line_tail              @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_NV12_GC_line_loop_lastpixel
ARGB8888_NV12_GC_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    VLDR        d6,   [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #8
    ADD         r4,   r4,   #8

    VMULL.u8    q8,   d4,   d27                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u8    q1,   d6,   d27                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u16   d16,  d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u16   d2,   d2,   d3

    VPADD.u16   d16,  d16,  d2                          @ R*K0 + G*K1 + B*K2

    VRSHRN.u16  d5,   q8,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10
    VADDL.u8    q2,   d4,   d6
    VADD.u16    d4,   d4,   d5                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #10                         @ >>10
    VRSHRN.s32  d4 ,  q2,   #10                         @ >>10
    VADD.s16    d6,   d6,   d26                         @ +0x80
    VADD.s16    d7,   d4,   d26                         @ +0x80
    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store UV

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_NV12_GC_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_NV12_GC_line_tail

ARGB8888_NV12_GC_line_loop_lastpixel:
    VLDR        s8,   [r0]                              @ read 1-nd source line
    VLDR        s10,  [r4]                              @ read 2-nd source line
    ADD         r0,   r0,   #4
    ADD         r4,   r4,   #4

    VMULL.u8    q8,   d4,   d27                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u8    q11,  d5,   d27                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u16   d16,  d16,  d16
    VPADD.u16   d22,  d22,  d22
    VPADD.u16   d16,  d16,  d22                         @ [Y10 Y00]

    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VADD.i8     d16,  d16,  d28
    VST1.8      d16[0],     [r3]!                       @ store Y00
    VST1.8      d16[2],     [r5]!                       @ store Y10

    VADDL.u8    q2,   d4,   d5                          @ [A00+A10 R00+R10 G00+G10 B00+B10]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d6,   d6,   d26                         @ +0x80
    VADD.s16    d7,   d4,   d26                         @ +0x80
    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store UV

    B           ARGB8888_NV12_GC_line_tail

ARGB8888_NV12_GC_last_line_head:                        @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_NV12_GC_last_line_loop_batch_end

ARGB8888_NV12_GC_last_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMULL.u8    q8,   d6,   d31                         @ R * K[0]
    VPADDL.u8   d22,  d6                                @ R00+R01
    VPADDL.u8   d21,  d5                                @ G00+G01
    VPADDL.u8   d20,  d4                                @ B00+B01
    VMULL.s16   q1,   d22,  d1[0]                       @ R * K[6]
    VMULL.s16   q11,  d22,  d0[0]                       @ R * K[3]
    VMLAL.u8    q8,   d5,   d30                         @ + G * K[1]
    VMLAL.s16   q1,   d21,  d1[1]                       @ G * K[7]
    VMLAL.s16   q11,  d21,  d0[1]                       @ G * K[4]
    VMLAL.u8    q8,   d4,   d29                         @ + B * K[2]
    VMLAL.s16   q1,   d20,  d1[2]                       @ B * K[8]
    VMLAL.s16   q11,  d20,  d0[2]                       @ B * K[5]

    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VRSHRN.s32  d2,   q1,   #9                          @ >>9
    VRSHRN.s32  d22,  q11,  #9                          @ >>9
    VADD.i8     d16,  d16,  d28

    VADD.s16    d8,   d22,  d26                         @ +0x80
    VADD.s16    d9,   d2,   d26
    VST1.8      d16,  [r3]!

    VZIP.s16    d8,   d9
    VMOVN.s16   d2,   q4                                @ pack, clamp with 0 and 0xff

    TST         r3,   #27
    BNE         ARGB8888_NV12_GC_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         ARGB8888_NV12_GC_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
ARGB8888_NV12_GC_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.8      {d2}, [r1]!
    BGE         ARGB8888_NV12_GC_last_line_loop_batch

ARGB8888_NV12_GC_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_NV12_GC_line_tail              @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_NV12_GC_last_line_loop_lastpixel
ARGB8888_NV12_GC_last_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMULL.u8    q8,   d4,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d6,   d16,  d17
    VPADD.u16   d6,   d6,   d6

    VRSHRN.u16  d5,   q3,   #8                          @ [Y1 Y0] >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.16     d5[0],[r3]!

ARGB8888_NV12_GC_last_line_loop_nobatch_getUV:
    VMOVL.u8    q2,   d4
    VADD.u16    d4,   d4,   d5                          @ [A00+A01 R00+R01 G00+G01 B00+B01]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d6,   d6,   d26                         @ +0x80
    VADD.s16    d7,   d4,   d26                         @ +0x80

    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store UV

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_NV12_GC_last_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_NV12_GC_line_tail

ARGB8888_NV12_GC_last_line_loop_lastpixel:
    VLDR        s4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMULL.u8    q8,   d2,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d4,   d16,  d17
    VPADD.u16   d4,   d4,   d4

    VRSHRN.u16  d5,   q2,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    VST1.8      d5[0],[r3]!                             @ store Y

    VMOVL.u8    q1,   d2
    VMULL.s16   q3,   d2,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VMULL.s16   q2,   d2,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d6,   d6,   d7
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d6,   d6,   d6
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d6 ,  q3,   #8                          @ >>8
    VRSHRN.s32  d4 ,  q2,   #8                          @ >>8
    VADD.s16    d6,   d6,   d26                         @ +0x80
    VADD.s16    d7,   d4,   d26                         @ +0x80
    VZIP.s8     d6,   d7
    VST1.16     d6[0],[r1]!                             @ store UV
@@
@@ Switch next pair of lines
@@
ARGB8888_NV12_GC_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         ARGB8888_NV12_GC_line_head              @ to the next pair of lines

    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return
@   ENDP        @gpu_csc_ARGBToNV12


    .align 5
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Purpose:    interlived ARGB -> UYVY
@
@ void gpu_csc_ARGBToUYVY(
@          const Ipp8u* pSrc, int srcSL,
@          Ipp8u* pDst, int dstSL,
@          int width, int height)
@
@   ARM Registers Assignment:
@       r0          :   pSrc, pSrcLine 1
@       r1          :
@       r2          :
@       r3          :   Y-plane line 1
@       r4          :
@       r5          :
@       r6          :   width
@       r7          :   height
@       r8          :   srcstride
@       r9          :   Y-plane stride
@       r10         :
@       r11         :
@       r12         :   width counter
@       lr          :   UV descaler
@   NEON registers Assignment:
@       d31         :   [K0 K0 K0 K0]
@       d30         :   [K1 K1 K1 K1]
@       d29         :   [K2 K2 K2 K2]
@       d28         :   [K3 K3 K3 K3]
@       d27         :   [K4 K4 K4 K4]
@       d26         :   [0x80 0x80 0x80 0x80]
@       q12         :   temp value
@       q11         :   temp value
@       d21         :   [0 K4 0  K3]
@       d20         :   [0 K1 K2 K3]
@       q9          :   temp value
@       q8          :   temp value
@       q3          :   temp value
@       q2          :   temp value
@       q1          :   temp value
@       q0          :   temp value
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

gpu_csc_ARGBToUYVY:
    LDR         r12,  =g_Formula
    STMDB       sp!,  {r4 - r11, lr}                    @ push registers
    LDR         r12,  [r12]
    TST         r12,  #0x10
    BEQ         gpu_csc_ARGBToUYVY_GC

    LDR         lr,   =RGB_YUV_Q10                      @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    LDRH        r9,   [lr,  #0*HWORDSIZE]               @ RGB_YUV[0] = K[0]
    LDRH        r10,  [lr,  #1*HWORDSIZE]               @ RGB_YUV[1] = K[1]
    LDRH        r11,  [lr,  #2*HWORDSIZE]               @ RGB_YUV[2] = K[2]
    LDRH        r4,   [lr,  #3*HWORDSIZE]               @ RGB_YUV[3] = K[3]
    LDRH        r5,   [lr,  #4*HWORDSIZE]               @ RGB_YUV[4] = K[4]

    VDUP.u16    d31,  r9                                @ [K0 K0 K0 K0]
    VDUP.u16    d30,  r10                               @ [K1 K1 K1 K1]
    VDUP.u16    d29,  r11                               @ [K2 K2 K2 K2]
    VDUP.u16    d28,  r4                                @ [K3 K3 K3 K3]
    VDUP.u16    d27,  r5                                @ [K4 K4 K4 K4]

    ADD         r11,  r11,  r10,  LSL #16
    VMOV.u16    d26,  #0x80                             @ [0x80 0x80 0x80 0x80]
    VMOV.u32    d20,  r11,  r9                          @ [0 k0 k1 k2]
    VMOV.u32    d21,  r4,   r5                          @ [0 k4  0 k3]

    MOV         r9,   r3                                @ Y-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r12,  r6                                @ width counter

    MOV         r3,   r2                                @ r3 Y-plane address (line 1)

    SUB         r8,   r4,   r6,   LSL #2                @ source advancer

@@
@@ Convert pair of lines
@@
gpu_csc_ARGBToUYVY_line_head:
    PLD         [r0]
    PLD         [r3]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_ARGBToUYVY_line_loop_batch_end

gpu_csc_ARGBToUYVY_line_loop_batch:
    VLD4.8      {d0 - d3},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VMOVL.u8    q12,  d2                                @ R
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ G
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ B
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ Y >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d4,   q8

gpu_csc_ARGBToUYVY_line_loop_batch_getUV:
    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01
    VPADDL.u8   d2,   d2                                @ R00+R01
    VPADDL.u8   d0,   d0                                @ B00+B01

    VSUB.s16    d2,   d2,   d16                         @ R -Y
    VSUB.s16    d0,   d0,   d16                         @ B -Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #11                         @ >>11
    VSHRN.s32   d18,  q0,   #11                         @ >>11

    VADD.s16    d1,   d19,  d26                         @ +0x80
    VADD.s16    d0,   d18,  d26

    VZIP.s16    d0,   d1

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff

    VZIP.s8     d0,   d4
    TST         r3,   #27
    BNE         gpu_csc_ARGBToUYVY_line_loop_batch_skipPLD
    PLD         [r3,  #32]
gpu_csc_ARGBToUYVY_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length

    VST1.8      d0,   [r3]!
    VST1.8      d4,   [r3]!                             @ store dst

    BGE         gpu_csc_ARGBToUYVY_line_loop_batch

gpu_csc_ARGBToUYVY_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_ARGBToUYVY_line_tail      @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_ARGBToUYVY_line_loop_lastpixel
gpu_csc_ARGBToUYVY_line_loop_nobatch:

    VLDR        d0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMOVL.u8    q0,   d0

    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2
    VMULL.u16   q9,   d1,   d20

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d4,   d4,   d5

    VSHRN.u32   d4,   q2,   #10                         @ [Y1 Y0] >> QBITS
    VMOVN.u16   d5,   q2

    ADD         lr,   r3,   #1
    VST1.8      d5[0],[lr]
    ADD         lr,   r3,   #3
    VST1.8      d5[1],[lr]
gpu_csc_ARGBToUYVY_line_loop_nobatch_getUV:
    VADD.u16    d0,   d0,   d1                          @ [A00+A01 R00+R01 G00+G01 B00+B01]
    VPADDL.u16  d4,   d4                                @ Y00+Y01
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]

    ADD         lr,   r3,   #2
    VST1.8      d4[0],[r3]                              @ store U
    VST1.8      d4[2],[lr]                              @ store V

    ADD         r3,   r3,   #4

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_ARGBToUYVY_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         gpu_csc_ARGBToUYVY_line_tail

gpu_csc_ARGBToUYVY_line_loop_lastpixel:
    VLDR        s0,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMOVL.u8    q0,   d0
    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d4,   d4,   d4

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    ADD         lr,   r3,   #1
    VST1.8      d5[0],[lr]                              @ store Y

    VDUP.u16    d4,   d4[0]
    VSUB.s16    d0,   d0,   d4
    VMULL.s16   q2,   d0,   d21
    VSHRN.s32   d4 ,  q2,   #10                         @ >>10
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff

    VST1.8      d4[0],[r3]                              @ store U

@@
@@ Switch next pair of lines
@@
gpu_csc_ARGBToUYVY_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r2,   r2,   r9
    MOV         r3,   r2
    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #1                          @ decrease target height
    BGT         gpu_csc_ARGBToUYVY_line_head            @ to the next pair of lines
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return

.ltorg
gpu_csc_ARGBToUYVY_GC_table_1:
    .word       0xff000204, 0xff000204
gpu_csc_ARGBToUYVY_GC_table_2:
    .word       0x03020504, 0xffff0100

    .align 5
gpu_csc_ARGBToUYVY_GC:
    LDR         lr,   =RGB_YUV_GC                       @ lr = k[]
    LDR         r6,   [sp,  #36]                        @ roi width
    LDR         r7,   [sp,  #40]                        @ roi height
                                                        @ prepare some constants:
    VLDR        d2,   [lr]                              @ [16 K2 K1 K0]
    VLDR        d0,   [lr, #4*HWORDSIZE]
    VLDR        d1,   [lr, #8*HWORDSIZE]
    VLDR        d3,   gpu_csc_ARGBToUYVY_GC_table_1
    VLDR        d4,   gpu_csc_ARGBToUYVY_GC_table_2
    VMOV.i32    d27,  #0
    VMOV.i32    d24,  #0
    VDUP.8      d31,  d2[0]                             @ [K0 K0 K0 K0 K0 K0 K0 K0]
    VDUP.8      d30,  d2[2]                             @ [K1 K1 K1 K1 K1 K1 K1 K1]
    VDUP.8      d29,  d2[4]                             @ [K2 K2 K2 K2 K2 K2 K2 K2]
    VDUP.8      d28,  d2[6]                             @ [16 16 16 16 16 16 16 16]
    VTBL.8      d27,  {d2},  d3                         @ [0  K0 K1 K2 0  K0 K1 K2]
    VDUP.16     d26,  d0[3]                             @ [128 128 128 128]
    VTBL.8      d24,  {d0},  d4                         @ [0  K3 K4 K5]
    VTBL.8      d25,  {d1},  d4                         @ [0  K6 K7 K8]
    ADD         r11,  r11,  r10,  LSL #16
    MOV         r9,   r3                                @ Y-plane scanline size

    MOV         r4,   r1                                @ source  scanline size
    MOV         r12,  r6                                @ width counter
    MOV         r3,   r2                                @ r3 Y-plane address (line 1)

    SUB         r8,   r4,   r6,   LSL #2                @ source advancer
@@
@@ Convert pair of lines
@@
ARGB8888_UYVY_GC_line_head:
    PLD         [r0]
    PLD         [r3]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         ARGB8888_UYVY_GC_line_loop_batch_end

ARGB8888_UYVY_GC_line_loop_batch:
    VLD4.8      {d4 - d7},  [r0]!                       @ read 1-nd source line:
    PLD         [r0,  #32]

    VPADDL.u8   d2,   d6                                @ R00+R01
    VMULL.u8    q8,   d6,   d31                         @ R * K[0]
    VMULL.s16   q9,   d2,   d0[0]                       @ R * K[3]
    VMULL.s16   q10,  d2,   d1[0]                       @ R * K[6]
    VPADDL.u8   d3,   d5                                @ G00+G01
    VMLAL.u8    q8,   d5,   d30                         @ + G * K[1]
    VMLAL.s16   q9,   d3,   d0[1]                       @ G * K[4]
    VMLAL.s16   q10,  d3,   d1[1]                       @ G * K[7]
    VMLAL.u8    q8,   d4,   d29                         @ + B * K[2]
    VPADDL.u8   d2,   d4                                @ B00+B01
    VRSHRN.u16  d16,  q8,   #8                          @ Y >> QBITS
    VMLAL.s16   q9,   d2,   d0[2]                       @ B * K[5]
    VMLAL.s16   q10,  d2,   d1[2]                       @ B * K[8]
    VADD.i8     d7,   d16,  d28

ARGB8888_UYVY_GC_line_loop_batch_getUV:

    VRSHRN.s32  d18,  q9,   #9                          @ >>9
    VRSHRN.s32  d20,  q10,  #9                          @ >>9
    VADD.s16    d8,   d18,  d26                         @ +0x80
    VADD.s16    d9,   d20,  d26

    VZIP.s16    d8,   d9
    VMOVN.s16   d2,   q4                                @ pack, clamp with 0 and 0xff
    VZIP.s8     d2,   d7
    TST         r3,   #27
    BNE         ARGB8888_UYVY_GC_line_loop_batch_skipPLD
    PLD         [r3,  #32]
ARGB8888_UYVY_GC_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VST1.8      d2,   [r3]!                             @ store dst
    VST1.8      d7,   [r3]!

    BGE         ARGB8888_UYVY_GC_line_loop_batch

ARGB8888_UYVY_GC_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         ARGB8888_UYVY_GC_line_tail              @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         ARGB8888_UYVY_GC_line_loop_lastpixel
ARGB8888_UYVY_GC_line_loop_nobatch:

    VLDR        d4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #8

    VMULL.u8    q8,   d4,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d6,   d16,  d17
    VPADD.u16   d6,   d6,   d6

    VRSHRN.u16  d5,   q3,   #8                          @ [Y1 Y0] >> QBITS
    VADD.i8     d5,   d5,   d28

    ADD         lr,   r3,   #1
    VST1.8      d5[0],[lr]
    ADD         lr,   r3,   #3
    VST1.8      d5[1],[lr]

ARGB8888_UYVY_GC_line_loop_nobatch_getUV:
    VMOVL.u8    q2,   d4
    VADD.u16    d4,   d4,   d5                          @ [A00+A01 R00+R01 G00+G01 B00+B01]

    VMULL.s16   q3,   d4,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VADD.i32    d6,   d6,   d7
    VPADD.i32   d6,   d6,   d6
    VRSHRN.s32  d6 ,  q3,   #9                          @ >>9
    VADD.s16    d6,   d6,   d26                         @ +0x80

    VMULL.s16   q2,   d4,   d25                         @ [A R G B] * [0 K6 K7 K8]
    VADD.i32    d4,   d4,   d5
    VPADD.i32   d4,   d4,   d4
    VRSHRN.s32  d4 ,  q2,   #9                          @ >>9
    VADD.s16    d7,   d4,   d26                         @ +0x80

    VMOVN.u16   d4,   q3
    ADD         lr,   r3,   #2
    VST1.8      d4[0],[r3]                              @ store U
    VST1.8      d4[4],[lr]                              @ store V

    ADD         r3,   r3,   #4
    SUBS        r12,  r12,  #2                          @ test target length
    BGE         ARGB8888_UYVY_GC_line_loop_nobatch
    ADDS        r12,  r12,  #2                          @ test target length
    BLE         ARGB8888_UYVY_GC_line_tail

ARGB8888_UYVY_GC_line_loop_lastpixel:
    VLDR        s4,   [r0]                              @ read 1-nd source line
    ADD         r0,   r0,   #4

    VMULL.u8    q8,   d2,   d27                         @ 0 R*K0 G*K1 B*K2

    VPADD.u16   d4,   d16,  d17
    VPADD.u16   d4,   d4,   d4

    VRSHRN.u16  d5,   q2,   #8                          @ Y >> QBITS
    VADD.i8     d5,   d5,   d28

    ADD         lr,   r3,   #1
    VST1.8      d5[0],[lr]                              @ store Y

    VMOVL.u8    q1,   d2
    VMULL.s16   q3,   d2,   d24                         @ [A R G B] * [0 K3 K4 K5]
    VADD.i32    d6,   d6,   d7
    VPADD.i32   d6,   d6,   d6
    VRSHRN.s32  d6 ,  q3,   #8                          @ >>8
    VADD.s16    d6,   d6,   d26                         @ +0x80

    VMOVN.u16   d4,   q3
    VST1.8      d4[0],[r3]                              @ store UV

@@
@@ Switch next pair of lines
@@
ARGB8888_UYVY_GC_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r2,   r2,   r9
    MOV         r3,   r2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #1                          @ decrease target height
    BGT         ARGB8888_UYVY_GC_line_head              @ to the next pair of linesgpu_csc_ARGBToUYVY_end:
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return
@   ENDP        @gpu_csc_ARGBToUYVY


    .align 5
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Purpose:    RGB565 -> NV12
@
@ void gpu_csc_RGBToNV12(
@          const Ipp16u* pSrc, int srcSL,
@          Ipp8u* pDst[2], int dstSL[2],
@          int width, int height)
@
@   ARM Registers Assignment:
@       r0          :   pSrc, pSrcLine 1
@       r1          :   UV-plane line
@       r2          :
@       r3          :   Y-plane line 1
@       r4          :   pSrcLine 2
@       r5          :   Y-plane line 2
@       r6          :   width
@       r7          :   height
@       r8          :   srcstride
@       r9          :   Y-plane stride
@       r10         :   UV-plane stride
@       r11         :
@       r12         :   width counter
@       lr          :   UV descaler
@   NEON registers Assignment:
@       d31         :   [K0 K0 K0 K0]
@       d30         :   [K1 K1 K1 K1]
@       d29         :   [K2 K2 K2 K2]
@       d28         :   [K3 K3 K3 K3]
@       d27         :   [K4 K4 K4 K4]
@       d26         :   [0x80 0x80 0x80 0x80]
@       q12         :   temp value
@       q11         :   temp value
@       d21         :   [0 K4 0  K3]
@       d20         :   [0 K1 K2 K3]
@       q9          :   temp value
@       q8          :   temp value
@       q3          :   temp value
@       q2          :   temp value
@       q1          :   temp value
@       q0          :   temp value
@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

gpu_csc_RGBToNV12:
    STMDB       sp!,  {r4 - r11, lr}                    @ push registers
    VSTMDB      sp!,  {d8 - d11}
    LDR         lr,   =RGB_YUV_Q10                      @ lr = k[]
    LDR         r6,   [sp,  #68]                        @ roi width
    LDR         r7,   [sp,  #72]                        @ roi height
                                                        @ prepare some constants:
    LDRH        r9,   [lr,  #0*HWORDSIZE]               @ RGB_YUV[0] = K[0]
    LDRH        r10,  [lr,  #1*HWORDSIZE]               @ RGB_YUV[1] = K[1]
    LDRH        r11,  [lr,  #2*HWORDSIZE]               @ RGB_YUV[2] = K[2]
    LDRH        r4,   [lr,  #3*HWORDSIZE]               @ RGB_YUV[3] = K[3]
    LDRH        r5,   [lr,  #4*HWORDSIZE]               @ RGB_YUV[4] = K[4]

    VDUP.u16    d31,  r9                                @ [K0 K0 K0 K0]
    VDUP.u16    d30,  r10                               @ [K1 K1 K1 K1]
    VDUP.u16    d29,  r11                               @ [K2 K2 K2 K2]
    VDUP.u16    d28,  r4                                @ [K3 K3 K3 K3]
    VDUP.u16    d27,  r5                                @ [K4 K4 K4 K4]

    ADD         r11,  r11,  r10,  LSL #16
    VMOV.u16    d26,  #0x80                             @ [0x80 0x80 0x80 0x80]
    VMOV.u32    d20,  r11,  r9                          @ [0 k0 k1 k2]
    VMOV.u32    d21,  r4,   r5                          @ [0 k4  0 k3]

    LDR         r9,   [r3,  #4*0]                       @ Y-plane scanline size
    LDR         r10,  [r3,  #4*1]                       @ U-plane scanline size

    MOV         r5,   r9                                @ Y-plane scanline size
    MOV         r12,  r6                                @ width counter
    LDR         r3,   [r2,  #4*0]                       @ r3 Y-plane address (line 1)

    MOV         r8,   r1                                @ source advancer
    RSB         r9,   r6,   r9,   LSL #1                @ Y-plane advancer
    ADD         r4,   r0,   r1                          @ r0,r4 are points source line 1 and line 2
    ADD         r6,   r6,   #1
    LDR         r1,   [r2,  #4*1]                       @ r1 U-plane address
    MOV         r11,  r6,   ASR #1
    SUB         r10,  r10,  r11,  LSL #1                @ U-plane advancer
    SUB         r6,   r6,   #1

    ADD         r5,   r3,   r5                          @ r5 Y-plane address (line 2)
@@
@@ Convert pair of lines
@@
gpu_csc_RGBToNV12_line_head:
    PLD         [r0]
    PLD         [r3]
    PLD         [r1]

    CMP         r7,   #1                                @ test target height
    BEQ         gpu_csc_RGBToNV12_last_line_head @ last line
    PLD         [r4]
    PLD         [r5]

    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_RGBToNV12_line_loop_batch_end
@@
@@ Don`t care about alignment at all
@@
gpu_csc_RGBToNV12_line_loop_batch:
    VLD1.8      {d0,  d1},  [r0]!                       @ read 1-nd source line:
    VLD1.8      {d10, d11}, [r4]!                       @ read 2-nd source line:
    PLD         [r0,  #16]
    PLD         [r4,  #16]
    VSHRN.i16   d2,   q0,   #8                          @ shift red elements right and narrow
    VSHRN.i16   d3,   q0,   #3                          @ shift green elements right and narrow
    VSHRN.i16   d6,   q5,   #8                          @ shift red elements right and narrow
    VSHRN.i16   d7,   q5,   #3                          @ shift green elements right and narrow
    VSRI.u8     d2,   d2,   #5                          @ d2: red
    VSRI.u8     d6,   d6,   #5                          @ d6: red
    VSHL.i16    q0,   q0,   #3                          @ shift blue elements right and narrow
    VSHL.i16    q5,   q5,   #3                          @ shift blue elements right and narrow
    VMOVL.u8    q12,  d2                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VSRI.u8     d3,   d3,   #6
    VSRI.u8     d7,   d7,   #6
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q4,   d3                                @ [G7 G6 G5 G4 G3 G2 G1 G0]
    VMOVN.i16   d0,   q0
    VMLAL.u16   q8,   d8,   d30                         @ + G * K[1]
    VMLAL.u16   q11,  d9,   d30
    VSRI.u8     d0,   d0,   #5                          @ d0: blue
    VMOV        d1,   d3                                @ d1: green
    VMOVL.u8    q12,  d0                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29
    VMOVL.u8    q4,   d7                                @ [G7 G6 G5 G4 G3 G2 G1 G0]

    VSHRN.u32   d16,  q8,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8
    VMOVL.u8    q11,  d6                                @ [R7 R6 R5 R4 R3 R2 R1 R0]
    VST1.8      d3,   [r3]!

    VMULL.u16   q9,   d22,  d31                         @ R * K[0]
    VMULL.u16   q12,  d23,  d31
    VMOVN.i16   d4,   q5
    VMLAL.u16   q9,   d8,   d30                         @ + G * K[1]
    VMLAL.u16   q12,  d9,   d30
    VSRI.u8     d4,   d4,   #5                          @ d4: blue
    VMOV        d5,   d7                                @ d5: green
    VMOVL.u8    q11,  d4                                @ [B7 B6 B5 B4 B3 B2 B1 B0]
    VMLAL.u16   q9,   d22,  d29                         @ + B * K[2]
    VMLAL.u16   q12,  d23,  d29

    VSHRN.u32   d18,  q9,   #10                         @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_16 >> QBITS
    VSHRN.u32   d19,  q12,  #10
    VMOVN.u16   d7,   q9                                @ [Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0]_8
    VST1.8      d7,   [r5]!
gpu_csc_RGBToNV12_line_loop_batch_getUV:
    VADD.u16    q8,   q9,   q8                          @ [Y07+Y17 Y06+Y16 Y05+Y15 Y04+Y14 Y03+Y13 Y02+Y12 Y01+Y11 Y00+Y10]
    VADDL.u8    q3,   d2,   d6                          @ [R07+R17 R06+R16 R05+R15 R04+R14 R03+R13 R02+R12 R01+R11 R00+R10]
    VADDL.u8    q2,   d0,   d4                          @ [B07+B17 B06+B16 B05+B15 B04+B14 B03+B13 B02+B12 B01+B11 B00+B10]

    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01 + Y10 + Y11
    VPADD.u16   d2,   d6,   d7                          @ R00+R01+R10+R11
    VPADD.u16   d0,   d4,   d5                          @ B00+B01+B10+B11

    VSUB.s16    d2,   d2,   d16                         @ R - Y
    VSUB.s16    d0,   d0,   d16                         @ B - Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #12                         @ >>12
    VSHRN.s32   d18,  q0,   #12                         @ >>12

    VADD.s16    d0,   d19,  d26                         @ +0x80
    VADD.s16    d1,   d18,  d26

    VZIP.s16    d1,   d0

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff
    TST         r3,   #27
    BNE         gpu_csc_RGBToNV12_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    PLD         [r5,  #32]
    BNE         gpu_csc_RGBToNV12_line_loop_batch_skipPLD
    PLD         [r1,  #32]

gpu_csc_RGBToNV12_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VREV64.32   d0,   d0
    VST1.8      {d0}, [r1]!                             @ store UV
    BGE         gpu_csc_RGBToNV12_line_loop_batch

gpu_csc_RGBToNV12_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_RGBToNV12_line_tail          @ next pair of lines

@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_RGBToNV12_line_loop_lastpixel

gpu_csc_RGBToNV12_line_loop_nobatch:
    VLD1.32     d0[0],[r0]!
    VLD1.32     d0[1],[r4]!
    VMOVN.u16   d2,   q0
    VSHRN.u16   d16,  q0,   #3
    VSHRN.u16   d18,  q0,   #8
    VSHL.i8     d2,   d2,   #3
    VSRI.u8     d2,   d2,   #5                          @ d2[0]:  [B B B B]
    VSRI.u8     d16,  d16,  #6                          @ d16[0]: [G G G G]
    VSRI.u8     d18,  d18,  #5                          @ d18[0]: [R R R R]
    VMOVL.u8    q1,   d2                                @ d2:  [0 B 0 B 0 B 0 B]
    VMOVL.u8    q8,   d16                               @ d16: [0 G 0 G 0 G 0 G]
    VMOVL.u8    q9,   d18                               @ d18: [0 R 0 R 0 R 0 R]
    VMOVL.u16   q1,   d2                                @ q1:  [0 0 0 B 0 0 0 B 0 0 0 B 0 0 0 B]
    VMOVL.u16   q8,   d16                               @ q8:  [0 0 0 G 0 0 0 G 0 0 0 G 0 0 0 G]
    VMOVL.u16   q9,   d18                               @ q9:  [0 0 0 R 0 0 0 R 0 0 0 R 0 0 0 R]
    VSHL.i32    q8,   q8,   #8                          @ q8:  [0 0 G 0 0 0 G 0 0 0 G 0 0 0 G 0]
    VSHL.i32    q9,   q9,   #16                         @ q9:  [0 R 0 0 0 R 0 0 0 R 0 0 0 R 0 0]
    VORR.i16    q1,   q1,   q8
    VORR.i16    q1,   q1,   q9

    VMOVL.u8    q0,   d2                                @ [A01 R01 G01 B01 A00 R00 G00 B00]
    VMOVL.u8    q1,   d3                                @ [A11 R11 G11 B11 A10 R10 G10 B10]

    VMULL.u16   q8,   d0,   d20                         @ [0 R00*K0 G00*K1 B00*K2]
    VMULL.u16   q9,   d1,   d20                         @ [0 R01*K0 G01*K1 B01*K2]
    VMULL.u16   q11,  d2,   d20                         @ [0 R10*K0 G10*K1 B10*K2]
    VMULL.u16   q12,  d3,   d20                         @ [0 R11*K0 G11*K1 B11*K2]

    VPADD.u32   d4,   d16,  d17                         @ [R*K0 G*K1+B*K2]
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d7,   d24,  d25

    VPADD.u32   d4,   d4,   d5                          @ R*K0 + G*K1 + B*K2
    VPADD.u32   d5,   d6,   d7

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0],[r3]!                             @ store Y01 Y00
    VST1.16     d5[1],[r5]!                             @ store Y11 Y10

gpu_csc_RGBToNV12_line_loop_nobatch_getUV:
    VADD.u16    q0,   q0,   q1
    VADD.u16    d0,   d0,   d1                          @ [A00+A01+A10+A11 R00+R01+R10+R11 G00+G01+G10+G11 B00+B01+B10+B11]

    VPADDL.u16  d4,   d4
    VPADDL.u32  d4,   d4                                @ Y00+Y01+Y10+Y11
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #12                         @ >>12
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]
    VREV32.16   d4,   d4

    VST1.8      d4[2],[r1]!                             @ store UV
    VST1.8      d4[0],[r1]!                             @ store UV

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_RGBToNV12_line_loop_nobatch
    ADDS        r12,  r12,  #2
    BLE         gpu_csc_RGBToNV12_line_tail

gpu_csc_RGBToNV12_line_loop_lastpixel:
    VLD1.16     d0[0], [r0]!
    VLD1.16     d0[2], [r4]!
    VMOVN.u16   d2,   q0
    VSHRN.u16   d16,  q0,   #3
    VSHRN.u16   d18,  q0,   #8
    VSHL.i8     d2,   d2,   #3
    VSRI.u8     d2,   d2,   #5                          @ d2[0]:  [X B X B]
    VSRI.u8     d16,  d16,  #6                          @ d16[0]: [X G X G]
    VSRI.u8     d18,  d18,  #5                          @ d18[0]: [X R X R]
    VMOVN.u16   d2,   q1                                @ d2[0]:  [X X B B]
    VMOVN.u16   d16,  q8                                @ d16[0]: [X X G G]
    VMOVN.u16   d18,  q9                                @ d18[0]: [X X R R]
    VMOVL.u8    q1,   d2                                @ d2:  [0 X 0 X 0 B 0 B]
    VMOVL.u8    q8,   d16                               @ d16: [0 X 0 X 0 G 0 G]
    VMOVL.u8    q9,   d18                               @ d18: [0 X 0 X 0 R 0 R]
    VMOVL.u16   q1,   d2                                @ d2:  [0 0 0 B 0 0 0 B]
    VMOVL.u8    q9,   d18
    VSHLL.u16   q8,   d16,  #8                          @ d16: [0 0 G 0 0 0 G 0]
    VSHL.u32    d18,  d18,  #16                         @ d18: [0 R 0 0 0 R 0 0]
    VORR.i16    d1,   d2,   d16
    VORR.i16    d1,   d1,   d18

    VMOVL.u8    q0,   d1

    VMULL.u16   q8,   d0,   d20                         @ 0 R00*K0 G00*K1 B00*K2
    VMULL.u16   q11,  d1,   d20                         @ 0 R10*K0 G10*K1 B10*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d6,   d22,  d23
    VPADD.u32   d4,   d4,   d6                          @ [Y10 Y00]

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y00
    VST1.8      d5[1],[r5]!                             @ store Y10

    VADD.u16    d0,   d0,   d1                          @ [A00+A10 R00+R10 G00+G10 B00+B10]
    VPADDL.u16  d4,   d4                                @ Y00+Y10
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]
    VREV32.16   d4,   d4

    VST1.8      d4[2],[r1]!                             @ store UV
    VST1.8      d4[0],[r1]!                             @ store UV

    B           gpu_csc_RGBToNV12_line_tail

gpu_csc_RGBToNV12_last_line_head:                       @ last line
    SUBS        r12,  r12,  #8                          @ test target length
    BLT         gpu_csc_RGBToNV12_last_line_loop_batch_end

gpu_csc_RGBToNV12_last_line_loop_batch:
    VLD1.16     {d0,  d1},  [r0]!
    PLD         [r0,  #16]
    VSHRN.i16   d2,   q0,   #8                          @ shift red elements right and narrow
    VSHRN.i16   d3,   q0,   #3                          @ shift green elements right and narrow
    VSRI.u8     d2,   d2,   #5                          @ d2: red
    VSRI.u8     d3,   d3,   #6
    VSHL.i16    q0,   q0,   #3                          @ shift blue elements right and narrow
    VMOVN.i16   d0,   q0
    VMOV        d1,   d3                                @ d1: green
    VSRI.u8     d0,   d0,   #5                          @ d0: blue

    VMOVL.u8    q12,  d2                                @ R
    VMULL.u16   q8,   d24,  d31                         @ R * K[0]
    VMULL.u16   q11,  d25,  d31
    VMOVL.u8    q12,  d1                                @ G
    VMLAL.u16   q8,   d24,  d30                         @ + G * K[1]
    VMLAL.u16   q11,  d25,  d30
    VMOVL.u8    q12,  d0                                @ B
    VMLAL.u16   q8,   d24,  d29                         @ + B * K[2]
    VMLAL.u16   q11,  d25,  d29

    VSHRN.u32   d16,  q8,   #10                         @ Y >> QBITS
    VSHRN.u32   d17,  q11,  #10
    VMOVN.u16   d3,   q8
    VST1.8      d3,   [r3]!

gpu_csc_RGBToNV12_last_line_loop_batch_getUV:
    VPADD.u16   d16,  d16,  d17                         @ Y00 + Y01
    VPADDL.u8   d2,   d2                                @ R00+R01
    VPADDL.u8   d0,   d0                                @ B00+B01

    VSUB.s16    d2,   d2,   d16                         @ R -Y
    VSUB.s16    d0,   d0,   d16                         @ B -Y

    VMULL.s16   q1,   d2,   d27                         @ (R -Y) * 0x382
    VMULL.s16   q0,   d0,   d28                         @ (B -Y) * 0x1f8

    VSHRN.s32   d19,  q1,   #11                         @ >>11
    VSHRN.s32   d18,  q0,   #11                         @ >>11

    VADD.s16    d1,   d19,  d26                         @ +0x80
    VADD.s16    d0,   d18,  d26

    VZIP.s16    d1,   d0

    VQSHRUN.s16 d0,   q0,   #0                          @ pack, clamp with 0 and 0xff
    VREV16.8    d0,   d0
    TST         r3,   #27
    BNE         gpu_csc_RGBToNV12_last_line_loop_batch_skipPLD
    TST         r1,   #29
    PLD         [r3,  #32]
    BNE         gpu_csc_RGBToNV12_last_line_loop_batch_skipPLD
    PLD         [r1,  #32]
gpu_csc_RGBToNV12_last_line_loop_batch_skipPLD:
    SUBS        r12,  r12,  #8                          @ test target length
    VREV64.32   d0,   d0
    VST1.8      {d0}, [r1]!                             @ store dst
    BGE         gpu_csc_RGBToNV12_last_line_loop_batch

gpu_csc_RGBToNV12_last_line_loop_batch_end:
    ADDS        r12,  r12,  #8                          @ restore remains of target length
    BEQ         gpu_csc_RGBToNV12_line_tail             @ next pair of lines
@@
@@ target is too short
@@
    SUBS        r12,  r12,  #2                          @ test target length
    BLT         gpu_csc_RGBToNV12_last_line_loop_lastpixel
gpu_csc_RGBToNV12_last_line_loop_nobatch:
    VLD1.32     d0[0], [r0]!
    VMOVN.u16   d2,   q0
    VSHRN.u16   d16,  q0,   #3
    VSHRN.u16   d18,  q0,   #8
    VSHL.i8     d2,   d2,   #3
    VSRI.u8     d2,   d2,   #5                          @ d2[0]:  [X X B B]
    VSRI.u8     d16,  d16,  #6                          @ d16[0]: [X X G G]
    VSRI.u8     d18,  d18,  #5                          @ d18[0]: [X X R R]
    VMOVL.u8    q1,   d2                                @ d2:   [0 X 0 X 0 B 0 B]
    VMOVL.u8    q8,   d16                               @ d16:  [0 X 0 X 0 G 0 G]
    VMOVL.u8    q9,   d18                               @ d18:  [0 X 0 X 0 R 0 R]
    VMOVL.u16   q1,   d2                                @ d2:   [0 0 0 B 0 0 0 B]
    VMOVL.u16   q8,   d16                               @ d16:  [0 0 0 G 0 0 0 G]
    VMOVL.u16   q9,   d18                               @ d18:  [0 0 0 R 0 0 0 R]
    VSHL.i32    d16,  d16,  #8                          @ d16:  [0 0 G 0 0 0 G 0]
    VSHL.i32    d18,  d18,  #16                         @ d18:  [0 R 0 0 0 R 0 0]
    VORR.i16    d2,   d2,   d16
    VORR.i16    d2,   d2,   d18

    VMOVL.u8    q0,   d2

    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2
    VMULL.u16   q9,   d1,   d20

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d5,   d18,  d19
    VPADD.u32   d4,   d4,   d5

    VSHRN.u32   d4,   q2,   #10                         @ [Y1 Y0] >> QBITS
    VMOVN.u16   d5,   q2

    VST1.16     d5[0], [r3]!

gpu_csc_RGBToNV12_last_line_loop_nobatch_getUV:
    VADD.u16    d0,   d0,   d1                          @ [A00+A01 R00+R01 G00+G01 B00+B01]
    VPADDL.u16  d4,   d4                                @ Y00+Y01
    VDUP.u16    d4,   d4[0]

    VSUB.s16    d0,   d0,   d4                          @ [A R G B] - [Y Y Y Y]
    VMULL.s16   q2,   d0,   d21                         @ [A-Y R-Y G-Y B-Y] * [0 K4 0 K3]
    VSHRN.s32   d4 ,  q2,   #11                         @ >>11
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff [0 V0 0 U0]
    VREV32.16   d4,   d4

    VST1.8      d4[2],[r1]!                             @ store UV
    VST1.8      d4[0],[r1]!                             @ store UV

    SUBS        r12,  r12,  #2                          @ test target length
    BGE         gpu_csc_RGBToNV12_last_line_loop_nobatch
    ADDS        r12,  r12,  #2
    BLE         gpu_csc_RGBToNV12_line_tail

gpu_csc_RGBToNV12_last_line_loop_lastpixel:
    VLD1.16     d0[0], [r0]!
    VMOVN.u16   d2,   q0
    VSHRN.u16   d16,  q0,   #3
    VSHRN.u16   d18,  q0,   #8
    VSHL.i8     d2,   d2,   #3
    VSRI.u8     d2,   d2,   #5                          @ d2[0]:  [X X X B]
    VSRI.u8     d16,  d16,  #6                          @ d16[0]: [X X X G]
    VSRI.u8     d18,  d18,  #5                          @ d18[0]: [X X X R]
    VMOVL.u8    q1,   d2                                @ d2:  [0 X 0 X 0 X 0 B]
    VMOVL.u8    q8,   d16                               @ d16: [0 X 0 X 0 X 0 G]
    VMOVL.u8    q9,   d18                               @ d18: [0 X 0 X 0 X 0 R]
    VMOVL.u16   q1,   d2                                @ q1:  [0 0 0 X 0 0 0 X 0 0 0 X 0 0 0 B]
    VMOVL.u16   q8,   d16                               @ q8:  [0 0 0 X 0 0 0 X 0 0 0 X 0 0 0 G]
    VMOVL.u16   q9,   d18                               @ q9:  [0 0 0 X 0 0 0 X 0 0 0 X 0 0 0 R]
    VSHL.i32    d16,  d16,  #8                          @ q8:  [0 0 X 0 0 0 X 0 0 0 X 0 0 0 G 0]
    VSHL.i32    d18,  d18,  #16                         @ q9:  [0 X 0 0 0 X 0 0 0 X 0 0 0 R 0 0]
    VORR.i16    d1,   d2,   d16
    VORR.i16    d1,   d1,   d18
    VMOVL.u8    q0,   d1

    VMULL.u16   q8,   d0,   d20                         @ 0 R*K0 G*K1 B*K2

    VPADD.u32   d4,   d16,  d17
    VPADD.u32   d4,   d4,   d4

    VSHRN.u32   d4,   q2,   #10                         @ Y >> QBITS
    VMOVN.u16   d5,   q2

    VST1.8      d5[0],[r3]!                             @ store Y

    VDUP.u16    d4,   d4[0]
    VSUB.s16    d0,   d0,   d4
    VMULL.s16   q2,   d0,   d21
    VSHRN.s32   d4 ,  q2,   #10                         @ >>10
    VADD.s16    d4,   d4,   d26                         @ +0x80

    VQSHRUN.s16 d4,   q2,   #0                          @ pack, clamp with 0 and 0xff
    VREV32.16   d4,   d4

    VST1.8      d4[2],[r1]!                             @ store UV
    VST1.8      d4[0],[r1]!                             @ store UV
@@
@@ Switch next pair of lines
@@
gpu_csc_RGBToNV12_line_tail:
    ADD         r0,   r0,   r8                          @ next source line 1
    ADD         r4,   r4,   r8                          @ next source line 2
    ADD         r1,   r1,   r10                         @ next U-plane line
    ADD         r3,   r3,   r9                          @ next Y-plane line 1
    ADD         r5,   r5,   r9                          @ next Y-plane line 2

    MOV         r12,  r6                                @ restore width counter

    SUBS        r7,   r7,   #2                          @ decrease target height
    BGT         gpu_csc_RGBToNV12_line_head             @ to the next pair of lines
    VLDMIA      sp!,  {d8 - d11}
    LDMIA       sp!,  {r4 - r11, pc}                    @ pop registers & return
@   ENDP        @gpu_csc_RGBToNV12



