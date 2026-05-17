CC ?= cc
CFLAGS ?= -O2 -g
LDFLAGS ?=

MAKEVARS := CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

.PHONY: all tools compiler tests clean debug asan parser htable SSA optimizer C_backend L_defun L_interpret L_int L_tri_if L_tri_call L_var2L_tri_int

all: compiler

tools: parser htable SSA optimizer C_backend L_defun L_interpret L_int L_tri_if L_tri_call L_var2L_tri_int compiler

tests:
	$(MAKE) -C SSA tests $(MAKEVARS)

parser:
	$(MAKE) -C parser all $(MAKEVARS)

htable:
	$(MAKE) -C htable all $(MAKEVARS)

SSA:
	$(MAKE) -C SSA all $(MAKEVARS)

optimizer: SSA
	$(MAKE) -C optimizer all $(MAKEVARS)

C_backend: SSA
	$(MAKE) -C C_backend all $(MAKEVARS)

L_defun: parser htable SSA
	$(MAKE) -C L_defun all $(MAKEVARS)

L_interpret: parser htable
	$(MAKE) -C L_interpret all $(MAKEVARS)

L_int: parser
	$(MAKE) -C L_int all $(MAKEVARS)

L_tri_if: parser htable
	$(MAKE) -C L_tri_if all $(MAKEVARS)

L_tri_call: parser htable
	$(MAKE) -C L_tri_call all $(MAKEVARS)

L_var2L_tri_int: parser
	$(MAKE) -C L_var2L_tri_int all $(MAKEVARS)

compiler: parser htable SSA optimizer C_backend L_defun
	$(MAKE) -C compiler all $(MAKEVARS)

clean:
	$(MAKE) -C compiler clean $(MAKEVARS)
	$(MAKE) -C L_var2L_tri_int clean $(MAKEVARS)
	$(MAKE) -C L_tri_call clean $(MAKEVARS)
	$(MAKE) -C L_tri_if clean $(MAKEVARS)
	$(MAKE) -C L_interpret clean $(MAKEVARS)
	$(MAKE) -C L_int clean $(MAKEVARS)
	$(MAKE) -C L_defun clean $(MAKEVARS)
	$(MAKE) -C C_backend clean $(MAKEVARS)
	$(MAKE) -C optimizer clean $(MAKEVARS)
	$(MAKE) -C SSA clean $(MAKEVARS)
	$(MAKE) -C htable clean $(MAKEVARS)
	$(MAKE) -C parser clean $(MAKEVARS)

debug:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-O0 -g -DDEBUG"

asan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-O0 -g -fsanitize=address -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address"
