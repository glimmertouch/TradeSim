CXX := ccache g++
CXXFLAGS := -O2 -std=c++20 -Wall -Wextra -pthread -I./src

SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=build/%.o)
DEPS := $(OBJS:.o=.d)
BIN := build/main

all: $(BIN)

# Build directory
build:
	@mkdir -p build

# Object files with auto dependency generation
build/%.o: $(SRC_DIR)/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN): $(OBJS) | build
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

clean:
	rm -rf build

.PHONY: all clean

# Include auto-generated dependency files if present
-include $(DEPS)
