CC ?= cc
CFLAGS ?= -O2 -g
LDFLAGS ?=
DEPFLAGS ?= -MMD -MP

OBJS := main.o
DEPS := $(OBJS:.o=.d)
EXT_OBJS := ../htable/htable.o ../parser/AST.o ../parser/parser.o ../L_defun/frontend.o ../SSA/SSA.o ../C_backend/backend.o ../SSA/SSA_dump_graphviz.o ../optimizer/optimizer.o

.PHONY: all clean deps

all: main

deps:
	$(MAKE) -C ../parser all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../htable all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../SSA all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../optimizer all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../C_backend all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	$(MAKE) -C ../L_defun all CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

main: deps $(OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXT_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) main

-include $(DEPS)
