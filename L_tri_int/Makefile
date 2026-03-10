all: interpret

interpret: interpret.o
	$(MAKE) -C ../parser/
	$(CC) $(CFLAGS) -o $@ $^ ../parser/parser.o ../parser/AST.o $(LDFLAGS)

clean:
	rm *.o || true
	rm interpret || true
