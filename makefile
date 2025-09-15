CXX = g++
CXXFLAGS = -std=c++20 -g -I"header files" -Ithird_party -MMD -MP
LDFLAGS = -lws2_32

# Source files from both directories
IMPL_SRC = $(wildcard implementation/*.cpp)
HEADER_SRC = $(wildcard "header files"/*.cpp)

BUILD_DIR = build

# Object files for both source directories
IMPL_OBJ = $(patsubst implementation/%.cpp,$(BUILD_DIR)/%.o,$(IMPL_SRC))
HEADER_OBJ = $(patsubst header\ files/%.cpp,$(BUILD_DIR)/%.o,$(HEADER_SRC))

# Combine all object files
OBJ = $(IMPL_OBJ) $(HEADER_OBJ)
DEP = $(OBJ:.o=.d)

SERVER = server.exe

all: $(SERVER)

$(SERVER): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# Build rules for implementation directory
$(BUILD_DIR)/%.o: implementation/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build rules for header files directory  
$(BUILD_DIR)/%.o: "header files"/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

# Include dependency files
-include $(DEP)

# Cross-platform clean
ifeq ($(OS),Windows_NT)
clean:
	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"
	@if exist "$(SERVER)" del /Q "$(SERVER)"
else
clean:
	rm -rf $(BUILD_DIR) $(SERVER)
endif

# Debug target to show variables
debug:
	@echo "IMPL_SRC: $(IMPL_SRC)"
	@echo "HEADER_SRC: $(HEADER_SRC)"
	@echo "OBJ: $(OBJ)"
	@echo "DEP: $(DEP)"

.PHONY: all clean debug