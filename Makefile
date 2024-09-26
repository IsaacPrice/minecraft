CC=g++
CFLAGS=-Iinclude
LDFLAGS=-Llib -lglfw3 -lgdi32 -lopengl32
FILES=src/main.cpp src/glad.c src/Fbo.cpp src/World.cpp src/Chunk.cpp

.PHONY: app clean

app: $(FILES)
	$(CC) $(CFLAGS) -o app $(FILES) $(LDFLAGS)

clean:
	rm -f app *.o
