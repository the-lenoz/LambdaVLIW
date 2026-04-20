all: interpret

interpret: interpret.o 
	$(MAKE) -C ../parser/
	$(MAKE) -C ../htable/
	$(CC) $(CFLAGS) -o $@ $^ ../parser/parser.o ../parser/AST.o ../htable/htable.o $(LDFLAGS)
