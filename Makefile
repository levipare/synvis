RAYLIB_DIR = lib/raylib/src
RAYLIB_LIB = $(RAYLIB_DIR)/libraylib.a
CFLAGS += -I$(RAYLIB_DIR) -L$(RAYLIB_DIR)
LDFLAGS += -lSDL3 -lraylib -lm

all: $(RAYLIB_LIB)
	$(CC) main.c $(CFLAGS) $(LDFLAGS)

$(RAYLIB_LIB):
	$(MAKE) -C$(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP_SDL SDL_INCLUDE_PATH=/usr/include/SDL3

clean:
	$(MAKE) clean -C$(RAYLIB_DIR)
