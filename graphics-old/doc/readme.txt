
    GC500/GC600/GC800 Unified Graphics Driver for Android
    Ver0.8.0.1155

1.  Introduction
    This package contains GC500/GC600/GC800 unified graphics driver for
    android and some documents.
    This document is used to describe the package contents, how to build
    kernel mode driver and how to install the driver binaries to device.

2.  Revision History
    Ver0.8.0.1155
    1) First release package of MMP2 graphics driver for android eclair.

    Ver0.8.0.1108
    1) Pre-alpha1 release for ARMADA 610 (MMP2) generic linux.
    2) Based on GC 2009.9.patch release, including recent bug fixes and
       optimizations.

    Ver0.8.0.1006
    1) First release package of TavorPV2-A0 graphics driver for generic linux.

3.  Package Contents
    --/                   -- Package root directory
       doc                -- Documentation
       galcroe_src        -- GC source code needed to build kernel mode driver
       prebuilt           -- Prebuilt driver binaries, just for reference, you need to build them by yourself.
       galcore            -- Dir to placed galcore.ko you build
       hgl                -- Dir placed user mode static link libraries to build
       Mdroid.mk          -- Android make file to build user mode driver and copy gc driver to android output dir

4.  Build and Installation
4.1 System Requirement
    Host PC :
       Operating system   -- Ubuntu 8.04 or newer
       Toolchain          -- arm-marvell-linux-gnueabi-vfp
    Target Device:
        Platform          -- PXA950 processor EVB platform
                             PXA968 processor EVB platform
                             ARMADA 610 processor EVB platform
        Operation system  -- Linux kernel 2.6.29 or newer

4.2  Build Kernel Mode Driver
     1) Untar release package, and copy them to android_home/vendor/marvell/generic/graphics
     2) cd android_home
     3) . ./build/envsetup.sh
     4) chooseproduct $product (according to your ENV)
     5) cd kernel
     6) make all
     If no error, the build result galcore.ko will be placed on out/modules.

4.3 Build User Mode Driver
    1) Setup env and choose product just like 4.2
    2) cd vendor/marvell/generic/graphics/hgl
    3) mm
    4) Output:
       android_output_dir/system/lib/modules/galcore.ko
       android_output_dir/system/lib/libGAL.so
       android_output_dir/system/lib/egl/egl.cfg
       android_output_dir/system/lib/egl/libEGL_MRVL.so
       android_output_dir/system/lib/egl/libGLESv1_CM_MRVL.so
       android_output_dir/system/lib/egl/libGLESv2_MRVL.so
       android_output_dir/system/lib/libgcu.so

4.4 Installation
    1) Add galcore insmod cmdline in init.rc
    For ARMADA610(MMP2)
       mknod    /dev/galcore c 199 0
       insmod    /lib/modules/galcore.ko registerMemBase=0xd420d000 irqLine=8 contiguousSize=0x2000000

    For PXA950(TavorPV2)
       mknod    /dev/galcore c 199 0
       insmod    /lib/modules/galcore.ko registerMemBase=0x54000000 irqLine=70 contiguousSize=0x1000000

    2) Push gc libs into device by adb tools
       adb root && adb remount
       adb push android_output_dir/system/lib/modules/galcore.ko /system/lib/modules/
       adb push android_output_dir/system/lib/libGAL.so /system/lib/
       adb push android_output_dir/system/lib/egl/egl.cfg /system/lib/egl/
       adb push android_output_dir/system/lib/egl/libEGL_MRVL.so /system/lib/egl/
       adb push android_output_dir/system/lib/egl/libGLESv1_CM_MRVL.so /system/lib/egl/
       adb push android_output_dir/system/lib/egl/libGLESv2_MRVL.so /system/lib/egl/
       adb push android_output_dir/system/lib/libgcu.so /system/lib/
    Reboot the device and run sample applications on device.

5.  FAQ
    1. "We can't find the arm-marvell-linux-gnueabi-vfp toolchain".
       Contact your Marvell representative to get arm-marvell-linux-gnueabi-vfp toolchain.

6.  Known Issues
    Problem   : The OpenVG implementation does not enable anti-alias by default.
    Workaround: Choose the EGL configuration with EGL_SAMPLES = 4.




