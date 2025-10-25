CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pthread

SRC := src/tcp_server.cpp
BIN := bin/tcp_server

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf bin

.PHONY: all clean
