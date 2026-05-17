CC ?= cc
CFLAGS ?= -O2 -g
LDFLAGS ?=
DEPFLAGS ?= -MMD -MP

OBJS := main.o interpret.o
DEPS := $(OBJS:.o=.d)
EXT_OBJS := ../parser/parser.o ../parser/AST.o ../htable/htable.o

.PHONY: all clean deps

all: main

deps:
	$(MAKE) -C ../parser all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../htable all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

main: deps $(OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXT_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) main

-include $(DEPS)
