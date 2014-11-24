.include "cheridefs.mk"
LD=$(CC)
LDFLAGS=
OBJS=
TESTOBJS=test/test.o

all: test

.PHONY: all clean lib test push
lib: libcherigc.a
test: test/gctest

.c.o: $<
	$(CC) $(CFLAGS) -o $@ -c $<

libcherigc.a: $(OBJS)
	$(AR) -r libcherigc.a $(OBJS)

test/gctest: libcherigc.a $(TESTOBJS)
	$(LD) $(LDFLAGS) -o test/gctest libcherigc.a $(TESTOBJS)

push:
	$(CHERI_PUSH) test/gctest $(CHERI_PUSH_DIR)/gctest

clean:
	rm -f *.o *.a test/*.o test/gctest
