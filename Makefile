CC       = gcc
CFLAGS   = -Wall -Wextra -g -pthread
LDFLAGS  = -pthread -luuid

SRC_DIR  = src
OBJ_DIR  = obj
BIN_DIR  = bin

# All source files (add new .c files here as you build them)
SOURCES  = $(wildcard $(SRC_DIR)/*.c)
OBJECTS  = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET   = $(BIN_DIR)/unisync

.PHONY: all clean test test-threads

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "  ✓ Build complete: $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Standalone threading demo
test-threads: dirs
	$(CC) $(CFLAGS) -o $(BIN_DIR)/test_threads tests/test_threads.c $(LDFLAGS)
	@echo "  Run: ./$(BIN_DIR)/test_threads"

clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*