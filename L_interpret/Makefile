all: main

main: main.o interpret.o
	$(MAKE) -C ../parser/
	$(MAKE) -C ../htable/
	$(CC) $(CFLAGS) -o $@ $^ ../parser/parser.o ../parser/AST.o ../htable/htable.o $(LDFLAGS)

clean:
	rm *.o || true
	rm main || true
