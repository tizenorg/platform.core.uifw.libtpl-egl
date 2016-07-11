########################################
##### Makefile to build libgtest.a #####
########################################

#GTEST_DIR +=
#GTEST_INCLUDE +=
#GTEST_FLAGS +=

GTEST_LIB = libgtest.a

all : $(GTEST_LIB)

clean :
	rm -f $(GTEST_LIB) gtest-all.o

gtest-all.o :
	$(CXX) $(GTEST_INCLUDE) $(GTEST_FLAGS) \
		-c $(GTEST_DIR)/src/gtest-all.cc

libgtest.a : gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

