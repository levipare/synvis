all:
	$(CC) main.c $(shell pkg-config --libs --cflags sdl3 glew) -lm
