CC ?= cc
CFLAGS ?= -O2 -g
LDFLAGS ?=
DEPFLAGS ?= -MMD -MP

OBJS := compiler.o
DEPS := $(OBJS:.o=.d)
EXT_OBJS := ../parser/parser.o ../parser/AST.o

.PHONY: all clean deps

all: compiler

deps:
	$(MAKE) -C ../parser all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

compiler: deps $(OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXT_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) compiler

-include $(DEPS)
