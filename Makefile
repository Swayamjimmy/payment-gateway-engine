CXX = g++
CXXFLAGS = -std=c++17 -pthread -O2 -Wall -Wextra -Iinclude
LDFLAGS = -pthread -latomic

# Default target builds both binaries
all: payment_engine stress_test

# 1. The HTTP Backend Server (Excludes simulator.o!)
payment_engine: build/main.o build/tree.o build/atomic_tree.o
	$(CXX) build/main.o build/tree.o build/atomic_tree.o $(LDFLAGS) -o payment_engine

# 2. The Standalone CLI Stress Test (Excludes main.o!)
stress_test: build/simulator.o build/tree.o build/atomic_tree.o
	$(CXX) build/simulator.o build/tree.o build/atomic_tree.o $(LDFLAGS) -o stress_test

# Pattern rule to compile any .cpp file into a .o object file
build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build payment_engine stress_test