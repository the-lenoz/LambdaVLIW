all: tests

tests: tests.o SSA.o SSA_dump_L_tri_if.o SSA_dump_graphviz.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dump: tests
	./tests

clean:
	rm -f *.o
	rm -f tests
