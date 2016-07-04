##### TPL Test for wayland #####

SRC_DIR = ./src
BIN_NAME = tpl-test
INST_DIR = /usr/bin
HOST_NAME = `uname -n`

CC ?= gcc

CFLAGS =
LDFLAGS = 

CFLAGS += -g
CFLAGS += -Wall -fPIC -I$(SRC_DIR)
CFLAGS += -DBLD_HOST_NAME=\"$(HOST_NAME)\"

REQ_PKGS = 
CFLAGS += `pkg-config --cflags $(REQ_PKGS)`
LDFLAGS += `pkg-config --libs $(REQ_PKGS)`

LDFLAGS += -lm -lrt -lpthread -ltpl-egl -lwayland-client -lwayland-egl


TC_HEADERS += $(SRC_DIR)/tpl_test_util.h

TC_SRCS += $(SRC_DIR)/main.c
TC_SRCS += $(SRC_DIR)/tpl_test_display.c
TC_SRCS += $(SRC_DIR)/tpl_test_surface.c
TC_SRCS += $(SRC_DIR)/tpl_test_object.c

OBJS = $(TC_SRCS:%.c=%.o)

################################################################################
all: $(BIN_NAME)

$(BIN_NAME) : $(OBJS) $(TC_HEADERS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

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
