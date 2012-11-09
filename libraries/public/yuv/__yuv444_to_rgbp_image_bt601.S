/*
 *  Copyright (C) 2009-2012 Texas Instruments, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

.text
.arch armv7-a
.fpu neon

.global __yuv444_to_rgbp_image_bt601

.include "yuv.inc"

__yuv444_to_rgbp_image_bt601:
width       .req r0
height      .req r1
pY          .req r2
pU          .req r3
pV          .req r4
srcStride   .req r5
srcStep     .req r5
pR          .req r6
pG          .req r7
pB          .req r8
dstStride   .req r9
dstStep     .req r9
pixelsLeft  .req r10
linesLeft   .req r11
tmp         .req r12

        PROLOG  r0, r12
        ldr     r4, [sp, #(14*4)] /* pV */
        ldr     r5, [sp, #(15*4)] /* srcStride */
        ldr     r6, [sp, #(16*4)] /* pR */
        ldr     r7, [sp, #(17*4)] /* pG */
        ldr     r8, [sp, #(18*4)] /* pB */
        ldr     r9, [sp, #(19*4)] /* dstStride */
        mov     linesLeft, height
        sub     srcStep, srcStride, width
        sub     dstStep, dstStride, width
        mov     tmp, #L2_LINE_SIZE
        FILL_YCBCR_TO_RGB_BT601 q15,q14,q13,q12,q11,d21,d20
__yuv444_to_rgbp_image_bt601_line:
        mov     pixelsLeft, width
        pld     [pY]
        pld     [pU]
        pld     [pV]
__yuv444_to_rgbp_image_bt601_pixels:
        pld     [pY, tmp]
        vld1.8  {d2}, [pY]! /* d2 = [y0,y1,y2,y3,y4,y5,y6,y7] */
        vld1.8  {d1}, [pU]! /* d1 = [u0,u1,u2,u3,u4,u5,u6,u7] */
        vld1.8  {d0}, [pV]! /* d0 = [v0,v1,v2,v3,v4,v5,v6,v7] */
        pld     [pU, tmp]
        vsubl.u8    q2, d2, d20     /* q2 = Y - 16 */
        vsubl.u8    q1, d1, d21     /* q1 = Cb - 128 */
        vsubl.u8    q0, d0, d21     /* q0 = Cr - 128 */
        vmul.s16    q4, q2, q15     /* R = 74*Ye */
        vmla.s16    q4, q0, q12     /* R += 102*Cr */
        vmul.s16    q5, q2, q15     /* G = 74*Ye */
        vmls.s16    q5, q0, q13     /* G -= 52*Cr */
        vmls.s16    q5, q1, q14     /* G -= 25*Cb */
        vmul.s16    q6, q2, q15     /* B = 74*Ye */
        vmla.s16    q6, q1, q11     /* B += 129*Cb */
        pld     [pV, tmp]
        vqshrun.s16 d0, q6, #6      /* B >> 6 */
        vqshrun.s16 d1, q5, #6      /* G >> 6 */
        vqshrun.s16 d2, q4, #6      /* R >> 6 */
        vst1.8  {d2}, [pR]!
        vst1.8  {d1}, [pG]!
        vst1.8  {d2}, [pB]!
        subs    pixelsLeft, pixelsLeft, #8
        bgt     __yuv444_to_rgbp_image_bt601_pixels
        add     pY, pY, srcStep
        add     pU, pU, srcStep
        add     pV, pV, srcStep
        add     pR, pR, dstStep
        add     pG, pG, dstStep
        add     pB, pB, dstStep
        subs    linesLeft, linesLeft, #1
        bgt     __yuv444_to_rgbp_image_bt601_line
        EPILOG  r0, r12

.unreq width
.unreq height
.unreq pY
.unreq pU
.unreq pV
.unreq srcStride
.unreq srcStep
.unreq pR
.unreq pG
.unreq pB
.unreq dstStride
.unreq dstStep
.unreq pixelsLeft
.unreq linesLeft
.unreq tmp

.end
