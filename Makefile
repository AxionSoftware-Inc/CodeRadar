CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -pthread -Isrc

SRCS = src/main.cpp src/graph.cpp src/parser.cpp src/analyzer.cpp src/json_writer.cpp src/binary_writer.cpp src/query.cpp
TARGET = arch_radar

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) analysis_result.json

.PHONY: all clean
