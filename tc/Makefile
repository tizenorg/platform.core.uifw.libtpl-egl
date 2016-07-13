#######################################################
##### Makefile to build tpl-test using libgtest.a #####
#######################################################

#GTEST_DIR +=
#GTEST_INCLUDE +=
#GTEST_FLAGS +=

GTEST_LIB = gtest/libgtest.a

LD_FLAGS = -lm -lrt -lpthread -ltpl-egl -lwayland-client -lwayland-egl

TEST_SRCS += src/main.cc
TEST_SRCS += src/tpl-test.cc
TEST_SRCS += src/tpl-test-display.cc
TEST_SRCS += src/tpl-test-surface.cc
TEST_SRCS += src/tpl-test-object.cc
TEST_HEADERS += src/tpl-test.h 

BIN_NAME = tpl-test

OBJS      = $(TEST_SRCS:%.cc=%.o) \
		    $(NULL)


all : $(BIN_NAME)

clean :
	rm -f $(BIN_NAME)

%.o: %.cc
	$(CXX) -c -o $@ $< $(GTEST_FLAGS) $(GTEST_INCLUDE) $(LD_FLAGS)

#$(TEST).o : $(TEST_SRCS) $(TEST_HEADERS)
#	$(CXX) -c $(TEST_SRCS) -o $@ $(GTEST_INCLUDE) $(GTEST_FLAGS) $(CFLAGS) $(LD_FLAGS) 

#$(TEST) : $(TEST).o $(GTEST_LIB)
#	$(CXX) -lpthread $^ -o $@ $(GTEST_FLAGS) $(CFLAGS) $(LD_FLAGS)

$(BIN_NAME) : $(OBJS) $(GTEST_LIB)
	$(CXX) -lpthread $^ -o $@ $(GTEST_FLAGS) $(CFLAGS) $(LD_FLAGS)
