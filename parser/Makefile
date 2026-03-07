

all: parser

clean:
	@echo "Cleaning..."
	@rm *.o || true
	@rm parser || true
	@echo "Done!"

parser: parser.o main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(LDFLAGS)
