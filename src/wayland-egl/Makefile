WLD_EGL_SO_VER ?= no_ver
SRC_DIR = ./

#CROSS_COMPILE ?= arm-none-linux-gnueabi-
TARGET_CC ?= $(CROSS_COMPILE)gcc
TARGET_AR ?= $(CROSS_COMPILE)ar
CFLAGS += -Wall -fno-strict-aliasing -Wno-strict-aliasing -Wno-long-long -O3 -fPIC

#wayland-egl sources
WAYLAND_EGL_SRCS +=  \
			 $(SRC_DIR)/wayland-egl.c

%.o: %.c
	$(TARGET_CC) -c -o $@ $< $(CFLAGS)

WAYLAND_EGL_OBJS := $(WAYLAND_EGL_SRCS:.c=.o)

libwayland-egl.so.$(WLD_EGL_SO_VER): $(WAYLAND_EGL_OBJS)
	$(TARGET_CC) -shared -o $@ $(WAYLAND_EGL_OBJS) $(CFLAGS)

.DEFAULT_GOAL = all
all: libwayland-egl.so.$(WLD_EGL_SO_VER)

clean:
	-rm -f $(WAYLAND_EGL_OBJS) libwayland-egl.so.$(WLD_EGL_SO_VER)

install: all
	cp  ../../pkgconfig/wayland-egl.pc $(libdir)/pkgconfig/
	cp  libwayland-egl.so.$(WLD_EGL_SO_VER) $(libdir)/

uninstall:
	-rm  -f $(libdir)/pkgconfig/wayland-egl.pc
	-rm  -f $(libdir)/libwayland-egl.so.$(WLD_EGL_SO_VER)
