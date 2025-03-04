# Set cross-compiler prefix (default to native gcc)
CROSS_COMPILE ?= 

# Compiler and flags
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g -pthread
LDFLAGS ?=
# Target program name
TARGET := aesdsocket

# Source file
SRC := aesdsocket.c

# Object file
OBJ := $(SRC:.c=.o)

# Default target to build everything
.PHONY: all
all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS)$(LDFLAGS) -o $@ $^

# Compile rule for .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJ)

