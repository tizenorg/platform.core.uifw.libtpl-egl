SRC_DIR = .
BIN_NAME = libgbm_tbm.so
INST_LIBDIR = $(libdir)

CC ?= gcc

CFLAGS += -Wall -fPIC -I$(SRC_DIR)
LDFLAGS += 

CFLAGS += `pkg-config --cflags libtbm` -I/usr/include/libdrm
LDFLAGS += `pkg-config --libs libtbm` -lwayland-drm

HEADERS = \
	$(SRC_DIR)/gbm_tbm.h \
	$(SRC_DIR)/gbm_tbmint.h

SRCS = \
	$(SRC_DIR)/gbm_tbm.c

OBJS = $(SRCS:%.c=%.o)

################################################################################
all: $(BIN_NAME)

$(BIN_NAME): $(OBJS) $(HEADERS)
	$(CC) -o $@ $(OBJS) -shared $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	find . -name "*.o" -exec rm -vf {} \;
	find . -name "*~" -exec rm -vf {} \;
	rm -vf $(BIN_NAME)

install: all
	cp -va $(BIN_NAME) $(INST_LIBDIR)/gbm/

uninstall:
	rm -f $(INST_LIBDIR)/gbm/$(BIN_NAME)
