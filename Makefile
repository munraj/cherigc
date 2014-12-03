.include "cheridefs.mk"
OBJS=gc.o gc_collect.o gc_scan.o gc_stack.o gc_debug.o gc_cheri.o
CFLAGS+=-g

.PHONY: all clean lib test push gctest
all: gctest
lib: libcherigc.a

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

libcherigc.a: $(OBJS)
	$(AR) -r libcherigc.a $(OBJS)

gctest: libcherigc.a
	cd test && $(MAKE)

push: gctest
	$(CHERI_PUSH) test/gctest $(CHERI_PUSH_DIR)/gctest

clean:
	rm -f *.o *.a test/*.o
	cd test && $(MAKE) clean

gc.h: gc_cheri.h gc_stack.h
gc_scan.h: gc_cheri.h
gc_stack.h: gc_cheri.h
gc_debug.h: gc_cheri.h gc.h
gc.o: gc.c gc.h
gc_scan.o: gc_scan.c gc_scan.h gc_debug.h
gc_stack.o: gc_stack.c gc_stack.h gc.h
gc_collect.o: gc_collect.c gc_collect.h gc_debug.h gc.h
gc_debug.o: gc_debug.c gc_debug.h
gc_cheri.o: gc_cheri.c gc_cheri.h gc_debug.h
