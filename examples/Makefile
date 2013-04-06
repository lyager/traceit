# Makefile for engine/fs/utilities/trace/tests/src/host/ directory.
# This is for testing the trace system on the local host architecture.

vpath %.c /src
vpath %.h ../src
vpath %.c ../src

SOURCES := $(notdir $(wildcard *.c))
PROGRAMS := $(patsubst %.c,%.bin,$(SOURCES))
FILES := $(filter-out %.c %~, $(notdir $(wildcard ../src/tst*)))

CFLAGS += -Wall -W -g
LDLIBS += -lpthread
CPPFLAGS += -I../src

all: $(PROGRAMS) $(FILES) traceit

clean:
	rm -f $(PROGRAMS) tmp.* *.o *~ $(FILES)  traceit

trace.o: trace.h

$(PROGRAMS): trace.o

$(FILES): % : ../src/%
	ln -s $< $@

traceit: ../src/traceit
	ln -s $< $@

# Remove the built-in rules.
%: %.c
%: %.o

%.bin: %.c
	$(LINK.c) $^ $(LDLIBS) $(OUTPUT_OPTION)

.PHONY: all clean
