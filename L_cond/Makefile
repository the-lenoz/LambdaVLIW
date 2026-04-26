all: compiler

compiler: compiler.o 
	$(MAKE) -C ../parser/
	$(CC) $(CFLAGS) -o $@ $^ ../parser/parser.o ../parser/AST.o $(LDFLAGS)

clean:
	rm *.o || true
	rm compiler || true
