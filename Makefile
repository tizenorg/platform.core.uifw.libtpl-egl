#$(call is-feature-enabled,featurename)
#returns non-empty string if enabled, empty if not
define is-feature-enabled
$(findstring -$1-,-$(TPL_OPTIONS)-)
endef

SRC_DIR = ./src
SO_NAME = libtpl-egl.so.$(TPL_VER_MAJOR).$(TPL_VER_MINOR)
BIN_NAME = $(SO_NAME).$(TPL_RELEASE)
INST_DIR = $(libdir)

CC ?= gcc

CFLAGS += -Wall -fPIC -I$(SRC_DIR)
LDFLAGS +=

CFLAGS += `pkg-config --cflags libdrm libtbm`
LDFLAGS += `pkg-config --libs libdrm libtbm`

ifneq ($(call is-feature-enabled,winsys_dri2),)
	CFLAGS += -DTPL_WINSYS_DRI2
	LDFLAGS += `pkg-config --libs libdri2 xext xfixes x11 x11-xcb xcb xcb-dri3 xcb-sync xcb-present xshmfence`
endif
ifneq ($(call is-feature-enabled,winsys_dri3),)
	CFLAGS += -DTPL_WINSYS_DRI3
	LDFLAGS += `pkg-config --libs libdri2 xext xfixes x11 x11-xcb xcb xcb-dri3 xcb-sync xcb-present xshmfence`
endif

ifneq ($(call is-feature-enabled,winsys_wl),)
	CFLAGS += -DTPL_WINSYS_WL -DEGL_BIND_WL_DISPLAY
	CFLAGS += -I$(SRC_DIR)/wayland_module/gbm_tbm `pkg-config --cflags wayland-drm gbm`
	LDFLAGS += `pkg-config --libs wayland-drm gbm`
endif
ifneq ($(call is-feature-enabled,ttrace),)
	CFLAGS += -DTTRACE_ENABLE
endif

TPL_HEADERS += $(SRC_DIR)/tpl.h
TPL_HEADERS += $(SRC_DIR)/tpl_internal.h
TPL_HEADERS += $(SRC_DIR)/tpl_utils.h

TPL_SRCS += $(SRC_DIR)/tpl.c
TPL_SRCS += $(SRC_DIR)/tpl_buffer.c
TPL_SRCS += $(SRC_DIR)/tpl_display.c
TPL_SRCS += $(SRC_DIR)/tpl_frame.c
TPL_SRCS += $(SRC_DIR)/tpl_object.c
TPL_SRCS += $(SRC_DIR)/tpl_region.c
TPL_SRCS += $(SRC_DIR)/tpl_surface.c

ifneq ($(call is-feature-enabled,winsys_wl),)
TPL_SRCS += $(SRC_DIR)/tpl_wayland.c
endif

ifneq ($(call is-feature-enabled,winsys_dri2),)
TPL_HEADERS += $(SRC_DIR)/tpl_x11_internal.h

TPL_SRCS += $(SRC_DIR)/tpl_x11_common.c
TPL_SRCS += $(SRC_DIR)/tpl_x11_dri2.c
endif

ifneq ($(call is-feature-enabled,winsys_dri3),)
TPL_HEADERS += $(SRC_DIR)/tpl_x11_internal.h

TPL_SRCS += $(SRC_DIR)/tpl_x11_common.c
TPL_SRCS += $(SRC_DIR)/tpl_x11_dri3.c
endif

OBJS = $(TPL_SRCS:%.c=%.o)

################################################################################
all: $(BIN_NAME)

$(BIN_NAME) : $(OBJS) $(TPL_HEADERS)
	$(CC) -o $@ $(OBJS) -shared -Wl,-soname,$(SO_NAME) $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	find . -name "*.o" -exec rm -vf {} \;
	find . -name "*~" -exec rm -vf {} \;
	rm -vf $(BIN_NAME)

install: all
	cp -va $(BIN_NAME) $(INST_DIR)/

uninstall:
	rm -f $(INST_DIR)/$(BIN_NAME)
