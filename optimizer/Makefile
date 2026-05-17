CC ?= cc
CFLAGS ?= -O2 -g
DEPFLAGS ?= -MMD -MP

OBJS := optimizer.o
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
