# Flux Language - Build Configuration
# ============================================================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
RELEASE_FLAGS = -O2
DEBUG_FLAGS = -g -O0 -DDEBUG

SRC_DIR = src
STD_DIR = standard
BUILD_DIR = build
BIN = flux

# Core interpreter sources
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/lexer.cpp \
          $(SRC_DIR)/parser.cpp \
          $(SRC_DIR)/interpreter.cpp \
          $(SRC_DIR)/transpiler.cpp

# Standard library sources
STD_SOURCES = $(STD_DIR)/std_io.cpp \
              $(STD_DIR)/std_net.cpp \
              $(STD_DIR)/std_collections.cpp \
              $(STD_DIR)/std_sys.cpp \
              $(STD_DIR)/std_json.cpp \
              $(STD_DIR)/std_time.cpp \
              $(STD_DIR)/std_crypto.cpp \
              $(STD_DIR)/std_os.cpp \
              $(STD_DIR)/std_regex.cpp \
              $(STD_DIR)/std_gpu.cpp \
              $(STD_DIR)/std_graphics.cpp \
              $(STD_DIR)/std_audio.cpp \
              $(STD_DIR)/std_video.cpp

ALL_SOURCES = $(SOURCES) $(STD_SOURCES)

HEADERS = $(SRC_DIR)/token.h \
          $(SRC_DIR)/lexer.h \
          $(SRC_DIR)/parser.h \
          $(SRC_DIR)/ast.h \
          $(SRC_DIR)/value.h \
          $(SRC_DIR)/environment.h \
          $(SRC_DIR)/interpreter.h

STD_HEADERS = $(STD_DIR)/std_io.h \
              $(STD_DIR)/std_net.h \
              $(STD_DIR)/std_collections.h \
              $(STD_DIR)/std_sys.h \
              $(STD_DIR)/std_json.h \
              $(STD_DIR)/std_time.h \
              $(STD_DIR)/std_crypto.h \
              $(STD_DIR)/std_os.h \
              $(STD_DIR)/std_regex.h \
              $(STD_DIR)/std_gpu.h \
              $(STD_DIR)/std_graphics.h \
              $(STD_DIR)/std_audio.h \
              $(STD_DIR)/std_video.h

ALL_HEADERS = $(HEADERS) $(STD_HEADERS)

# --------------------------------------------------------------------------
# Optional dependency detection
# --------------------------------------------------------------------------
# libcurl (for std.net HTTP support)
CURL_CHECK := $(shell pkg-config --exists libcurl 2>/dev/null && echo yes)
ifeq ($(CURL_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_CURL $(shell pkg-config --cflags libcurl)
    LDFLAGS  += $(shell pkg-config --libs libcurl)
endif

# SDL2 (for std.graphics window backend)
SDL2_CHECK := $(shell pkg-config --exists sdl2 2>/dev/null && echo yes)
ifeq ($(SDL2_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_SDL2 $(shell pkg-config --cflags sdl2)
    LDFLAGS  += $(shell pkg-config --libs sdl2)
endif

# SDL2_ttf (for text rendering)
SDL2_TTF_CHECK := $(shell pkg-config --exists SDL2_ttf 2>/dev/null && echo yes)
ifeq ($(SDL2_TTF_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_SDL2_TTF $(shell pkg-config --cflags SDL2_ttf)
    LDFLAGS  += $(shell pkg-config --libs SDL2_ttf)
endif

# SDL2_image (for image loading)
SDL2_IMG_CHECK := $(shell pkg-config --exists SDL2_image 2>/dev/null && echo yes)
ifeq ($(SDL2_IMG_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_SDL2_IMAGE $(shell pkg-config --cflags SDL2_image)
    LDFLAGS  += $(shell pkg-config --libs SDL2_image)
endif

# SDL2_mixer (for audio playback)
SDL2_MIX_CHECK := $(shell pkg-config --exists SDL2_mixer 2>/dev/null && echo yes)
ifeq ($(SDL2_MIX_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_SDL2_MIXER $(shell pkg-config --cflags SDL2_mixer)
    LDFLAGS  += $(shell pkg-config --libs SDL2_mixer)
endif

# GLFW3 (3D graphics backend, used alongside SDL2 for OpenGL 3D rendering)
GLFW_CHECK := $(shell pkg-config --exists glfw3 2>/dev/null && echo yes)
ifeq ($(GLFW_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_GLFW $(shell pkg-config --cflags glfw3)
    LDFLAGS  += $(shell pkg-config --libs glfw3) -lGL -lGLU
endif

# FFmpeg (for std.video playback — libavcodec, libavformat, libswscale, libswresample)
FFMPEG_CHECK := $(shell pkg-config --exists libavcodec libavformat libswscale libswresample 2>/dev/null && echo yes)
ifeq ($(FFMPEG_CHECK),yes)
    CXXFLAGS += -DFLUX_HAS_FFMPEG $(shell pkg-config --cflags libavcodec libavformat libswscale libswresample libavutil)
    LDFLAGS  += $(shell pkg-config --libs libavcodec libavformat libswscale libswresample libavutil)
endif

# Threading support (always needed for std.sys)
LDFLAGS += -lpthread

# --------------------------------------------------------------------------
# Build targets
# --------------------------------------------------------------------------

# Default target: release build
all: release

release: $(ALL_SOURCES) $(ALL_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(ALL_SOURCES) -o $(BUILD_DIR)/$(BIN) $(LDFLAGS)
	@echo "Build complete: $(BUILD_DIR)/$(BIN)"

debug: $(ALL_SOURCES) $(ALL_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(ALL_SOURCES) -o $(BUILD_DIR)/$(BIN)_debug $(LDFLAGS)
	@echo "Debug build complete: $(BUILD_DIR)/$(BIN)_debug"

clean:
	rm -rf $(BUILD_DIR)

# Run a flux file: make run FILE=hello.flux
run: release
	./$(BUILD_DIR)/$(BIN) $(FILE)

# Start the REPL
repl: release
	./$(BUILD_DIR)/$(BIN)

# Run all tests
test: release
	@bash test_suite/run_all.sh

# Show detected optional dependencies
info:
	@echo "=== Flux Build Configuration ==="
	@echo "Compiler: $(CXX)"
	@echo "libcurl (HTTP):  $(if $(CURL_CHECK),YES,NO)"
	@echo "SDL2 (Graphics): $(if $(SDL2_CHECK),YES,NO)"
	@echo "SDL2_ttf (Text): $(if $(SDL2_TTF_CHECK),YES,NO)"
	@echo "SDL2_image:      $(if $(SDL2_IMG_CHECK),YES,NO)"
	@echo "GLFW (Graphics): $(if $(GLFW_CHECK),YES,NO)"
	@echo "================================="

.PHONY: all release debug clean run repl test info
