RAYLIB_DIR = lib/raylib/src
RAYLIB_LIB = $(RAYLIB_DIR)/libraylib.a
CFLAGS += -I$(RAYLIB_DIR) -L$(RAYLIB_DIR) -lraylib -lm

all: $(RAYLIB_LIB)
	$(CC) main.c $(CFLAGS)

$(RAYLIB_LIB):
	GLFW_LINUX_ENABLE_WAYLAND=TRUE $(MAKE) -C $(RAYLIB_DIR)
