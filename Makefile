OBJS += lib/cimgui/imgui/backends/imgui_impl_sdl3.o \
		lib/cimgui/imgui/backends/imgui_impl_opengl3.o
LIBS = lib/cimgui/libcimgui.a
CXXFLAGS = -shared -fPIC -fno-exceptions -fno-rtti -Ilib/cimgui/imgui -DIMGUI_IMPL_API="extern \"C\""

CFLAGS = -fPIC -Ilib/cimgui
LDFLAGS = -Llib/cimgui -lcimgui -lSDL3 -lGL -lGLEW -lm -lstdc++

all: $(OBJS) $(LIBS)
	$(CC)  main.c $(OBJS) $(CFLAGS) $(LDFLAGS) 

lib/cimgui/libcimgui.a:
	$(MAKE) static -Clib/cimgui

lib/cimgui/imgui/backends/%.o: lib/cimgui/imgui/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f a.out

clean-all: clean
	rm -f $(LIBS) $(OBJS)
	$(MAKE) -Clib/cimgui clean
