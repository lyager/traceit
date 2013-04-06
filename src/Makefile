# Make the libtrace.a file.

CFLAGS = -Wall -W -D_REENTRANT

all: libtrace.a

libtrace.a: trace.o
	$(AR) $(ARFLAGS) $@ $<

clean:
	rm -f *.o *.a *~

