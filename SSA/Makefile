all: tests

tests: tests.o SSA.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dump: tests
	./tests

clean:
	rm -f *.o
	rm -f tests
