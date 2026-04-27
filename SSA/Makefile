all: tests

tests: tests.o SSA.o SSA_to_L_tri_call.o SSA_dump_graphviz.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dump: tests
	./tests

clean:
	rm -f *.o
	rm -f tests
