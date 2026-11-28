CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -pthread -Isrc

SRCS = src/main.cpp src/graph.cpp src/parser.cpp src/analyzer.cpp src/json_writer.cpp src/binary_writer.cpp src/query.cpp
CORE_SRCS = src/graph.cpp src/parser.cpp src/analyzer.cpp src/json_writer.cpp src/binary_writer.cpp src/query.cpp
TARGET = arch_radar
TEST_TARGET = test_core

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): tests/test_core.cpp $(CORE_SRCS)
	$(CXX) $(CXXFLAGS) tests/test_core.cpp $(CORE_SRCS) -o $(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) analysis_result.json

.PHONY: all test clean
