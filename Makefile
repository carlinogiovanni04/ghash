CC ?= clang
CFLAGS = -std=c11 -g -O2 -Wall -Wextra -Werror
INCLUDE = include/
OUT = bin/ghash_test
EXAMPLE_OUT = bin/ghash_example
PROFILE_OUT = bin/profile_ghash
TRACE = bin/ghash_trace.json

all: $(OUT) $(EXAMPLE_OUT)

$(OUT): tests/test.c include/ghash.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -I$(INCLUDE) tests/test.c -o $(OUT)

$(EXAMPLE_OUT): examples/example.c include/ghash.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -I$(INCLUDE) examples/example.c -o $(EXAMPLE_OUT)

$(PROFILE_OUT): examples/profile_ghash.c include/ghash.h ../microprof/include/microprof.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -I$(INCLUDE) -I../microprof/include examples/profile_ghash.c -o $(PROFILE_OUT)

test: $(OUT)
	./$(OUT)

example: $(EXAMPLE_OUT)
	./$(EXAMPLE_OUT)

# Build the profiler and write a microprof trace the gview viewer can open.
trace: $(PROFILE_OUT)
	./$(PROFILE_OUT) $(TRACE)

clean:
	rm -rf bin/
