all: compiler

compiler: compiler.o ../htable/htable.o ../SSA/SSA.o ../SSA/SSA_to_L_tri_call.o ../SSA/SSA_dump_graphviz.o
	$(MAKE) -C ../parser/
	$(CC) $(CFLAGS) -o $@ $^ ../parser/parser.o ../parser/AST.o $(LDFLAGS)

clean:
	rm *.o || true
	rm compiler || true
