# Compiler and flags
CC      = gcc
#CC for macOS in VM. Running gcc with x86_64 architecture to avoid issues with MLV library on ARM-based Macs.
#CC      = x86_64-linux-gnu-gcc
BASE_CFLAGS = -fopenmp -Ofast $(shell pkg-config --cflags MLV)
WARN_CFLAGS = -Wall -Wextra
CFLAGS ?= $(BASE_CFLAGS) $(WARN_CFLAGS)
STRICT_CFLAGS = $(BASE_CFLAGS) $(WARN_CFLAGS) -Werror
LDLIBS  = $(shell pkg-config --libs MLV) -lm -fopenmp

# Target executable name
TARGET  = build/medifrance

# Source files (automatically finds all .c files in the current directory and modules)
SRCS    = $(wildcard *.c) $(wildcard modules/*.c)
OBJS    = $(patsubst %.c,build/%.o,$(SRCS))

# Default rule
all: $(TARGET)

# Strict build for CI (same flags as normal build + -Werror)
quality: clean
	$(MAKE) CFLAGS="$(STRICT_CFLAGS)" all

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDLIBS)

# Compile source files into object files
build/%.o: %.c | build build/modules
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build

build/modules:
	@mkdir -p build/modules

# Clean, build and run the program
run: clean all
	./$(TARGET)

doc:
	doxygen Doxyfile
	@echo "Ouverture de la documentation..."
	@linux-like-command: xdg-open doc/html/index.html || open doc/html/index.html || start doc/html/index.html

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET) *.o
	rm -rf build

.PHONY: all clean quality