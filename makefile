# Compiler and flags
CC      = gcc
#CC for macOS in VM. Running gcc with x86_64 architecture to avoid issues with MLV library on ARM-based Macs.
#CC      = x86_64-linux-gnu-gcc
CFLAGS  = -Wall -Wextra -std=c99 $(shell pkg-config --cflags MLV)
LDLIBS  = $(shell pkg-config --libs MLV)

# Target executable name
TARGET  = build/medifrance

# Source files (main sources + modules)
SRCS    = $(wildcard *.c modules/*.c)
OBJS    = $(patsubst %.c,build/%.o,$(SRCS))

# Default rule
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDLIBS)

# Compile source files into object files
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET) *.o
	rm -rf build

.PHONY: all clean