# Compiler + flags
CXX = g++
CXXFLAGS = -std=c++20 -g -I"header files" -Ithird_party -MMD
LDFLAGS = -lws2_32

# Sources & objects
SRC = $(wildcard implementation/*.cpp)
OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)

# Executable
SERVER = server.exe

# Default target
all: $(SERVER)

# Link the executable
$(SERVER): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# Compile object files & generate dependency files
implementation/%.o: implementation/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include dependency files (auto-rebuild on header change)
-include $(DEP)

# Clean up
clean:
	del /Q implementation\*.o $(SERVER) 2>nul || true
	del /Q implementation\*.d 2>nul || true
