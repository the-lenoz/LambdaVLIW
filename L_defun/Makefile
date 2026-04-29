all: main

main: main.o frontend.o ../parser/AST.o ../parser/parser.o ../htable/htable.o ../SSA/SSA.o ../SSA/SSA_to_L_tri_call.o ../SSA/SSA_dump_graphviz.o ../parser/parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm *.o || true
	rm main || true
