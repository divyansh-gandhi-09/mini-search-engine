# Compiler + flags
CXX = g++
CXXFLAGS = -std=c++20 -g -I"header files" -Ithird_party
LDFLAGS = -lws2_32

# Sources & objects
SRC = $(wildcard implementation/*.cpp)
OBJ = $(SRC:.cpp=.o)

# Executable
SERVER = server.exe

# Default target
all: $(SERVER)

# Server build
$(SERVER): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# Rule for object files
implementation/%.o: implementation/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	del /Q implementation\*.o $(SERVER) 2>nul || true
