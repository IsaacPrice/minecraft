CC=g++
CFLAGS=-Iinclude
LDFLAGS=-Llib -lglfw3 -lgdi32 -lopengl32
FILES=src/main.cpp src/glad.c src/Fbo.cpp

.PHONY: app clean

app: src/main.cpp src/glad.c
	$(CC) $(CFLAGS) -o app $(FILES) $(LDFLAGS)

clean:
	rm -f app
