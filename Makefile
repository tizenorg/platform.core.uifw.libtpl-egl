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

CFLAGS += `pkg-config --cflags libdrm libtbm dlog`
LDFLAGS += `pkg-config --libs libdrm libtbm dlog`

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
	CFLAGS += `pkg-config --cflags gbm`
	LDFLAGS += `pkg-config --libs gbm`
ifneq ($(call is-feature-enabled,wl_tbm),)
	LDFLAGS += `pkg-config --libs wayland-tbm-client wayland-tbm-server`
	CFLAGS += -DTPL_USING_WAYLAND_TBM
else
	LDFLAGS += `pkg-config --libs wayland-drm`
endif
endif
ifneq ($(call is-feature-enabled,ttrace),)
	CFLAGS += -DTTRACE_ENABLE
endif
ifneq ($(call is-feature-enabled,dlog),)
	CFLAGS += -DDLOG_DEFAULT_ENABLE
endif
ifneq ($(call is-feature-enabled,egl_bind_wl_display),)
	CFLAGS += -DEGL_BIND_WL_DISPLAY
endif
ifneq ($(call is-feature-enabled,pngdump),)
	CFLAGS += -DPNG_DUMP_ENABLE
	LDFLAGS += `pkg-config libpng`
endif

ifneq ($(call is-feature-enabled,arm_atomic_operation),)
	CFLAGS += -DARM_ATOMIC_OPERATION
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
TPL_SRCS += $(SRC_DIR)/tpl_utils_hlist.c

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
