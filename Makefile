CC=g++
CFLAGS=-Iinclude
LDFLAGS=-Llib -lglfw3 -lgdi32 -lopengl32

.PHONY: app clean

app: src/main.cpp src/glad.c 
	$(CC) $(CFLAGS) -o app src/main.cpp src/glad.c $(LDFLAGS)

clean:
	rm -f app
