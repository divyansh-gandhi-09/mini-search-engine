# Compiler + flags
CXX = g++
CXXFLAGS = -std=c++20 -g -I"header files" -Ithird_party -MMD
LDFLAGS = -lws2_32

# Source files
SRC = $(wildcard implementation/*.cpp)

# Build folder
BUILD_DIR = build

# Object & dependency files in build/
OBJ = $(patsubst implementation/%.cpp,$(BUILD_DIR)/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

# Executable
SERVER = server.exe

# Default target
all: $(SERVER)

# Link the executable
$(SERVER): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# Compile object files & generate dependency files in build/
$(BUILD_DIR)/%.o: implementation/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

# Create build folder if it doesn't exist
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# Include dependency files
-include $(DEP)

# Clean up
clean:
	del /Q $(BUILD_DIR)\*.o $(BUILD_DIR)\*.d $(SERVER) 2>nul || true
