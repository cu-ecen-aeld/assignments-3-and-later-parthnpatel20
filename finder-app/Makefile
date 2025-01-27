# Compiler and flags
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror

# Targets
.PHONY: all clean

all: writer

writer: writer.c
	$(CC) $(CFLAGS) -o writer writer.c

clean:
	rm -f writer

