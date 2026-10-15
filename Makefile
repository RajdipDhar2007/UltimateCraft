# UltimateCraft - GNU make build (Linux, macOS, or MSYS2/MinGW on Windows).
# Usage: make -j8        then run ./ultimatecraft from this folder.
# Extra include dirs can be passed with:  make EXTRA_CFLAGS=-I/some/dir
CC ?= gcc
UNAME_S := $(shell uname -s 2>/dev/null)
OUT := ultimatecraft
BUILD := build

CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
CFLAGS += -Ilib/glad/include -Ilib/glfw/include -Ilib/stb -Ilib/noise $(EXTRA_CFLAGS)
GLFW_CFLAGS := -O2 -w -Ilib/glfw/include $(EXTRA_CFLAGS)

GLFW_COMMON := context init input monitor vulkan window egl_context osmesa_context
ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
  GLFW_PLAT := win32_init win32_joystick win32_monitor win32_time win32_thread win32_window wgl_context
  GLFW_CFLAGS += -D_GLFW_WIN32
  LDLIBS := -lgdi32 -lwinmm -lpthread -static -static-libgcc -mwindows   # fully static GUI app: no console, no MSYS2 DLLs
  # make CONSOLE=1 keeps a console window (logs on screen)
  ifeq ($(CONSOLE),1)
    LDLIBS := -lgdi32 -lwinmm -lpthread -static -static-libgcc
    CFLAGS += -DUC_CONSOLE
  endif
  OUT := ultimatecraft.exe
else ifeq ($(UNAME_S),Darwin)
  GLFW_PLAT := cocoa_init cocoa_joystick cocoa_monitor cocoa_window cocoa_time posix_thread nsgl_context
  GLFW_CFLAGS += -D_GLFW_COCOA
  LDLIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lpthread
else
  GLFW_PLAT := x11_init x11_monitor x11_window xkb_unicode posix_time posix_thread glx_context linux_joystick
  GLFW_CFLAGS += -D_GLFW_X11 -D_DEFAULT_SOURCE
  LDLIBS := -ldl -lpthread -lm
endif

SRC := $(wildcard src/*.c src/*/*.c)
OBJ := $(SRC:%.c=$(BUILD)/%.o)
LIB_OBJ := $(BUILD)/lib/glad.o $(BUILD)/lib/noise1234.o $(BUILD)/lib/stb_vorbis.o
GLFW_OBJ := $(patsubst %,$(BUILD)/lib/glfw/%.o,$(GLFW_COMMON) $(GLFW_PLAT))

all: $(OUT)

$(OUT): $(OBJ) $(LIB_OBJ) $(GLFW_OBJ)
	$(CC) -o $@ $^ $(LDLIBS) -lm

$(BUILD)/src/%.o: src/%.c $(wildcard src/*/*.h src/*/*.inc)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/lib/glad.o: lib/glad/src/glad.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -w -Ilib/glad/include -c $< -o $@
$(BUILD)/lib/stb_vorbis.o: lib/stb/stb_vorbis.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -w -c $< -o $@
$(BUILD)/lib/noise1234.o: lib/noise/noise1234.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -w -c $< -o $@
$(BUILD)/lib/glfw/%.o: lib/glfw/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(GLFW_CFLAGS) -c $< -o $@
$(BUILD)/lib/glfw/%.o: lib/glfw/src/%.m
	@mkdir -p $(dir $@)
	$(CC) $(GLFW_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(OUT)
.PHONY: all clean
