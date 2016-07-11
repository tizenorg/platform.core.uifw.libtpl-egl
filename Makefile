#######################################################
##### Makefile to build tpl-test using libgtest.a #####
#######################################################

#GTEST_DIR +=
#GTEST_INCLUDE +=
#GTEST_FLAGS +=

GTEST_LIB = gtest/libgtest.a

LD_FLAGS = -lm -lrt -lpthread -ltpl-egl -lwayland-client -lwayland-egl

TEST_SRCS = src/my_test.cc
TEST_HEADERS = 

TEST = tpl-test

all : $(TEST)

clean :
	rm -f $(TEST)

$(TEST).o : $(TEST_SRCS) $(TEST_HEADERS)
	$(CXX) -c $(TEST_SRCS) -o $@ $(GTEST_INCLUDE) $(GTEST_FLAGS) $(CFLAGS) $(LD_FLAGS) 

$(TEST) : $(TEST).o $(GTEST_LIB)
	$(CXX) -lpthread $^ -o $@ $(GTEST_FLAGS) $(CFLAGS) $(LD_FLAGS)

