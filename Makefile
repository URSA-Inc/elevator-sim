UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CLANG = clang-18
LLC = llc-18
CC = gcc

LDFLAGS = -lpaho-mqtt3cs -lncurses
CFLAGS = -g -Wall
endif

ifeq ($(UNAME), Darwin)
CLANG = clang
LLC = llc
CC = gcc

LDFLAGS = -L/opt/homebrew/lib/ -lpaho-mqtt3cs -lncurses
CFLAGS = -I/opt/homebrew/include/ -Wall
endif

all: elevator_sim breakdown fire_response

elevator_sim: elevator_sim.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) $< -o $@

breakdown: breakdown.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) $< -o $@

fire_response: fire_response.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f elevator_sim breakdown fire_response *.o 

