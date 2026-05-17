CC ?= cc
CFLAGS ?= -O2 -g
LDFLAGS ?=
DEPFLAGS ?= -MMD -MP

LIB_OBJS := SSA.o SSA_to_L_tri_call.o SSA_dump_graphviz.o
TEST_OBJS := tests.o
OBJS := $(LIB_OBJS) $(TEST_OBJS)
DEPS := $(OBJS:.o=.d)

.PHONY: all tests dump clean

all: $(LIB_OBJS)

tests: $(TEST_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dump: tests
	./tests

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) tests

-include $(DEPS)
