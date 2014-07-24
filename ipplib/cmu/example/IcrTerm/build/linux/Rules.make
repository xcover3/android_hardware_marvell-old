
# T: tool chain selection; mavell gcc / arm gcc
#    valuse: -gcc
# CPU_TYPE: CFLAGS selection; -mcpu=iwmmxt -mtune=iwmmxt / -mcpu=xscale -mtune=xscale
#    values: xsc_linux
# PGO: pgo switch; y-enable / n-disable
#    values: y, n
# USE_STATICLIB: use static or dynamic lib switch; y-use static lib / n-use dynamic lib
#    values: y, n

USE_STATICLIB=y

# if pgo, toolchain should be the same with codec lib build
PGO=n


ifeq ($(T), -gcc)

#==============================================================================
# GNU pathes                          (server admin update)
#==============================================================================
PATH_GNU_BIN=/usr/local/arm-linux-4.1.1/bin
#PATH_GNU_BIN=/usr/local/arm-iwmmxt-linux-gnueabi/bin

#==============================================================================
# GNU           binaries             (server admin update)
#==============================================================================
CC=$(PATH_GNU_BIN)/arm-iwmmxt-linux-gnueabi-gcc
CXX=$(PATH_GNU_BIN)/arm-iwmmxt-linux-gnueabi-gcc
AR=$(PATH_GNU_BIN)/arm-iwmmxt-linux-gnueabi-ar
AS=$(PATH_GNU_BIN)/arm-iwmmxt-linux-gnueabi-as
LN=$(PATH_GNU_BIN)/arm-iwmmxt-linux-gnueabi-gcc

#==============================================================================
# GNU build options:                (build engineer update)
#==============================================================================

#cpu: wmmx2_linux, wmmx_linux, xsc_linux
ifeq ($(CPU_TYPE), xsc_linux)
CFLAGS= -mcpu=xscale -mtune=xscale
else
CFLAGS= -mcpu=iwmmxt -mtune=iwmmxt
endif

CXXFLAGS= $(CFLAGS)

ifeq "$(PGO)" "y"
LNFLAGS= -lgcov 
else
LNFLAGS=
endif

else

#==============================================================================
# GNU pathes                          (server admin update)
#==============================================================================
PATH_GNU_BIN=/usr/local/arm-marvell-linux-gnueabi/bin


#==============================================================================
# GNU           binaries                 (server admin update)
#==============================================================================
CC=$(PATH_GNU_BIN)/arm-marvell-linux-gnueabi-gcc
CXX=$(PATH_GNU_BIN)/arm-marvell-linux-gnueabi-gcc
AR=$(PATH_GNU_BIN)/arm-marvell-linux-gnueabi-ar
AS=$(PATH_GNU_BIN)/arm-marvell-linux-gnueabi-as
LN=$(PATH_GNU_BIN)/arm-marvell-linux-gnueabi-gcc

#==============================================================================
# GNU build options:    	     (build engineer update)				
#==============================================================================
CFLAGS= -mcpu=marvell-f 
CXXFLAGS= -mcpu=marvell-f 
ifeq "$(PGO)" "y"
LNFLAGS= -lgcov
else
LNFLAGS=
endif

endif
