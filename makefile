
CXX = g++
CXXFLAGS = -std=c++20 -g -I"header files" -Ithird_party -MMD
LDFLAGS = -lws2_32

SRC = $(wildcard implementation/*.cpp)

BUILD_DIR = build

OBJ = $(patsubst implementation/%.cpp,$(BUILD_DIR)/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

SERVER = server.exe

all: $(SERVER)

$(SERVER): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: implementation/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# Include dependency files
-include $(DEP)

clean:
	del /Q $(BUILD_DIR)\*.o $(BUILD_DIR)\*.d $(SERVER) 2>nul || true
