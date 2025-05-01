# === Project settings ===
TARGET       := build/Oscilloscopio
CC           := gcc
SRC_DIR      := src
BUILD_DIR    := build
OBJS_DIR     := build/objs
LIB_DIR      := libs
INCLUDE_DIR  := include

# === Gather source and object files ===
SRCS         := $(wildcard $(SRC_DIR)/*.c)
OBJS         := $(patsubst $(SRC_DIR)/%.c, $(OBJS_DIR)/%.o, $(SRCS))

# === Compiler and linker flags ===
CFLAGS       := -Wall -O2 `pkg-config --cflags gtk4 gl` -I$(INCLUDE_DIR) -D_POSIX_C_SOURCE_=199309L
LDFLAGS      := `pkg-config --libs gtk4 gl` -L$(LIB_DIR) -Wl,-rpath,$(LIB_DIR) -leml -lm

# === Default target ===
all: $(TARGET)

# === Ensure build directory exists ===
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(OBJS_DIR)

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

# === Build final executable ===
$(TARGET): $(BUILD_DIR) $(OBJS_DIR) $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# === Compile each source file to object ===
$(OBJS_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# === Clean build artifacts ===
clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: all clean

