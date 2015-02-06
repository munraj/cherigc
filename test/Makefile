.include "../cheridefs.mk"
LD=$(CC)
LDFLAGS=-g
#LDADD+=-Wl,-Bstatic -lcheri -Wl,-Bstatic
LDADD+=$(CHERI_SYSROOT)/usr/lib/libcheri.a
CFLAGS+=-g -gdwarf-2
CFLAGS+=-Wall -I.. -DTF_FORK
CFLAGS+=-DSB_BIN=\"sb.bin\" -DSB_HPSZ=1048576
OBJS=test.o framework.o test_sb.o cheri_gc.o classes.o

.PHONY: all clean
all: gctest sb.bin

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.S.o:
	$(CC) $(CFLAGS) -o $@ -c $<

gctest: ../libcherigc.a $(OBJS)
	$(LD) $(LDFLAGS) -o gctest $(OBJS) ../libcherigc.a $(LDADD)

clean:
	rm -f *.o *.E gctest sb.elf sb.bin sb.E

test.o: test.c ../gc.h

# Sandbox
sb.bin: sb.elf
	$(OBJCOPY) -S -O binary sb.elf sb.bin

sb.elf: sb.o cheri_gc_stub.o
	$(LD) -Wl,--script=$(CHERI_SANDBOX_LD) -nostdlib -o sb.elf cheri_gc_stub.o sb.o -Wl,-Bstatic -lc_cheri

sb.o: sb.c
	$(CC) $(CFLAGS) -mabi=sandbox -o sb.E -E sb.c
	$(CC) $(CFLAGS) -mabi=sandbox -o sb.o -c sb.c

cheri_gc_stub.o: cheri_gc_stub.c
	$(CC) $(CFLAGS) -mabi=sandbox -o cheri_gc_stub.E -E cheri_gc_stub.c
	$(CC) $(CFLAGS) -mabi=sandbox -o cheri_gc_stub.o -c cheri_gc_stub.c
