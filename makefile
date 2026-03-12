# Compiler and flags
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 $(shell pkg-config --cflags MLV)
LDLIBS  = $(shell pkg-config --libs MLV)

# Target executable name
TARGET  = build/medifrance

# Source files (automatically finds all .c files in the current directory)
SRCS    = $(wildcard *.c)
OBJS    = $(SRCS:.c=.o)

# Default rule
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $(TARGET) $(LDLIBS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf build

.PHONY: all clean