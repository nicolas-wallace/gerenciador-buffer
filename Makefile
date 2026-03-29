CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17

SRC_DIR  = src
BIN_DIR  = bin
TARGET   = $(BIN_DIR)/buffer_manager

SRC      = $(SRC_DIR)/main.cpp

# Arquivo e política padrão para o target run (sobrescrevível via CLI)
FILE     ?= data/bancodedados.csv
POLICY   ?= LRU

# ─── Targets ──────────────────────────────────────────────────────────────────

all: $(BIN_DIR) $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

run: all
	./$(TARGET) $(FILE) $(POLICY)

clean:
	rm -f $(TARGET)

.PHONY: all run clean