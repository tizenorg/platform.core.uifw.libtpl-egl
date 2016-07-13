#######################################################
##### Makefile to build tpl-test using libgtest.a #####
#######################################################

# Following variables will be exported from spec file
#
# GTEST_DIR
# GTEST_INCLUDE
# GTEST_FLAGS

GTEST_LIB = gtest/libgtest.a

LD_FLAGS = -lm -lrt -lpthread -ltpl-egl -lwayland-client -lwayland-egl

TEST_SRCS += src/main.cpp
TEST_SRCS += src/tpl-test.cpp
TEST_SRCS += src/tpl-test-wayland.cpp
TEST_SRCS += src/tpl-test-display.cpp
TEST_SRCS += src/tpl-test-surface.cpp
TEST_SRCS += src/tpl-test-object.cpp
TEST_HEADERS += src/tpl-test.h 
TEST_HEADERS += src/tpl-test-wayland.h 

BIN_NAME = tpl-test

OBJS      = $(TEST_SRCS:%.cpp=%.o) \
		    $(NULL)


all : $(BIN_NAME)

clean :
	rm -f $(BIN_NAME)

%.o: %.cpp
	$(CXX) $(LD_FLAGS) -c -o $@ $< $(GTEST_FLAGS) $(GTEST_INCLUDE) 

$(BIN_NAME) : $(OBJS) $(GTEST_LIB)
	$(CXX) -lpthread $^ -o $@ $(GTEST_FLAGS) $(CFLAGS) $(LD_FLAGS)

