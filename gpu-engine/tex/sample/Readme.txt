/***********************************************************************************
 *
 *    Copyright (c) 2013 by Marvell International Ltd. and its affiliates.
 *    All rights reserved.
 *
 *    This software file (the "File") is owned and distributed by Marvell
 *    International Ltd. and/or its affiliates ("Marvell") under Marvell Commercial
 *    License.
 *
 *    If you received this File from Marvell and you have entered into a commercial
 *    license agreement (a "Commercial License") with Marvell, the File is licensed
 *    to you under the terms of the applicable Commercial License.
 *
 ***********************************************************************************/

If you want to create an ASTC-decoded file by yourself, you can go to
http://malideveloper.arm.com/develop-for-mali/tools/astc-evaluation-codec/
to download ARM ASTC Evaluation Codec.
Unzip it, then you will find in the folder:

win32/astcenc.exe
    Windows executable ASTC encoder
        Built on Windows 7 as a 32-bit executable

linux/astcenc
    Linux executable ASTC encoder
        Built under RHEL but should work on most x86 Linuxes

cygwin-6.1/astcenc.exe
    Cygwin executable ASTC encoder
        Built under Cygwin 6.1

mac/astcenc-mac
        Macintosh executable ASTC encoder
        Build under Snow Leopard, should run under Lion

source/
        The source for the reference codec.

  Then you can choose astcenc.linux to run on linux platform using following command.

  ./astcenc.linux -c source.bmp compress.astc 8x8 -fast;         //compress a bitmap to a file

  Then you can use compress.astc to run our sample.
