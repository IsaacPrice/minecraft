CC=g++
INCLUDES=-Iinclude
OPTFLAGS=-O2
WARNINGS=-Wall -Wextra

# Project sources are built with warnings enabled. The vendored sources below
# are third party and are built without them, so the build stays quiet.
PROJECT_SRCS=src/main.cpp src/Chunk.cpp src/Controls.cpp src/Object.cpp src/Shader.cpp src/World.cpp
VENDOR_CXX_SRCS=src/FastNoise.cpp
VENDOR_C_SRCS=src/glad.c

PROJECT_OBJS=$(PROJECT_SRCS:.cpp=.o)
VENDOR_OBJS=$(VENDOR_CXX_SRCS:.cpp=.o) $(VENDOR_C_SRCS:.c=.o)
OBJS=$(PROJECT_OBJS) $(VENDOR_OBJS)

# Platform detection replaces the old MakefileCopyPaste/ directory, which
# required pasting the right link flags in by hand before every build.
ifeq ($(OS),Windows_NT)
	# lib/ ships the prebuilt GLFW static library for MinGW.
	LDFLAGS=-Llib -lglfw3 -lgdi32 -lopengl32
	TARGET=app.exe
else
	UNAME_S := $(shell uname -s)
	TARGET=app
	ifeq ($(UNAME_S),Darwin)
		# Expects GLFW from Homebrew: brew install glfw
		LDFLAGS=-lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	else
		# Expects system GLFW: apt install libglfw3-dev
		LDFLAGS=-lglfw -lGL -ldl -lpthread
	endif
endif

.PHONY: app clean

app: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(PROJECT_OBJS): %.o: %.cpp
	$(CC) $(INCLUDES) $(OPTFLAGS) $(WARNINGS) -c $< -o $@

$(VENDOR_CXX_SRCS:.cpp=.o): %.o: %.cpp
	$(CC) $(INCLUDES) $(OPTFLAGS) -c $< -o $@

$(VENDOR_C_SRCS:.c=.o): %.o: %.c
	$(CC) $(INCLUDES) $(OPTFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) app app.exe
