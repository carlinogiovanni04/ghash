CC ?= clang
CFLAGS = -std=c11 -g -Wall -Wextra -Werror
INCLUDE = include/
OUT = bin/ghash_test
EXAMPLE_OUT = bin/ghash_example

all: $(OUT) $(EXAMPLE_OUT)

$(OUT): tests/test.c include/ghash.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -I$(INCLUDE) tests/test.c -o $(OUT)

$(EXAMPLE_OUT): examples/example.c include/ghash.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -I$(INCLUDE) examples/example.c -o $(EXAMPLE_OUT)

test: $(OUT)
	./$(OUT)

example: $(EXAMPLE_OUT)
	./$(EXAMPLE_OUT)

clean:
	rm -rf bin/
