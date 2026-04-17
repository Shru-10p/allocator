CC      := clang
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -g
CPPFLAGS:= -I./src
LDFLAGS :=

SRC     := src/alloc.c src/alloc_debug.c
TESTSRC := tests/test.c
TESTBIN := tests/test
EXAMPLESRC := examples/example.c
EXAMPLEBIN := examples/example

.PHONY: all clean test run example

all: $(TESTBIN)

$(TESTBIN): $(SRC) $(TESTSRC) src/alloc.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) $(TESTSRC) -o $(TESTBIN) $(LDFLAGS)

$(EXAMPLEBIN): $(SRC) $(EXAMPLESRC) src/alloc.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) $(EXAMPLESRC) -o $(EXAMPLEBIN) $(LDFLAGS)

test: $(TESTBIN)

run: $(TESTBIN)
	./$(TESTBIN)

example: $(EXAMPLEBIN)

clean:
	rm -f $(TESTBIN) $(EXAMPLEBIN) *.o src/*.o tests/*.o examples/*.o
