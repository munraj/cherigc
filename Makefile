.include "cheridefs.mk"
OBJS=gc.o

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

gc.o: gc.c gc.h
