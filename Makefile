#$(call is-feature-enabled,featurename)
#returns non-empty string if enabled, empty if not
define is-feature-enabled
$(findstring -$1-,-$(VARIANT)-)
endef

SRC_DIR = ./src
SO_NAME = libtpl.so.$(TPL_VER_MAJOR)
BIN_NAME = $(SO_NAME).$(TPL_VER_MINOR)
INST_DIR = $(libdir)
SO_VER = $(TPL_VER_MAJOR)

CC ?= gcc

CFLAGS += -Wall -fPIC -I$(SRC_DIR)
LDFLAGS += 

CFLAGS += $(pkg-config --cflags gles20 libdrm libtbm)
LDFLAGS += $(pkg-config --libs gles20 libdrm libtbm)

ifneq ($(call is-feature-enabled,winsys_dri2),)
	CFLAGS += -DWINSYS_DRI2
endif
ifneq ($(call is-feature-enabled,winsys_dri3),)
	CFLAGS += -DWINSYS_DRI3
endif
ifneq ($(call is-feature-enabled,winsys_wl),)
	CFLAGS += -DWINSYS_WL
endif
ifneq ($(call is-feature-enabled,ttrace),)
	CFLAGS += -DTTRACE_ENABLE
endif

HEADERS = \
	$(SRC_DIR)/tpl.h

SRCS = \
	$(SRC_DIR)/tpl.c

OBJS = $(SRCS:%.c=%.o)

################################################################################
all: $(BIN_NAME)

$(BIN_NAME) : $(OBJS) $(HEADERS)
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
