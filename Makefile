CC=g++
CFLAGS=-Iinclude
LDFLAGS=-Llib -lglfw -lGL -ldl

.PHONY: app clean

app: src/main.cpp src/glad.c
	$(CC) $(CFLAGS) -o app src/main.cpp src/glad.c $(LDFLAGS)

clean:
	rm -f app
