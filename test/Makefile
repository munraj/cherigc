.include "../cheridefs.mk"
LD=$(CC)
LDFLAGS=
CFLAGS+=-I..
OBJS=test.o

.PHONY: all clean
all: gctest

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

gctest: ../libcherigc.a $(OBJS)
	$(LD) $(LDFLAGS) -o gctest $(OBJS) ../libcherigc.a

clean:
	rm -f *.o gctest

test.o: test.c ../gc.h
