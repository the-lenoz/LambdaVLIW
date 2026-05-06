all: main

main: main.o ../htable/htable.o ../parser/AST.o ../parser/parser.o ../L_defun/frontend.o ../SSA/SSA.o ../C_backend/backend.o ../SSA/SSA_dump_graphviz.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm *.o -f
	rm main -f
