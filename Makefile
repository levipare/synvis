OBJS += lib/cimgui/cimgui.so
OBJS += lib/cimgui/imgui/backends/imgui_impl_sdl3.o
OBJS += lib/cimgui/imgui/backends/imgui_impl_opengl3.o
CXXFLAGS = -shared -fPIC -fno-exceptions -fno-rtti -Ilib/cimgui/imgui -DIMGUI_IMPL_API="extern \"C\""

CFLAGS = -fPIC -Ilib/cimgui
LDFLAGS = -lSDL3 -lGL -lGLEW -lm -lstdc++

all: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) main.c $(OBJS) 

lib/cimgui/cimgui.so:
	$(MAKE) -Clib/cimgui

lib/cimgui/imgui/backends/%.o: lib/cimgui/imgui/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f a.out

clean-all: clean
	rm -f $(OBJS)
	$(MAKE) -Clib/cimgui clean
