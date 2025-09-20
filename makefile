CXX = g++
CXXFLAGS = -std=c++20 -g -I"header files" -Ithird_party -MMD -MP

# Platform-specific linker flags
ifeq ($(OS),Windows_NT)
	LDFLAGS = -lws2_32 -lstdc++fs
else
	LDFLAGS = -lstdc++fs -pthread
endif

# Sources: everything in implementation (includes server.cpp)
SRC = $(wildcard implementation/*.cpp)

BUILD_DIR = build

# Object files
OBJ = $(patsubst implementation/%.cpp,$(BUILD_DIR)/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

SERVER = server.exe

all: $(SERVER)

$(SERVER): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# Build rules
$(BUILD_DIR)/%.o: implementation/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
else
	@mkdir -p $(BUILD_DIR)
endif

# Include dependency files
-include $(DEP)

# Debug build
debug: CXXFLAGS += -DDEBUG -O0
debug: $(SERVER)

# Release build  
release: CXXFLAGS += -DNDEBUG -O3
release: clean $(SERVER)

# Cross-platform clean
ifeq ($(OS),Windows_NT)
clean:
	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"
	@if exist "$(SERVER)" del /Q "$(SERVER)"
else
clean:
	rm -rf $(BUILD_DIR) $(SERVER)
endif

.PHONY: all clean debug release