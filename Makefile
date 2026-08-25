CC=g++
INCLUDES=-Iinclude
# -O2, not -O3. Measured with bench/: -O3 costs about 10% on GenerateChunk and
# MakeVertexObject, and neither -flto nor -march=native recovers it. The extra
# inlining -O3 does to the noise sampling seems to hurt more than it helps, so
# the flag that is actually faster is the one that is here.
OPTFLAGS=-O2
WARNINGS=-Wall -Wextra

# -MMD -MP makes the compiler write a .d file listing the headers each object
# depends on, which the include at the bottom feeds back to make. Without it a
# header change rebuilt nothing: editing CHUNK_HEIGHT in BlockMap.hpp left every
# already-built object on the old value and linked a binary whose translation
# units disagreed about the size of a chunk.
DEPFLAGS=-MMD -MP

# Project sources are built with warnings enabled. The vendored sources below
# are third party and are built without them, so the build stays quiet.
PROJECT_SRCS=src/main.cpp src/Chunk.cpp src/Controls.cpp src/DebugOverlay.cpp src/Display.cpp src/Features.cpp src/Input.cpp src/Object.cpp src/PostProcess.cpp src/Screens.cpp src/Settings.cpp src/Shader.cpp src/TerrainGen.cpp src/Texture.cpp src/UIRenderer.cpp src/Widgets.cpp src/World.cpp
VENDOR_CXX_SRCS=src/FastNoise.cpp
VENDOR_C_SRCS=src/glad.c

# bench/ times generation and meshing without opening a window, so a change to
# either can be measured on its own instead of being read off a frame rate.
BENCH_SRCS=bench/bench.cpp bench/gl_stub.cpp
BENCH_OBJS=$(BENCH_SRCS:.cpp=.o)
BENCH_DEPS=src/Chunk.o src/Features.o src/TerrainGen.o src/FastNoise.o

PROJECT_OBJS=$(PROJECT_SRCS:.cpp=.o)
VENDOR_OBJS=$(VENDOR_CXX_SRCS:.cpp=.o) $(VENDOR_C_SRCS:.c=.o)
OBJS=$(PROJECT_OBJS) $(VENDOR_OBJS)
DEPS=$(OBJS:.o=.d) $(BENCH_OBJS:.o=.d)

# Platform detection replaces the old MakefileCopyPaste/ directory, which
# required pasting the right link flags in by hand before every build.
ifeq ($(OS),Windows_NT)
	# lib/ ships the prebuilt GLFW static library for MinGW.
	LDFLAGS=-Llib -lglfw3 -lgdi32 -lopengl32
	TARGET=app.exe
	BENCH_TARGET=benchmark.exe
else
	UNAME_S := $(shell uname -s)
	TARGET=app
	BENCH_TARGET=benchmark
	ifeq ($(UNAME_S),Darwin)
		# Expects GLFW from Homebrew: brew install glfw
		LDFLAGS=-lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	else
		# Expects system GLFW: apt install libglfw3-dev
		LDFLAGS=-lglfw -lGL -ldl -lpthread
	endif
endif

.PHONY: app bench clean

app: $(TARGET)

bench: $(BENCH_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OPTFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BENCH_TARGET): $(BENCH_OBJS) $(BENCH_DEPS)
	$(CC) $(OPTFLAGS) -o $@ $(BENCH_OBJS) $(BENCH_DEPS)

$(BENCH_OBJS): %.o: %.cpp
	$(CC) $(INCLUDES) $(OPTFLAGS) $(WARNINGS) $(DEPFLAGS) -c $< -o $@

$(PROJECT_OBJS): %.o: %.cpp
	$(CC) $(INCLUDES) $(OPTFLAGS) $(WARNINGS) $(DEPFLAGS) -c $< -o $@

$(VENDOR_CXX_SRCS:.cpp=.o): %.o: %.cpp
	$(CC) $(INCLUDES) $(OPTFLAGS) $(DEPFLAGS) -c $< -o $@

$(VENDOR_C_SRCS:.c=.o): %.o: %.c
	$(CC) $(INCLUDES) $(OPTFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BENCH_OBJS) $(DEPS) app app.exe benchmark benchmark.exe

-include $(DEPS)
