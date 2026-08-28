CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude
TARGET = server
SRC = src/main.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC) include/*.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) -pthread

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)