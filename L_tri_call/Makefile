CC ?= cc
CFLAGS ?= -O2 -g
LDFLAGS ?=
DEPFLAGS ?= -MMD -MP

OBJS := interpret.o
DEPS := $(OBJS:.o=.d)
EXT_OBJS := ../htable/htable.o ../parser/parser.o ../parser/AST.o

.PHONY: all clean deps

all: interpret

deps:
	$(MAKE) -C ../parser all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../htable all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

interpret: deps $(OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXT_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) interpret

-include $(DEPS)
