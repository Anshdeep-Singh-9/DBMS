CXX ?= g++

# path #
SRC_PATH = src
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin

# executable #
BIN_NAME = miniDB
API_BIN_NAME = server

# extensions #
SRC_EXT = cpp

# code lists #
# Find all source files in the source directory, sorted by
# most recently modified
SOURCES = $(shell find $(SRC_PATH) -name '*.$(SRC_EXT)' | sort -k 1nr | cut -f2-)
# Set the object file names, with the source directory stripped
# from the path, and the build path prepended in its place
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
# Set the dependency files that will be used to add header dependencies
DEPS = $(OBJECTS:.o=.d)

# flags #
COMPILE_FLAGS = -std=c++17 -Wall -Wextra -g
INCLUDES = -I include/ -I /usr/local/include
# Space-separated pkg-config libraries used by this project
LIBS =

.PHONY: default_target
default_target: release

.PHONY: release
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
release: dirs
	@$(MAKE) all

.PHONY: api
api: release setup_api
	@echo "Building API Server: $(API_BIN_NAME)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) api/server.cpp $(filter-out build/main.o, $(OBJECTS)) -lpthread -o $(BIN_PATH)/$(API_BIN_NAME)
	@$(RM) $(API_BIN_NAME)
	@ln -s $(BIN_PATH)/$(API_BIN_NAME) $(API_BIN_NAME)

.PHONY: setup_api
setup_api:
	@if [ ! -f include/crow_all.h ]; then \
		echo "Downloading Crow library..."; \
		curl -L https://github.com/CrowCpp/Crow/releases/download/v1.2.0/crow_all.h -o include/crow_all.h; \
	fi
	@if [ ! -d include/asio ]; then \
		echo "Downloading Asio library..."; \
		curl -L https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-0.tar.gz -o asio.tar.gz; \
		tar -xzf asio.tar.gz; \
		cp -r asio-asio-1-28-0/asio/include/* include/; \
		rm -rf asio-asio-1-28-0 asio.tar.gz; \
	fi

.PHONY: dirs
dirs:
	@echo "Creating directories"
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(BIN_PATH)

.PHONY: clean
clean:
	@echo "Deleting $(BIN_NAME) symlink"
	@$(RM) $(BIN_NAME)
	@echo "Deleting directories"
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(BIN_PATH)

# checks the executable and symlinks to the output
.PHONY: all
all: $(BIN_PATH)/$(BIN_NAME)
	@echo "Making symlink: $(BIN_NAME) -> $<"
	@$(RM) $(BIN_NAME)
	@ln -s $(BIN_PATH)/$(BIN_NAME) $(BIN_NAME)

# Creation of the executable
$(BIN_PATH)/$(BIN_NAME): $(OBJECTS)
	@echo "Linking: $@"
	$(CXX) $(OBJECTS) -o $@

# Add dependency files, if they exist
-include $(DEPS)

# Source file rules
# After the first compilation they will be joined with the rules from the
# dependency files to provide header dependencies
$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	@echo "Compiling: $< -> $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@
