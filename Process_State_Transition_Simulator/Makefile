# Makefile - Process State Transition Simulator
#
# Targets:
#   make        (or make all)  build the demonstration program
#   make test                  build and run the automated tests
#   make clean                 delete generated executables
#
# Run these from an MSYS2/MinGW shell (or Git Bash). If "make" is not
# installed, use the direct gcc commands in README.md instead.

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -Iinclude

ifeq ($(OS),Windows_NT)
EXE = .exe
else
EXE =
endif

ENGINE = src/process_simulator.c
DEMO   = simulator$(EXE)
TESTS  = test_simulator$(EXE)

.PHONY: all test clean

all: $(DEMO)

$(DEMO): $(ENGINE) src/main.c include/process_simulator.h
	$(CC) $(CFLAGS) $(ENGINE) src/main.c -o $(DEMO)

$(TESTS): $(ENGINE) tests/test_simulator.c include/process_simulator.h
	$(CC) $(CFLAGS) $(ENGINE) tests/test_simulator.c -o $(TESTS)

test: $(TESTS)
	./$(TESTS)

clean:
	rm -f simulator simulator.exe test_simulator test_simulator.exe
