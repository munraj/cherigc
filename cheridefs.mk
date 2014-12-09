CHERIBSD?=/home/mbv21/trunk/tools/cheribsd
CHERI_SDK?=/home/mbv21/trunk/tools/sdk
CHERI_SYSROOT:=$(CHERI_SDK)/sysroot
CHERI_SDK_BIN:=$(CHERI_SDK)/bin
CHERI_SANDBOX_LD:=$(CHERI_SYSROOT)/usr/libdata/ldscripts/sandbox.ld
CFLAGS=
LDADD=
CFLAGS+=-O2

CC:=$(CHERI_SDK_BIN)/clang
LD:=$(CHERI_SDK_BIN)/ld
NM:=$(CHERI_SDK)/bin/nm
AS:=$(CHERI_SDK)/bin/as
OBJCOPY:=$(CHERI_SDK)/bin/objcopy
OBJDUMP:=$(CHERI_SDK)/bin/objdump
CFLAGS+=--sysroot=$(CHERI_SYSROOT)
CPOSTFLAGS+=-target cheri-unknown-freebsd -msoft-float -B$(CHERI_SDK)

# CHERI_PUSH is intended to be used in the form
# $(CHERI_PUSH) local_file $(CHERI_PUSH_DIR)/remote_file
CHERI_PUSH=n localhost 8888
CHERI_PULL=n -1 8888
CHERI_PUSH_DIR=/home/mbv21/tmp

MACHINE_ARCH=mips64
MACHINE=mips

# libprocstat
CFLAGS+=-DGC_USE_LIBPROCSTAT
LDADD+=-lprocstat -lelf -lkvm -lutil
